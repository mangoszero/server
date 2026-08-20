/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
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
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#ifndef MANGOS_TRANSPORT_MAP_H
#define MANGOS_TRANSPORT_MAP_H

#include "Map.h"

#include <optional>
#include <string>
#include <vector>

class Creature;
class Player;
class Transport;
class Unit;
class UpdateData;

/// One line saying where a unit ACTUALLY is -- map, whether that map is a deck, the frame its
/// coordinates are measured in, the pose, and which generator is driving it. The deck bugs are
/// all the same picture from the client and all a different answer here.
std::string DescribeSpatially(Unit* u);

/// A map region a visibility pass must sweep in addition to the viewer's own, because a
/// vessel and the shore it sails past are two maps that must see each other.
struct RelaySource
{
    Map* map;
    float x;
    float y;
    float radius;
};

/**
 * @brief A VESSEL, AS A MAP -- and the map is where the ship's whole life is kept.
 *
 * The hull's baked geometry is this map's terrain, so height, collision and routing aboard
 * are answered by the ordinary engines; model space is this map's space, so a position
 * aboard is composed with nothing and transformed by nothing. What is left over is what a
 * continent does not have, and it all lives here rather than on the game object:
 *
 *  - the CREW and the PASSENGERS, who are on this map and nowhere else,
 *  - the MINIONS, reconciled against masters who may be standing on another map entirely,
 *  - the hull's extent, and the rail: off the edge is refused, not clamped,
 *  - the observers ASHORE, who must be told what happens here although they are elsewhere.
 *
 * The game object keeps the route and its own pose, because someone has to know where to
 * look for those observers. It keeps nothing else.
 */
class TransportMap : public Map
{
    public:
        TransportMap(uint32 id, time_t expiry, Transport* vessel)
            : Map(id, expiry, 0), m_vessel(vessel) {}
        ~TransportMap() {}

        /**
         * @brief Run this map, nested inside the vessel's tick.
         *
         * Not a map in the scheduler's rotation: it has no independent existence, so it runs
         * on the thread of the map the vessel sails, after that map has finished walking its
         * own containers. That is what makes every packet sent from here sendable at once,
         * with no queue and no tick of latency.
         */
        void Update(const uint32&) override;

        /**
         * @brief A passenger arrives. NOT what an ordinary map does on entry, and it does not
         *        borrow any of it.
         *
         * His client has never heard of this map and never will: it was told the world map
         * the ship sails, and it is still rendering it. So there is no new world to introduce
         * -- only the ship herself, whom no sweep of his can ever reach because she stands on
         * that other map, and then himself. In that order, and the order is load-bearing: his
         * own block names her guid, and a client that does not hold her yet drops him at the
         * origin of the world.
         *
         * Everyone else aboard -- crew, pets, the other passengers -- arrives through the
         * ordinary visibility pass at the end, because on this map they are ordinary objects
         * in ordinary cells.
         */
        bool Add(Player* passenger) override;

        TransportMap* AsTransport() override { return this; }
        TransportMap const* AsTransport() const override { return this; }

        /// The game object whose hull this map is. Never NULL for a map of this kind.
        Transport* Vessel() const { return m_vessel; }

        /**
         * @brief Measure the hull and pin its grids, once, at start-up.
         *
         * False when the baker left nothing here. The map stays -- it is still the vessel's
         * relay -- but it takes nobody aboard: Map::Add cannot load a grid with no terrain,
         * so whoever stepped on would be removed from the world he was in and added to
         * nothing, and end up at (0,0) belonging nowhere.
         */
        bool Commission();

        /// Is there a hull here at all? False and she carries no one, whatever Map.dbc says.
        bool IsCommissioned() const { return m_commissioned; }

        // --- The hull, as terrain --------------------------------------------------

        /// The hull's extent, from the baked mesh. A ship is not a point.
        float HullRadius() const { return m_hullRadius; }

        /// The surface under a point: the highest one at or below it, found by dropping a ray
        /// from `searchUp` above. Nothing when there is none -- which is a REFUSAL, not an
        /// invitation to fall back to the water the ship is crossing.
        std::optional<float> SurfaceAt(float x, float y, float z,
                                       float searchUp, float searchDown) const;

        /// True when the hull's own geometry stands between two points on it.
        bool IsBlocked(Geometry::Vector3 const& from, Geometry::Vector3 const& to) const;

        /**
         * @brief A spot `distance2d` yards from `master` at its facing plus `angle`.
         *
         * The requested bearing is tried first and then swept around the master, because a
         * ship is small and cluttered and the one spot asked for is very often out over the
         * rail. Nothing when `master` is not aboard.
         */
        std::optional<Position> FreeSpotNear(WorldObject const& master, float distance2d,
                                             float angle) const;

        /// Where something aboard stands, or nothing when it is not on this ship. There is
        /// nothing to look up and nothing to convert: this map's coordinates are the answer.
        std::optional<Geometry::Placement> PositionOf(WorldObject const& obj) const;

        // --- Who is aboard ---------------------------------------------------------

        /// Bring a player onto this map. Only once his client is already in the world and
        /// has been sent the vessel -- it must never be told this map id, having no terrain
        /// for it, and dies in CMap::LoadWdt() looking for one.
        void Embark(Player* passenger);

        /**
         * @brief Bring a player aboard FROM ANYWHERE, at a named spot on this deck.
         *
         * Embark is for the man who walked up the gangplank and whose client already holds
         * the vessel. This is for everyone else -- a teleport, a summon, a script -- and it
         * takes the only route that works: the deck spot goes into his movement state, his
         * client is sent to the water she sails, and Player::BoardingMap puts him on this
         * map on the far side of the world-port ack. That is the login path exactly.
         *
         * @param x,y,z,o A position on THIS map. Composed with nothing, converted from nothing.
         * @return false when she cannot take him -- no hull, or she is between two maps. He
         *         has not been moved: refusal happens before any state is written.
         */
        bool Board(Player* passenger, float x, float y, float z, float o, uint32 options = 0);

        /// Put him down on the map the ship sails, at the point the CALLER names. Never
        /// called on unregistering: whoever ends the voyage owns the destination.
        void Disembark(Player* passenger, float x, float y, float z, float o);

        // --- The seam ----------------------------------------------------------------
        //
        // The vessel announces WHEN she crosses and nothing more; she knows nothing about
        // the people on her. What crossing MEANS for them is decided here, on both sides.

        /**
         * @brief She is LEAVING `oldWorld` for `newMapId`, and has not gone yet.
         *
         * Everyone on the old map loses her, and this map with her. Her own passengers are
         * started on their way to the new one -- their CLIENTS must load its terrain, they
         * themselves never leave this map. It happens HERE, before she moves, because the
         * transfer packet they are sent names the map they are leaving, and a moment later
         * that would already be the map they are going to.
         */
        void VesselLeavingWorld(Map* oldWorld, uint32 newMapId,
                                float x, float y, float z, float o);

        /// She has arrived on `newWorld`: everyone there gains her. This map itself does not
        /// move -- it never does, and its crew never learn any of this happened.
        void VesselEnteredWorld(Map* newWorld);

        /**
         * @brief Index a creature that has arrived. An INDEX, not a registration: nothing
         *        here decides whether it is aboard, its map already did.
         *
         * Called from Creature::Add/RemoveFromWorld -- the one place both the grid loader
         * and Map::Add pass through.
         */
        void EnlistCrew(Creature* crew);
        void DelistCrew(Creature* crew);
        bool HasCrew() const { return !m_crew.empty(); }

        /// Drop the index at shutdown. The creatures are this map's to destroy, like any
        /// other map's.
        void ReleaseCrew() { m_crew.clear(); }

        // --- What the shore is told ------------------------------------------------
        //
        // A vessel is not seen the way a mob is. A mob lives in a cell and the visibility
        // sweep polls distance to it every tick; a ship's pose is a waypoint estimate that
        // disagrees with the curve the client draws, so any distance test flickers. So
        // possession is MAP MEMBERSHIP and it is edge-triggered: you have a vessel, and
        // everyone aboard her, for as long as you share the map she sails.

        /// The vessel AND everyone on her. Static because a vessel the baker gave nothing to
        /// has no map to ask, and is then exactly what the base class says it is. The
        /// append-only seam lets login reuse the blocks without forcing an early packet.
        static void AppendVesselCreateBlocks(Transport* vessel, Player* observer, UpdateData& data);
        static void AnnounceVessel(Transport* vessel, Player* observer);
        static void RetractVessel(Transport* vessel, Player* observer);

        /**
         * @brief The cell sources a viewer's visibility pass must sweep BESIDES his own map.
         *
         * For someone aboard, the shore his ship sails past; for someone ashore, every ship
         * within reach. One notifier, several sources, one elimination -- so an object that
         * drops out across the boundary is destroyed by the ordinary sweep, with no ledger
         * and no special case anywhere.
         */
        static void CollectRelaySources(WorldObject const* viewer, float visibility,
                                        std::vector<RelaySource>& out);

        /// One newly-arrived crew member, announced to everyone already watching. `.trans npc
        /// add` boards mid-voyage, with an audience.
        void SendCrewMemberCreate(Creature* crew);

        /// Append the crew's create blocks to a packet already carrying the vessel's.
        /// Deliberately does NOT stamp m_clientGUIDs: they ride the vessel's map-membership
        /// channel, and the elimination sweep -- the only thing that set feeds -- must never
        /// learn they exist.
        void AppendCrewCreateBlocks(UpdateData& data, Player* observer);
        void AppendCrewDestroyBlocks(UpdateData& data);

    private:
        /**
         * @brief Reconcile which minions belong aboard, once per tick.
         *
         * Reconciled rather than hooked onto a boarding event, because there are half a dozen
         * ways a minion comes to be standing here -- its master walks aboard, it is summoned
         * at sea, its master logs in mid-voyage, a totem is dropped on the forecastle -- and
         * only one way to be sure we caught them all.
         */
        void UpdateMinions();

        /// One subscription to the outer world, held on behalf of everyone aboard. Computed
        /// once rather than once per passenger: everyone on a ship is within a few yards of
        /// everyone else, so they share an answer.
        void GatherObservers();

        Transport* m_vessel;

        bool m_commissioned = false;

        float m_hullRadius = 0.0f;

        /// In arrival order, so the tick is deterministic and the seam can hand out destroy
        /// blocks in the right order.
        std::vector<Creature*> m_crew;
};

#endif
