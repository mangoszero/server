/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2025 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include "Utilities/MathDefines.h"
#include "TransportMap.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <list>
#include <string>
#include <vector>

#include "Transports.h"
#include "Creature.h"
#include "Pet.h"
#include "Player.h"
#include "MapManager.h"
#include "DBCStores.h"
#include "MotionGenerators/MotionMaster.h"
#include "WorldPacket.h"
#include "Log.h"
#include "terrain/GoModelStore.hpp"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"

/**
 * @brief Where a unit ACTUALLY is: map, frame, pose, and whether anything can move it.
 *
 * Every pet symptom aboard looks the same from the client -- it stands there, or it is gone --
 * and every one of them is a different answer to this line. The frame matters as much as the
 * map: a pet whose placement still says `deck` while it sits on a world map is infinitely far
 * from its master, so nothing reaches it and no rule fires.
 */
std::string DescribeSpatially(Unit* u)
{
    if (!u)
    {
        return "(null)";
    }

    Map* on = u->FindMap();
    Geometry::Frame const& f = u->Where().CurrentFrame();

    char buf[320];
    snprintf(buf, sizeof(buf),
             "%s map=%u%s frame=%s/%llu pos=(%.2f %.2f %.2f) inworld=%d alive=%d gen=%u",
             u->GetGuidStr().c_str(),
             on ? on->GetId() : 0u,
             on && on->AsTransport() ? "[deck]" : "",
             f.IsPlaced() ? (f.IsDeck() ? "deck" : "world") : "nowhere",
             static_cast<unsigned long long>(f.Id()),
             u->Where().X(), u->Where().Y(), u->Where().Z(),
             u->IsInWorld() ? 1 : 0, u->IsAlive() ? 1 : 0,
             unsigned(u->GetMotionMaster()->GetCurrentMovementGeneratorType()));

    return buf;
}

namespace
{
    /// How far above and below a point to look for a surface when placing something on it.
    constexpr float HULL_SEARCH_UP = 3.0f;
    constexpr float HULL_SEARCH_DOWN = 6.0f;

    /// Chest height for the obstruction probe, so the plating a unit stands ON is not itself
    /// read as the thing blocking it.
    constexpr float HULL_PROBE_HEIGHT = 1.0f;

    /// Bearings tried when sweeping for a free spot around a master.
    constexpr int HULL_SPOT_BEARINGS = 8;

    /**
     * @brief A totem rides the ship it was planted on for as long as it lives, whoever its
     *        master is and wherever they have got to.
     *
     * A shaman who drops a totem on the pier and then sails has left it on the pier, which is
     * exactly right, and one dropped on the forecastle sails with the ship. Death or despawn
     * is what takes it off the boat.
     */
    bool IsPlanted(Unit const* minion)
    {
        return minion->GetTypeId() == TYPEID_UNIT &&
               static_cast<Creature const*>(minion)->IsTotem();
    }

    /**
     * @brief THE TWO BITS THAT KILL A CLIENT ABOARD A MOVING SHIP.
     *
     * `CreatureTypeFlags & (0x100000 | 0x400000)`, and it takes BOTH -- either one alone is
     * harmless, proven by putting each on a deck on its own and sailing. Together, every
     * client that can see the creature dies in its render path the moment the ship gets
     * under way. Stationary, nothing happens: the client only walks a transport's
     * attachments while it is moving.
     *
     * These are `UNK21` and `UNK23` in SharedDefines, and our own notes on them read "may be
     * related to rendering" and "probably controls some creature visual". They reach the
     * client in SMSG_CREATURE_QUERY_RESPONSE, cached per ENTRY -- so this cannot be papered
     * over per creature on the wire, and the creature simply does not sail.
     *
     * This is why `.wp add` was lethal on a deck: the waypoint marker carried them. Nothing
     * to do with its entry, which is where this hunt spent an evening -- a copy of it under a
     * different entry killed the client just the same until the flags came off.
     *
     * Disproved on the way here, so nobody repeats it: CreatureModelData.Flags (both crashing
     * models carried 0x1|0x2, and so do 2644 ordinary templates), InhabitType (never sent,
     * and orthogonal in the data), UnitFlags, NpcFlags and DynamicFlags (cross-tabulated bit
     * by bit over 29912 templates; nothing separates the samples).
     */
    bool CanRide(Creature const* crew)
    {
        CreatureInfo const* info = crew->GetCreatureInfo();

        return !info || (info->CreatureTypeFlags & CREATURE_TYPEFLAGS_TRANSPORT_FORBIDDEN)
                        != CREATURE_TYPEFLAGS_TRANSPORT_FORBIDDEN;
    }

    /// Drop a minion from one client, both halves: the packet AND the server's record that
    /// the client holds it. Either one alone leaves the two disagreeing.
    void ForgetMinion(Creature* minion, Unit* watcher)
    {
        Player* client = watcher && watcher->GetTypeId() == TYPEID_PLAYER
                             ? static_cast<Player*>(watcher) : NULL;
        if (!client)
        {
            return;
        }

        minion->DestroyForPlayer(client);
        client->m_clientGUIDs.erase(minion->GetObjectGuid());
    }

    /**
     * @brief Move a minion to the map its master is on, beside him.
     *
     * Aboard-to-ashore and ashore-to-aboard are the same call: the destination map decides
     * every coordinate the moment the minion is added to it. There is nothing to board and no
     * frame to change, because the ship IS the frame.
     */
    void DrawMinionTo(Unit* minion, Unit* master, Map* dest)
    {
        if (!minion || !master || !dest)
        {
            DEBUG_FILTER_LOG(LOG_FILTER_DECK_MINIONS,
                             "DrawMinionTo: SKIP null (minion=%p master=%p dest=%p)",
                             (void*)minion, (void*)master, (void*)dest);
            return;
        }

        if (!minion->IsInWorld() || !minion->IsAlive())
        {
            DEBUG_FILTER_LOG(LOG_FILTER_DECK_MINIONS,
                             "DrawMinionTo: SKIP not-drawable %s", DescribeSpatially(minion).c_str());
            return;
        }

        if (minion->FindMap() == dest)
        {
            return;                         // the ordinary case, once a tick, per minion
        }

        Creature* c = static_cast<Creature*>(minion);

        DEBUG_FILTER_LOG(LOG_FILTER_DECK_MINIONS,
                         "DrawMinionTo: FROM %s", DescribeSpatially(c).c_str());
        DEBUG_FILTER_LOG(LOG_FILTER_DECK_MINIONS,
                         "DrawMinionTo:   TO map=%u%s beside %s", dest->GetId(),
                         dest->AsTransport() ? "[deck]" : "", DescribeSpatially(master).c_str());

        float x, y, z;
        ClosePointNear(*master, x, y, z, minion->Where().Extent(), PET_FOLLOW_DIST,
                       PET_FOLLOW_ANGLE);

        // Whatever leg it was on was planned in the map it is leaving; none of it survives.
        c->StopMoving();

        // TELL EVERY CLIENT TO DROP IT FIRST, and MAKE THE SERVER AGREE. Without this the
        // ones already holding it get only a heartbeat at the far side and interpolate the
        // difference -- which a player sees as his pet swimming up through the hull.
        //
        // The packet alone is not enough and that is the whole trap: DestroyForPlayer does
        // not touch m_clientGUIDs, so the server still believes the client holds it, and the
        // visibility pass at the far side takes the HaveAtClient branch and sends NO CREATE.
        // The client is left drawing its last copy -- for a pet coming off a ship, still
        // attached to the ship. Erasing the guid is what makes the arrival a real CREATE.
        //
        // And THE MASTER MUST BE IN THE LIST. He is the one client that has to re-create it,
        // and he is the one this loop cannot reach: by the time a minion is drawn ashore he
        // has already been taken off the deck map it is walking.
        ForgetMinion(c, master);

        for (Map::PlayerList::const_iterator itr = c->GetMap()->GetPlayers().begin();
             itr != c->GetMap()->GetPlayers().end(); ++itr)
        {
            ForgetMinion(c, itr->getSource());
        }

        c->GetMap()->Remove(c, false);
        c->Place().MoveTo(x, y, z, master->Where().Facing());
        dest->Add(c);

        // AND THE MOTION, in the frame it now stands in. Initialize() alone restores the
        // creature's DEFAULT generator -- for a pet that is not following anybody, which is
        // a pet teleported neatly to its master's side and then standing there.
        //
        // MoveFollow and MoveIdle each clear the stack themselves, and correctly. Clearing
        // it here first with all=true emptied it down to and including the idle generator,
        // and the Clear inside MoveFollow then asserted on !empty() -- a crash on every
        // step ashore, from MotionMaster::DirectClean.
        if (c->GetCharmInfo() && c->GetCharmInfo()->HasCommandState(COMMAND_STAY))
        {
            c->GetMotionMaster()->MoveIdle();
        }
        else
        {
            c->GetMotionMaster()->MoveFollow(master, PET_FOLLOW_DIST, PET_FOLLOW_ANGLE);
        }

        c->SendHeartBeat();

        DEBUG_FILTER_LOG(LOG_FILTER_DECK_MINIONS,
                         "DrawMinionTo: DONE %s", DescribeSpatially(c).c_str());
    }

    /// Every minion this master controls, moved to `dest` beside him. The reconciler does
    /// the same sweep once per tick; this is the immediate half, so nothing is ever seen
    /// standing where its master no longer is.
    void DrawMinionsTo(Player* master, Map* dest)
    {
        if (!master || !dest)
        {
            return;
        }

        // A ZERO HERE IS THE ANSWER, not a quiet success: the sweep found nothing to move,
        // which means the master no longer owns what is standing on the other map.
        int seen = 0;
        master->CallForAllControlledUnits(
            [master, dest, &seen](Unit* minion) { ++seen; DrawMinionTo(minion, master, dest); },
            CONTROLLED_PET | CONTROLLED_MINIPET | CONTROLLED_GUARDIANS | CONTROLLED_TOTEMS);

        DEBUG_FILTER_LOG(LOG_FILTER_DECK_MINIONS,
                         "DrawMinionsTo: %s -> map=%u%s, %d controlled unit(s), petguid=%s",
                         master->GetGuidStr().c_str(), dest->GetId(),
                         dest->AsTransport() ? "[deck]" : "", seen,
                         master->GetPetGuid().GetString().c_str());
    }
}

/* ******************************** The hull ******************************************* */

bool TransportMap::Commission()
{
    GameObjectInfo const* goinfo = m_vessel ? m_vessel->GetGOInfo() : NULL;
    if (!goinfo)
    {
        return false;
    }

    auto model = world::terrain::GoModelStore::Instance().Get(goinfo->displayId);
    if (model)
    {
        Geometry::Aabb const& b = model->Bounds();
        const float hx = std::max(std::fabs(b.lo.x), std::fabs(b.hi.x));
        const float hy = std::max(std::fabs(b.lo.y), std::fabs(b.hi.y));
        m_hullRadius = std::sqrt(hx * hx + hy * hy);

        // A SHIP IS ALWAYS LOADED. Its grids are pinned at start-up and never unloaded: no
        // player ever "enters" this map to trigger a load, and a hull whose grid had expired
        // would answer no height, no collision and no route -- silently, and only once she
        // was already at sea. The bounds are walked corner to corner because a hull straddles
        // the map centre and so spans up to four grids.
        uint32 pinned = 0;
        for (float gx = b.lo.x; gx < b.hi.x + SIZE_OF_GRIDS; gx += SIZE_OF_GRIDS)
        {
            for (float gy = b.lo.y; gy < b.hi.y + SIZE_OF_GRIDS; gy += SIZE_OF_GRIDS)
            {
                // Clamped to the hull: stepping a whole grid past it pinned cells the ship
                // does not occupy, half a kilometre off the bow.
                ForceLoadGrid(std::min(gx, b.hi.x), std::min(gy, b.hi.y));
                ++pinned;
            }
        }

        DETAIL_LOG("Transport %u map %u: %u grid(s) pinned, hull x[%.1f %.1f] y[%.1f %.1f]",
                   goinfo->id, GetId(), pinned, b.lo.x, b.hi.x, b.lo.y, b.hi.y);
    }

    // A map with no baked tile is no ship, whatever Map.dbc says. Left standing it swallows
    // whoever steps aboard: Map::Add cannot load a grid that has no terrain, so a passenger
    // is removed from the world he was in and added to nothing, and ends up at (0,0)
    // belonging nowhere.
    if (!GetTerrain()->ColumnAt(0.0f, 0.0f, m_hullRadius * 3.0f, -m_hullRadius * 3.0f)
                     .HighestSolid())
    {
        sLog.outErrorDb("Transport %u (%s, display %u): map %u has no baked terrain "
                        "(w_%u.tile missing). Re-bake the vessel maps; it will carry no crew "
                        "and board no passengers until then.",
                        goinfo->id, goinfo->name, goinfo->displayId, GetId(), GetId());
        return false;
    }

    m_commissioned = true;
    return true;
}

std::optional<float> TransportMap::SurfaceAt(float x, float y, float z,
                                             float searchUp, float searchDown) const
{
    // The window is the point: an open hatch must refuse rather than answer with the deck two
    // levels down.
    return GetTerrain()->ColumnAt(x, y, z + searchUp, z - searchDown)
           .HighestSolidAtOrBelow(z + searchUp);
}

bool TransportMap::IsBlocked(Geometry::Vector3 const& from, Geometry::Vector3 const& to) const
{
    const Geometry::Vector3 seg = to - from;
    const float len = std::sqrt(seg.x * seg.x + seg.y * seg.y + seg.z * seg.z);
    if (len < 1e-4f)
    {
        return false;
    }

    return !GetTerrain()->IsInLineOfSight(from.x, from.y, from.z, to.x, to.y, to.z);
}

std::optional<Geometry::Placement> TransportMap::PositionOf(WorldObject const& obj) const
{
    // ABOARD, ITS POSITION IS ITS POSITION. Nothing to look up and nothing to convert: this
    // map's coordinates are the answer for a crew member, a pet, a totem and a player alike.
    if (obj.GetMap() == this)
    {
        return obj.Where();
    }

    return std::nullopt;
}

std::optional<Position> TransportMap::FreeSpotNear(WorldObject const& master, float distance2d,
                                                   float angle) const
{
    const auto anchor = PositionOf(master);
    if (!anchor)
    {
        return std::nullopt;                        // its master is not aboard after all
    }

    // The requested bearing first, then swept around the master: a ship is small and
    // cluttered, and the one spot the caller asked for is very often out over the rail.
    for (int step = 0; step < HULL_SPOT_BEARINGS; ++step)
    {
        const float bearing = anchor->Facing() + angle +
                              (step ? (2 * M_PI_F * float(step) / float(HULL_SPOT_BEARINGS)) : 0.0f);

        const float x = anchor->X() + distance2d * std::cos(bearing);
        const float y = anchor->Y() + distance2d * std::sin(bearing);

        const auto z = SurfaceAt(x, y, anchor->Z(), HULL_SEARCH_UP, HULL_SEARCH_DOWN);
        if (!z)
        {
            continue;                               // out over the rail
        }

        // ...and nothing solid in between, or we would set a pet down on the far side of a
        // bulkhead from the master it is supposed to be heeling.
        if (IsBlocked(Geometry::Vector3(anchor->X(), anchor->Y(), anchor->Z() + HULL_PROBE_HEIGHT),
                      Geometry::Vector3(x, y, *z + HULL_PROBE_HEIGHT)))
        {
            continue;
        }

        return Position(x, y, *z, anchor->Facing());
    }

    return std::nullopt;
}

/* ******************************** Who is aboard ************************************** */

bool TransportMap::Add(Player* passenger)
{
    // WHERE HE REALLY STANDS. The wire calls it an offset; the moment it is ours it is a
    // position on this map, composed with nothing.
    Position const* aboard = passenger->m_movementInfo.GetTransportPos();
    passenger->Place().MoveTo(aboard->x, aboard->y, aboard->z, aboard->o);

    passenger->GetMapRef().link(this, passenger);
    passenger->SetMap(this);

    CellPair p = MaNGOS::ComputeCellPair(passenger->Where().X(), passenger->Where().Y());
    Cell cell(p);
    EnsureGridLoadedAtEnter(cell, passenger);
    PromoteEnvelopeNeighboursToFull(cell.GridX(), cell.GridY());
    passenger->AddToWorld();

    // The ship, then the man standing on her. Nothing else: no world to introduce, no map
    // id he could be told. Her block is not stamped into his client set -- possession of a
    // vessel is map membership, and the elimination sweep must never learn she exists.
    UpdateData data;
    m_vessel->BuildCreateUpdateBlockForPlayer(&data, passenger);
    passenger->BuildCreateUpdateBlockForPlayer(&data, passenger);

    WorldPacket packet;
    // hasTransport, and it must be true: this packet carries the vessel and a
    // passenger whose own block names her. The byte is written on CLASSIC and TBC
    // only -- mangos_two omits the argument because there the field does not exist,
    // and copying that form here tells the client there is no transport in a packet
    // that is nothing but transport. It then has nothing to compose him against and
    // never leaves the loading screen.
    data.BuildPacket(&packet, true);
    passenger->GetSession()->SendPacket(&packet);

    // And the OTHER ships on the water she is crossing. His client is drawing that map, so
    // they are his to see, and no sweep of his will ever reach them: he is not on it.
    //
    // Never while she is between two maps: she would name the one she is leaving, and he
    // would be handed a continent's worth of ships that are not on the water he can see.
    if (Map* sailed = m_vessel->IsCrossing() ? NULL : m_vessel->GetMap())
    {
        MapManager::TransportsByMapType::const_iterator vessels =
            sMapMgr.m_TransportsByMap.find(sailed->GetId());
        if (vessels != sMapMgr.m_TransportsByMap.end())
        {
            for (Transport* other : vessels->second)
            {
                if (other != m_vessel && other->GetMap() == sailed)
                {
                    AnnounceVessel(other, passenger);
                }
            }
        }
    }

    NGridType* grid = getNGrid(cell.GridX(), cell.GridY());
    passenger->GetViewPoint().Event_AddedToWorld(&(*grid)(cell.CellX(), cell.CellY()));
    UpdateObjectVisibility(passenger, cell, p);

    return true;
}

void TransportMap::Embark(Player* passenger)
{
    if (!m_commissioned || !passenger->IsInWorld() || passenger->GetMap() == this)
    {
        return;
    }

    // He WALKED aboard: he really was ashore a moment ago, so he really does leave that map.
    // Login and the far side of a seam do not come through here -- they never touch the
    // world's grid at all, they are added straight to this map.
    DEBUG_FILTER_LOG(LOG_FILTER_DECK_MINIONS, "Embark: %s",
                     DescribeSpatially(passenger).c_str());

    passenger->GetMap()->Remove(passenger, false);
    Add(passenger);

    // His minions come with him, NOW. UpdateMinions reconciles this once per tick and is
    // the safety net for the half-dozen other ways one arrives -- but a pet that waits a
    // tick is a pet standing at the rail while its master walks off, which is what a player
    // actually sees. Retail teleports it beside him on the spot; so does this.
    DrawMinionsTo(passenger, this);
}

void TransportMap::Disembark(Player* passenger, float x, float y, float z, float o)
{
    if (passenger->GetMap() != this)
    {
        return;
    }

    Map* sailed = m_vessel ? m_vessel->GetMap() : NULL;
    if (!sailed)
    {
        return;
    }

    DEBUG_FILTER_LOG(LOG_FILTER_DECK_MINIONS, "Disembark: %s",
                     DescribeSpatially(passenger).c_str());

    Remove(passenger, false);
    passenger->Place().MoveTo(x, y, z, o);

    // BEFORE the add, not after. Map::Add sends SendInitTransports, whose loop skips
    // player->GetTransport() on the assumption that our own vessel already reached us
    // through SendInitSelf. Stepping ashore is the one case where that is false: leave the
    // pointer set and the ship he just left is the single vessel never announced to him, so
    // it vanishes the instant he is off it.
    passenger->SetTransport(NULL);

    sailed->Add(passenger);

    // And they follow him ashore in the same tick, for the same reason.
    DrawMinionsTo(passenger, sailed);
}

void TransportMap::VesselLeavingWorld(Map* oldWorld, uint32 newMapId,
                                      float x, float y, float z, float o)
{
    if (!oldWorld)
    {
        return;
    }

    // From EVERYONE there, not from a range: possession of a vessel is map membership, and
    // this is the moment membership ends.
    PlayerList const& ashore = oldWorld->GetPlayers();
    for (PlayerList::const_iterator itr = ashore.begin(); itr != ashore.end(); ++itr)
    {
        if (Player* leaving = itr->getSource())
        {
            RetractVessel(m_vessel, leaving);
        }
    }

    // Snapshotted: TeleportTo takes the player off this map, and the reference manager being
    // walked is the one it edits.
    std::vector<Player*> aboard;
    for (PlayerList::const_iterator itr = GetPlayers().begin(); itr != GetPlayers().end(); ++itr)
    {
        if (Player* passenger = itr->getSource())
        {
            aboard.push_back(passenger);
        }
    }

    for (Player* passenger : aboard)
    {
        if (passenger->IsDead() && !passenger->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST))
        {
            passenger->ResurrectPlayer(1.0);
        }

        // His CLIENT is sent to the new world map because it has to load that terrain; he
        // himself never leaves this one -- Player::BoardingMap puts him straight back aboard
        // on the far side. The vessel has NOT moved yet, so the transfer packet still names
        // the map he is really leaving.
        passenger->TeleportTo(newMapId, x, y, z, o, TELE_TO_NOT_LEAVE_TRANSPORT);
    }
}

void TransportMap::VesselEnteredWorld(Map* newWorld)
{
    if (!newWorld)
    {
        return;
    }

    // The same channel that took her away gives her back: everyone on the new map gains her
    // by membership, here and now, with no distance asked.
    PlayerList const& arriving = newWorld->GetPlayers();
    for (PlayerList::const_iterator itr = arriving.begin(); itr != arriving.end(); ++itr)
    {
        if (Player* found = itr->getSource())
        {
            AnnounceVessel(m_vessel, found);
        }
    }
}

void TransportMap::EnlistCrew(Creature* crew)
{
    if (!crew || !m_commissioned)
    {
        return;
    }

    // REFUSED AT THE GANGWAY. One of these aboard is not a creature that looks wrong, it is
    // every client watching the ship crashing the moment she gets under way -- so it is put
    // off rather than carried, and the log says which one.
    if (!CanRide(crew))
    {
        sLog.outErrorDb("Transport map %u: creature %u (guid %u) carries CreatureTypeFlags "
                        "0x%X, which includes TRANSPORT_FORBIDDEN; both of those bits together "
                        "kill every client watching a moving transport. Removed. Clear either "
                        "one of them if it really must sail.",
                        GetId(), crew->GetEntry(), crew->GetGUIDLow(),
                        crew->GetCreatureInfo() ? crew->GetCreatureInfo()->CreatureTypeFlags : 0);

        crew->AddObjectToRemoveList();
        return;
    }

    if (std::find(m_crew.begin(), m_crew.end(), crew) == m_crew.end())
    {
        m_crew.push_back(crew);
    }

    // Active, because this map's grids are pinned and no player is required to be on it to
    // keep it awake -- the observers are ashore, watching through the vessel.
    crew->SetActiveObjectState(true);

}

void TransportMap::DelistCrew(Creature* crew)
{
    m_crew.erase(std::remove(m_crew.begin(), m_crew.end(), crew), m_crew.end());
}

void TransportMap::UpdateMinions()
{
    // A MINION FOLLOWS ITS MASTER ACROSS A MAP BOUNDARY. That is the whole of it, and it is
    // the same move a pet makes following its master through a portal.
    for (PlayerList::const_iterator itr = GetPlayers().begin(); itr != GetPlayers().end(); ++itr)
    {
        Player* master = itr->getSource();
        if (!master)
        {
            continue;
        }

        master->CallForAllControlledUnits(
            [this, master](Unit* minion) { DrawMinionTo(minion, master, this); },
            CONTROLLED_PET | CONTROLLED_MINIPET | CONTROLLED_GUARDIANS | CONTROLLED_TOTEMS);
    }

    // And the reverse: anything aboard whose master has left.
    //
    // NOT `m_crew`. A pet is not crew and must never be enlisted as one -- crew are exempt
    // from the visibility sweep, and a pet that is exempt is a pet no client ever destroys,
    // which is how one came to swim up through the hull. So the roster it is looked for in
    // is the MAP'S OWN pet store: everything of the kind that is standing here, crew or not.
    // Scanning m_crew asked a container pets are not in, found nothing, and left every pet
    // whose master had walked ashore standing on the deck for good.
    std::vector<Creature*> stranded;

    for (auto const& entry : GetObjectsStore().GetElements<Pet>())
    {
        Creature* aboard = entry.second;
        if (!aboard || !aboard->IsInWorld() || IsPlanted(aboard))
        {
            continue;                       // a totem rides the ship it was planted on
        }

        Unit* master = aboard->GetOwner();
        if (master && master->IsInWorld() && master->FindMap() != this)
        {
            stranded.push_back(aboard);
        }
    }

    for (Creature* minion : stranded)
    {
        Unit* master = minion->GetOwner();
        DEBUG_FILTER_LOG(LOG_FILTER_DECK_MINIONS,
                         "UpdateMinions: STRANDED aboard %s", DescribeSpatially(minion).c_str());
        DrawMinionTo(minion, master, master->FindMap());
    }
}

/* ******************************** What the shore is told ***************************** */

void TransportMap::AppendCrewCreateBlocks(UpdateData& data, Player* observer)
{
    for (Creature* crew : m_crew)
    {
        if (crew->IsInWorld())
        {
            crew->BuildCreateUpdateBlockForPlayer(&data, observer);
        }
    }
}

void TransportMap::AppendCrewDestroyBlocks(UpdateData& data)
{
    for (Creature* crew : m_crew)
    {
        crew->BuildOutOfRangeUpdateBlock(&data);
    }
}

void TransportMap::SendCrewMemberCreate(Creature* crew)
{
    if (!crew || !crew->IsInWorld() || !m_vessel || !m_vessel->GetMap())
    {
        return;
    }

    PlayerList const& everyone = m_vessel->GetMap()->GetPlayers();
    for (PlayerList::const_iterator itr = everyone.begin(); itr != everyone.end(); ++itr)
    {
        Player* observer = itr->getSource();
        if (!observer)
        {
            continue;
        }

        UpdateData data;
        crew->BuildCreateUpdateBlockForPlayer(&data, observer);

        WorldPacket packet;
        data.BuildPacket(&packet, true);
        observer->SendDirectMessage(&packet);
    }
}

void TransportMap::AnnounceVessel(Transport* vessel, Player* observer)
{
    if (!vessel || !observer)
    {
        return;
    }

    // The hull AND everyone on her. The observer's own visibility sweep would find the crew
    // too, but only when HE moves -- and a man standing on a pier watching a ship come in
    // does not move. Leaving it to the sweep gave him an empty deck until he stepped aboard.
    UpdateData data;
    vessel->BuildCreateUpdateBlockForPlayer(&data, observer);

    if (TransportMap* hull = vessel->AsMap())
    {
        hull->AppendCrewCreateBlocks(data, observer);
    }

    WorldPacket packet;
    data.BuildPacket(&packet, true);
    observer->SendDirectMessage(&packet);
}

void TransportMap::RetractVessel(Transport* vessel, Player* observer)
{
    if (!vessel || !observer)
    {
        return;
    }

    // The CREW FIRST, the hull last. Reversed, the client loses the ship while it still holds
    // them, and they hang in the air at the last place it was drawn.
    UpdateData data;

    if (TransportMap* hull = vessel->AsMap())
    {
        hull->AppendCrewDestroyBlocks(data);
    }

    vessel->BuildOutOfRangeUpdateBlock(&data);

    WorldPacket packet;
    data.BuildPacket(&packet, true);
    observer->SendDirectMessage(&packet);
}

void TransportMap::CollectRelaySources(WorldObject const* viewer, float visibility,
                                       std::vector<RelaySource>& out)
{
    if (!viewer || !viewer->GetMap())
    {
        return;
    }

    // Aboard: his own pass walks the ship, and the shore is reached from her estimate. That
    // pose is a lie by up to NodeSlack, which is exactly why it is added.
    if (TransportMap const* hull = viewer->GetMap()->AsTransport())
    {
        Transport* vessel = hull->Vessel();
        if (Map* sailed = vessel ? vessel->GetMap() : NULL)
        {
            out.push_back({sailed, vessel->Where().X(), vessel->Where().Y(),
                           visibility + hull->HullRadius() + vessel->NodeSlack()});
        }
        return;
    }

    MapManager::TransportsByMapType::const_iterator vessels =
        sMapMgr.m_TransportsByMap.find(viewer->GetMapId());
    if (vessels == sMapMgr.m_TransportsByMap.end())
    {
        return;
    }

    for (Transport* vessel : vessels->second)
    {
        TransportMap* hull = vessel->AsMap();
        if (!hull || vessel->GetMap() != viewer->GetMap())
        {
            continue;
        }

        const float reach = visibility + hull->HullRadius() + vessel->NodeSlack();
        if (!vessel->Where().WithinDist(viewer->Where(), reach, false))
        {
            continue;
        }

        // The whole ship, swept from her origin. Her space is small and a crew is a few
        // dozen, so a radius that certainly covers the hull is cheaper than being exact.
        out.push_back({hull, 0.0f, 0.0f, hull->HullRadius() * 2.0f + visibility});
    }
}

void TransportMap::GatherObservers()
{
    Map* world = m_vessel ? m_vessel->GetMap() : NULL;
    if (!world)
    {
        return;
    }

    const float reach = world->GetVisibilityDistance() + m_hullRadius + m_vessel->NodeSlack();

    // FROM THE GRID, around the estimate. That approximate pose is what the estimate is FOR:
    // it names the cells to look in. Sweeping the whole map's player list instead would
    // answer the same question by reading every player on a continent.
    //
    // The check is written out rather than reusing InReach, which asks SharesWorld and brings
    // phase and world-membership rules that do not belong in a cell sweep.
    struct WatchingFromShore
    {
        WorldObject const* focus;
        float range;
        WorldObject const& GetFocusObject() const { return *focus; }
        bool operator()(Player* watcher) const
        {
            return watcher->IsInWorld() &&
                   focus->Where().WithinDist(watcher->Where(), range);
        }
    };

    std::list<Player*> found;
    WatchingFromShore check{m_vessel, reach};
    MaNGOS::PlayerListSearcher<WatchingFromShore> searcher(found, check);
    Cell::VisitWorldObjects(m_vessel, searcher, reach);

    // For BROADCAST only -- a spline, an emote, a spell go, anything said aboard while
    // nobody's visibility pass happens to be running. Visibility itself is decided by each
    // viewer's own sweep, which reaches across through CollectRelaySources.
    SetExternalObservers(std::vector<Player*>(found.begin(), found.end()));
}

void TransportMap::Update(const uint32& t_diff)
{
    // Phase one, on the thread of the map the vessel sails and with her pose already advanced
    // for this tick: who ashore is watching, and who aboard should not be.
    GatherObservers();
    UpdateMinions();

    // Then an ordinary map tick: grids, active objects, everyone aboard.
    Map::Update(t_diff);
}
