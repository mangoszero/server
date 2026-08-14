#include "Utilities/MathDefines.h"
#include <cmath>
#include <optional>
#include <algorithm>
#include <string>
#include <vector>
#include <list>
#include "botpch.h"
#include "TransportMap.h"
#include "../../playerbot.h"
#include "../values/LastMovementValue.h"
#include "MovementActions.h"
#include "MotionMaster.h"
#include "MovementGenerator.h"
#include "../../FleeManager.h"
#include "../../LootObjectStack.h"
#include "../../PlayerbotAIConfig.h"
#include "WorldHandlers/Transports.h"
#include "movement/MoveSplineInit.h"
#include "movement/MoveSpline.h"
#include "Creature.h"

using namespace ai;

bool MovementAction::MoveNear(uint32 mapId, float x, float y, float z, float distance)
{
    float angle = GetFollowAngle();
    return MoveTo(mapId, x + cos(angle) * distance, y + sin(angle) * distance, z);
}

bool MovementAction::MoveNear(WorldObject* target, float distance)
{
    if (!target)
    {
        return false;
    }

    distance += target->GetObjectBoundingRadius();

    float followAngle = GetFollowAngle();
    for (float angle = followAngle - M_PI; angle <= followAngle + M_PI; angle += M_PI / 4)
    {
        bool moved = MoveTo(target->GetMapId(),
            target->GetPositionX() + cos(angle) * distance,
            target->GetPositionY() + sin(angle) * distance,
            target->GetPositionZ());
        if (moved)
        {
            return true;
        }
    }
    return false;
}

bool MovementAction::MoveTo(uint32 mapId, float x, float y, float z, bool unsafe, bool ignoreReactDistance, bool requireRoute)
{
    bot->UpdateGroundPositionZ(x, y, z);
    if (!IsMovingAllowed(mapId, x, y, z, ignoreReactDistance))
    {
        return false;
    }

    if (!unsafe && ai->HasStrategy("cautious") && IsAggroPosition(x, y))
    {
        return false;
    }

    float distance = bot->GetDistance(x, y, z);
    if (distance > sPlayerbotAIConfig.contactDistance)
    {
        WaitForReach(distance);

        if (bot->IsSitState())
        {
            bot->SetStandState(UNIT_STAND_STATE_STAND);
        }

        if (bot->GetLootGuid())
        {
            bot->SetLootGuid(ObjectGuid());
            bot->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_LOOTING);
        }

        if (bot->IsNonMeleeSpellCasted(true))
        {
            bot->CastStop();
            ai->InterruptSpell();
        }

        // Do not restart a leg that is already going where we want to go.
        //
        // Every call here used to Clear() and re-lay the spline unconditionally, and
        // WaitForReach clamps the AI's next check to globalCoolDown -- about half a second --
        // whenever ANY target is set. A bot chasing something therefore wiped and re-issued
        // its movement twice a second. Measured live: 9113 MovePoint calls in ninety seconds,
        // individual bots sustaining two to three per second, and a packet capture whose
        // consecutive-spline pairs showed a median of 0.495 s of progress before the route
        // was abandoned. That is the dominant movement event on this server.
        //
        // The predicate is strict, because a first attempt at it was not and was unsafe in
        // two ways worth remembering:
        //
        //   * It compared against LastMovement -- the previous REQUEST -- and advanced that
        //     baseline even when it skipped, so a goal creeping forward could be ignored for
        //     as long as the stale spline lasted. It now compares against the destination the
        //     mover is ACTUALLY travelling to, read from the spline.
        //   * It treated every running spline as interchangeable, so a routed-only corpse
        //     move could be suppressed because an ordinary point spline happened to be in
        //     flight -- silently defeating MOVE_REQUIRE_ROUTE, which exists precisely to stop
        //     a bot cutting through geometry. A requireRoute call is never suppressed now.
        float activeX, activeY, activeZ;
        const bool sameGoal =
            !requireRoute &&
            bot->GetMotionMaster()->GetCurrentMovementGeneratorType() == POINT_MOTION_TYPE &&
            bot->GetMotionMaster()->GetDestination(activeX, activeY, activeZ) &&
            sqrt((activeX - x) * (activeX - x) +
                 (activeY - y) * (activeY - y) +
                 (activeZ - z) * (activeZ - z)) < sPlayerbotAIConfig.contactDistance;

        if (!sameGoal)
        {
            MotionMaster &mm = *bot->GetMotionMaster();
            mm.Clear();

            if (requireRoute)
            {
                mm.MovePointRouted(mapId, x, y, z);
            }
            else
            {
                mm.MovePoint(mapId, x, y, z);
            }
        }
    }

    AI_VALUE(LastMovement&, "last movement").Set(x, y, z, bot->GetOrientation());
    return true;
}

bool MovementAction::MoveTo(Unit* target, float distance)
{
    if (!IsMovingAllowed(target))
    {
        return false;
    }

    float bx = bot->GetPositionX();
    float by = bot->GetPositionY();

    float tx = target->GetPositionX();
    float ty = target->GetPositionY();
    float tz = target->GetPositionZ();

    float distanceToTarget = bot->GetDistance(target);
    float angle = bot->GetAngle(target);
    float needToGo = distanceToTarget - distance;

    float maxDistance = sPlayerbotAIConfig.spellDistance;
    if (needToGo > 0 && needToGo > maxDistance)
    {
        needToGo = maxDistance;
    }
    else if (needToGo < 0 && needToGo < -maxDistance)
    {
        needToGo = -maxDistance;
    }

    float dx = cos(angle) * needToGo + bx;
    float dy = sin(angle) * needToGo + by;

    if (needToGo != 0)
    {
        float travelAngle = needToGo > 0 ? angle : angle + M_PI;
        float travelDist  = fabs(needToGo);

        static const float deltas[] = { 0.0f, M_PI/6, -M_PI/6, M_PI/3, -M_PI/3, M_PI/2, -M_PI/2 };
        float bestSafeDist = 0.0f;
        float bestAngle    = travelAngle;
        for (float delta : deltas)
        {
            float safe = CalculateAggroFreeDistance(bx, by, travelAngle + delta, travelDist);
            if (safe > bestSafeDist)
            {
                bestSafeDist = safe;
                bestAngle    = travelAngle + delta;
            }
            if (bestSafeDist >= travelDist)
            {
                break;
            }
        }

        float moveDist = std::min(bestSafeDist, travelDist);
        if (moveDist < sPlayerbotAIConfig.contactDistance)
        {
            return false;
        }
        dx = cos(bestAngle) * moveDist + bx;
        dy = sin(bestAngle) * moveDist + by;
    }
    return MoveTo(target->GetMapId(), dx, dy, tz, true);
}

float MovementAction::GetFollowAngle()
{
    Player* master = GetMaster();
    Group* group = master ? master->GetGroup() : bot->GetGroup();
    if (!group)
    {
        return 0.0f;
    }

    int index = 1;
    for (GroupReference *ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        if ( ref->getSource() == master)
        {
            continue;
        }

        if ( ref->getSource() == bot)
        {
            int botCount = (int)group->GetMembersCount() - 1;
            return M_PI / 2.0f + M_PI * (index - 1) / std::max(botCount - 1, 1);
        }

        index++;
    }
    return 0;
}

bool MovementAction::IsMovingAllowed(Unit* target)
{
    if (!target)
    {
        return false;
    }

    if (bot->GetMapId() != target->GetMapId())
    {
        return false;
    }

    float distance = bot->GetDistance(target);
    if (distance > sPlayerbotAIConfig.reactDistance)
    {
        return false;
    }

    return IsMovingAllowed();
}

bool MovementAction::IsMovingAllowed(uint32 mapId, float x, float y, float z, bool ignoreReactDistance)
{
    if (!ignoreReactDistance)
    {
        float distance = bot->GetDistance(x, y, z);
        if (distance > sPlayerbotAIConfig.reactDistance)
        {
            return false;
        }
    }

    return IsMovingAllowed();
}

bool MovementAction::IsMovingAllowed()
{
    if (bot->IsFrozen() || bot->IsPolymorphed() ||
        (bot->IsDead() && !bot->GetCorpse()) ||
        bot->IsBeingTeleported() ||
        bot->GetTransport() ||
        bot->IsInRoots() ||
        bot->HasAuraType(SPELL_AURA_MOD_CONFUSE) || bot->IsCharmed() ||
        bot->HasAuraType(SPELL_AURA_MOD_STUN) || bot->IsTaxiFlying())
        return false;

    MotionMaster &mm = *bot->GetMotionMaster();
    return mm.GetCurrentMovementGeneratorType() != FLIGHT_MOTION_TYPE;
}

bool MovementAction::FollowOnTransport(Unit* target, Player* master)
{
    Transport* vessel = master->GetTransport();
    TransportMap* deck = vessel ? vessel->AsMap() : NULL;
    if (!deck)
    {
        return false;
    }

    // The master is aboard, so his coordinates are the vessel's own and the bot's are
    // the world's. Measuring between the two frames answers "infinitely far" by design,
    // so the question asked here is the bot's distance to the VESSEL -- the only thing
    // in this picture allowed a world position at all, and only an estimate of one,
    // which is why the vessel's own slack is added rather than pretended away.
    uint32 currentTime = time(0);
    const float reach = sPlayerbotAIConfig.sightDistance + vessel->NodeSlack();
    if (!bot->Where().WithinDist(vessel->Where(), reach))
    {
        bot->m_movementInfo.RemoveMovementFlag(MOVEFLAG_ONTRANSPORT);
        transportBoardingDelayTime = 0;
        return false;
    }

    PlayerbotMgr* mgr = master->GetPlayerbotMgr();
    uint32 botIdx = 0;
    uint32 botCount = 0;
    for (auto it = mgr->GetPlayerBotsBegin();
         it != mgr->GetPlayerBotsEnd(); ++it, ++botCount)
    {
        if (it->second == bot)
            break;
        ++botIdx;
    }
    float offsetRadius = 1.0f;
    float angleStep = 2.0f * M_PI / std::max((uint32)4, botCount);
    float offsetX = cos(angleStep * botIdx) * offsetRadius;
    float offsetY = sin(angleStep * botIdx) * offsetRadius;

    bool isApproaching = transportBoardingDelayTime > 0;
    bool approachTimedOut = isApproaching && (currentTime - transportBoardingDelayTime) > 1;
    // Determine if we should complete boarding now
    if (isApproaching && approachTimedOut)
    {
        MotionMaster& mm = *bot->GetMotionMaster();
        transportBoardingDelayTime = 0;
        bot->clearUnitState(UNIT_STAT_IGNORE_PATHFINDING);
        mm.Clear();
        bot->movespline->_Interrupt();

        // Embark moves him onto the vessel's map, and from there the deck IS his map:
        // the spot beside the master is chosen in the vessel's own frame, and no world
        // position is composed for either of them.
        //
        // The deck spot is set BEFORE the embark, and that order is the whole point.
        // TransportMap::Add places the passenger at m_movementInfo.GetTransportPos() and
        // builds the create packet from there. Boarding first and moving him afterwards
        // therefore announced him at one spot and then relocated him to another with
        // nothing sent -- the silent relocation this campaign exists to remove, and it
        // would have glided him across the deck in front of everyone already aboard.
        // Setting the offset first makes the create carry the final position, so there is
        // nothing left to correct. Same order TransportMap::Board uses.
        const float deckX = master->Where().X() + offsetX;
        const float deckY = master->Where().Y() + offsetY;
        const float deckZ = master->Where().Z();
        const float deckO = bot->Where().Facing();

        bot->SetTransport(vessel);
        bot->m_movementInfo.SetTransportData(vessel->GetObjectGuid(), deckX, deckY, deckZ, deckO, 0);
        deck->Embark(bot);

        AI_VALUE(LastMovement&, "last movement").Set(target);
        return true;
    }

    // Not aboard yet: he still walks in the world, so the target is the vessel, not the
    // master -- a deck coordinate would send him to the middle of the map.
    if (!isApproaching)
    {
        transportBoardingDelayTime = currentTime;
    }
    Movement::MoveSplineInit init(*bot);
    init.MoveTo(vessel->Where().X(), vessel->Where().Y(), vessel->Where().Z());
    init.SetWalk(false);
    init.Launch();
    AI_VALUE(LastMovement&, "last movement").Set(target);
    return true;
}

bool MovementAction::FollowOffTransport(Unit* target, Player* master)
{
    Transport* transport = master->GetTransport();
    Transport* botTransport = bot->GetTransport();
    if (!transport || transport != botTransport) // master has left the transport
    {
        TransportMap* deck = botTransport ? botTransport->AsMap() : NULL;
        if (!deck)
        {
            return true;
        }

        transportBoardingDelayTime = 0;
        bot->TradeCancel(false);

        // The master is ashore, so HIS position is a world one and is the right place to
        // put the bot down. Disembark is what moves him off the hull's map; nothing else
        // may, and no offset is composed from the deck he is leaving.
        deck->Disembark(bot, master->Where().X(), master->Where().Y(),
                        master->Where().Z(), bot->Where().Facing());
        bot->SetTransport(NULL);
        bot->m_movementInfo.ClearTransportData();
        bot->m_movementInfo.RemoveMovementFlag(MOVEFLAG_ONTRANSPORT);

        WorldPacket data(MSG_MOVE_HEARTBEAT, 64);
        data << bot->GetPackGUID();
        bot->m_movementInfo.Write(data);
        bot->SendMessageToSetInRange(&data, DEFAULT_VISIBILITY_DISTANCE, false);
        AI_VALUE(LastMovement&, "last movement").Set(target);
    }
    else
    {
        // Bot and master on same transport - clear any stale delay
        transportBoardingDelayTime = 0;
    }
    return true;
}

bool MovementAction::Follow(Unit* target, float distance)
{
    return Follow(target, distance, GetFollowAngle());
}

bool MovementAction::Follow(Unit* target, float distance, float angle)
{
    MotionMaster &mm = *bot->GetMotionMaster();

    if (!target || bot->IsBeingTeleported())
    {
        return false;
    }

    Player* master = target->ToPlayer();
    if (master  && bot->GetTransport() != master->GetTransport())
    {
        if(bot->GetTransport())
        {
            return FollowOffTransport(target, master);
        }
        else
        {
            if (master->GetTransport()) // master on transport
            {
                return FollowOnTransport(target, master);
            }
        }
    }

    if (bot->GetDistance2d(target->GetPositionX(), target->GetPositionY()) <= sPlayerbotAIConfig.sightDistance &&
        abs(bot->GetPositionZ() - target->GetPositionZ()) >= sPlayerbotAIConfig.spellDistance)
    {
        // Teleport, do not relocate in place.
        //
        // This used to call SetPosition directly for the same-map case, which is the very
        // defect that made bots appear to glide: a server-side relocation tells NO observer
        // anything -- UpdateVisibilityOf is transition-only -- so a watcher keeps rendering
        // the bot where it was, and the 1.12 client then stitches its own rendered position
        // onto the front of the next spline and travels the gap at four times run speed. The
        // vertical hop this performs is exactly the sort a nearby player is looking at.
        //
        // MotionMaster::Clear did not protect it either: Clear removes generators, it does
        // not interrupt the spline that is already running, so UpdateSplineMovement could
        // overwrite the relocation from the old leg a moment later.
        //
        // Routing through TeleportTo puts it on the normal bot path, where the faked ack in
        // HandleTeleportAck follows with a heartbeat and an observer resync.
        //
        // The map test is also fixed. `target->GetMapId() && ...` treated map 0 -- Eastern
        // Kingdoms -- as "no map", so a follow onto map 0 fell into the relocate branch and
        // silently kept the bot on whatever map it was already on. Compare the maps, and when
        // they differ take the target's OWN x and y: carrying the bot's coordinates into a
        // different map lands it at whatever happens to occupy them there.
        //
        // No Clear() first: the ack path clears on arrival, and clearing ahead of a
        // TeleportTo that can fail would strip the generators off a bot that never moved.
        //
        // TELE_TO_NOT_LEAVE_COMBAT because TeleportTo calls CombatStop by default and the
        // SetPosition this replaced did not. Without it, correcting a follower's height
        // while the group is fighting drops the bot's combat state -- shedding its threat
        // and letting whatever it was tanking pick a new target or evade. The flag keeps
        // the fix to what it is meant to be, a change of position and nothing else.
        // Compare Map*, not map id. Two instances of the same dungeon share an id, so an id
        // test calls them the same place and keeps the bot's own x and y -- which belong to
        // the instance it is standing in, not the one the target is in.
        const bool differentMap = bot->GetMap() != target->GetMap();
        const float x = differentMap ? target->GetPositionX() : bot->GetPositionX();
        const float y = differentMap ? target->GetPositionY() : bot->GetPositionY();
        const float z = target->GetPositionZ();

        // A refused teleport is not a follow. Claiming success left the bot standing while
        // the strategy believed it had acted, so it retried the same refusal every tick;
        // falling through lets the ordinary follow below carry it instead.
        if (!bot->TeleportTo(target->GetMapId(), x, y, z, bot->GetOrientation(),
                             TELE_TO_NOT_LEAVE_COMBAT))
        {
            return MoveTo(target, distance);
        }

        AI_VALUE(LastMovement&, "last movement").Set(target);
        return true;
    }

    if (!IsMovingAllowed(target))
    {
        return false;
    }

    if (target->IsFriendlyTo(bot) && bot->IsMounted() && AI_VALUE(list<ObjectGuid>, "possible targets").empty())
    {
        distance += angle;
    }

    if (bot->GetDistance(target) <= sPlayerbotAIConfig.followDistance)
    {
        return false;
    }

    if (bot->IsSitState())
    {
        bot->SetStandState(UNIT_STAND_STATE_STAND);
    }

    if (bot->IsNonMeleeSpellCasted(true))
    {
        bot->CastStop();
        ai->InterruptSpell();
    }

    float followX = target->GetPositionX() + cos(angle) * distance;
    float followY = target->GetPositionY() + sin(angle) * distance;
    if (IsAggroPosition(followX, followY))
    {
        return false;
    }

    mm.MoveFollow(target, distance, angle);

    AI_VALUE(LastMovement&, "last movement").Set(target);
    return true;
}

void MovementAction::WaitForReach(float distance)
{
    float delay = 1000.0f * distance / bot->GetSpeed(MOVE_RUN) + sPlayerbotAIConfig.reactDelay;

    if (delay > sPlayerbotAIConfig.maxWaitForMove)
    {
        delay = sPlayerbotAIConfig.maxWaitForMove;
    }

    Unit* target = *ai->GetAiObjectContext()->GetValue<Unit*>("current target");
    Unit* player = *ai->GetAiObjectContext()->GetValue<Unit*>("enemy player target");
    if ((player || target) && delay > sPlayerbotAIConfig.globalCoolDown)
    {
        delay = sPlayerbotAIConfig.globalCoolDown;
    }

    ai->SetNextCheckDelay((uint32)delay);
}

bool MovementAction::Flee(Unit *target)
{
    Player* master = GetMaster();
    if (!target)
    {
        target = master;
    }

    if (!target)
    {
        return false;
    }

    if (!sPlayerbotAIConfig.fleeingEnabled)
    {
        return false;
    }

    if (!IsMovingAllowed())
    {
        return false;
    }

    FleeManager manager(bot, sPlayerbotAIConfig.fleeDistance, GetFollowAngle());

    float rx, ry, rz;
    if (!manager.CalculateDestination(&rx, &ry, &rz))
    {
        return false;
    }

    return MoveTo(target->GetMapId(), rx, ry, rz);
}

/**
 * Returns the farthest distance along the beeline from (bx,by) at the
 * given angle that doesn't enter any hostile creature's aggro zone.
 * Returns maxDist if the entire path is clear.
 */
float MovementAction::CalculateAggroFreeDistance(float bx, float by,
    float angle, float maxDist)
{
    if (!ai->HasStrategy("cautious") ||
        bot->HasAuraType(SPELL_AURA_MOD_STEALTH) ||
        bot->HasAuraType(SPELL_AURA_MOD_INVISIBILITY))
    {
        return maxDist;
    }
    float cosA = cos(angle);
    float sinA = sin(angle);
    float safeDist = maxDist;

    list<ObjectGuid> targets = AI_VALUE(list<ObjectGuid>, "possible targets");
    for (list<ObjectGuid>::iterator i = targets.begin(); i != targets.end(); ++i)
    {
        Unit* unit = ai->GetUnit(*i);
        if (!unit || !unit->IsAlive() || unit->IsInCombat() || !unit->IsHostileTo(bot) || unit == bot->getVictim())
        {
            continue;
        }

        Creature* creature = dynamic_cast<Creature*>(unit);
        if (!creature || !creature->CanInitiateAttack())
        {
            continue;
        }

        float aggroRange = creature->GetAttackDistance(bot);
        float ex = bx - creature->GetPositionX();
        float ey = by - creature->GetPositionY();
        float b = ex * cosA + ey * sinA;
        float c = ex * ex + ey * ey - aggroRange * aggroRange;

        float disc = b * b - c;
        if (disc < 0)
        {
            continue;
        }

        float sqrtDisc = sqrt(disc);
        float tEntry = -b - sqrtDisc;
        if (tEntry < 0)
        {
            continue;
        }

        if (tEntry < safeDist)
        {
            safeDist = std::max(0.0f, tEntry - 2.0f);
        }
    }

    return safeDist;
}

bool MovementAction::IsAggroPosition(float x, float y)
{
    float bx = bot->GetPositionX();
    float by = bot->GetPositionY();

    float dx = x - bx;
    float dy = y - by;
    float dist = sqrt(dx * dx + dy * dy);
    if (dist < 0.1f)
    {
        return false;
    }

    float angle = atan2(dy, dx);
    return CalculateAggroFreeDistance(bx, by, angle, dist) < dist;
}

bool MovementAction::FindNearbyLosPoint(Unit* target, float& nx, float& ny,
                                        float& nz, float maxRadius)
{
    if (!target)
    {
        return false;
    }

    float bx = bot->GetPositionX();
    float by = bot->GetPositionY();
    float bz = bot->GetPositionZ();
    float tx = target->GetPositionX();
    float ty = target->GetPositionY();
    float tz = target->GetPositionZ();

    Map* map = bot->GetMap();

    for (float r = 2.0f; r <= maxRadius; r += 2.0f)
    {
        int steps = std::max(8, (int)(2.0f * M_PI * r / 2.0f));
        for (int i = 0; i < steps; i++)
        {
            float angle = 2.0f * M_PI * i / steps;
            float x = bx + r * cos(angle);
            float y = by + r * sin(angle);
            float z = map->GetHeight(x, y, bz);

            if (map->IsInLineOfSight(x, y, z + 2.0f, tx, ty, tz + 2.0f) &&
                IsMovingAllowed(map->GetId(), x, y, z))
            {
                nx = x;
                ny = y;
                nz = z;
                return true;
            }
        }
    }
    return false;
}

bool FleeAction::Execute(Event event)
{
    return Flee(AI_VALUE(Unit*, "current target"));
}

bool FleeAction::isUseful()
{
    return AI_VALUE(uint8, "attacker count") > 0 &&
        AI_VALUE2(float, "distance", "current target") <= sPlayerbotAIConfig.tooCloseDistance;
}

bool RunAwayAction::Execute(Event event)
{
    return Flee(AI_VALUE(Unit*, "master target"));
}

template<typename F>
static std::vector<WorldObject*> CollectValidUnits(PlayerbotAI* ai, const std::list<ObjectGuid>& guids, F filter)
{
    Player* bot = ai->GetBot();
    std::vector<WorldObject*> results;
    for (const auto& guid : guids)
    {
        Unit* unit = ai->GetUnit(guid);
        if (unit && filter(unit) && bot->GetDistance(unit) > sPlayerbotAIConfig.tooCloseDistance)
        {
            results.push_back(unit);
        }
    }
    return results;
}

template<typename F>
static std::vector<WorldObject*> CollectValidGameObjects(PlayerbotAI* ai, const std::list<ObjectGuid>& guids, F filter)
{
    Player* bot = ai->GetBot();
    std::vector<WorldObject*> results;
    for (const auto& guid : guids)
    {
        GameObject* go = ai->GetGameObject(guid);
        if (go && filter(go) && bot->GetDistance(go) > sPlayerbotAIConfig.tooCloseDistance)
        {
            results.push_back(go);
        }
    }
    return results;
}

bool MoveRandomAction::Execute(Event event)
{
    if (m_hasFaceTarget)
    {
        if (bot->IsStopped())
        {
            m_hasFaceTarget = false;
            bot->SetFacingTo(bot->GetAngle(m_faceX, m_faceY));
        }
        return true;
    }

    WorldObject* target = NULL;

    // If no configured targets, fall through to random-position fallback
    if (!sPlayerbotAIConfig.randomMovementTargets.empty())
    {
        // Cache all value queries ONCE per Execute()
        std::list<ObjectGuid> npcs = AI_VALUE(std::list<ObjectGuid>, "nearest npcs");
        std::list<ObjectGuid> players = AI_VALUE(std::list<ObjectGuid>, "nearest players");
        std::list<ObjectGuid> gos = AI_VALUE(std::list<ObjectGuid>, "nearest game objects");

        struct CategoryPick { WorldObject* target; int weight; };
        std::vector<CategoryPick> picks;
        size_t configSize = sPlayerbotAIConfig.randomMovementTargets.size();

        for (size_t idx = 0; idx < configSize; ++idx)
        {
            const std::string& type = sPlayerbotAIConfig.randomMovementTargets[idx];
            int weight = 1 << (configSize - 1 - idx);

            if (type == "random")
            {
                picks.push_back({nullptr, weight});
                continue;
            }

            std::vector<WorldObject*> matches;

            if (type == "anynpcs")
            {
                matches = CollectValidUnits(ai, npcs, [](Unit*) { return true; });
            }
            else if (type == "usefulnpcs")
            {
                matches = CollectValidUnits(ai, npcs, [](Unit* u)
                {
                    Creature* c = dynamic_cast<Creature*>(u);
                    return c && c->GetUInt32Value(UNIT_NPC_FLAGS) != UNIT_NPC_FLAG_NONE;
                });
            }
            else if (type == "players")
            {
                matches = CollectValidUnits(ai, players, [](Unit*) { return true; });
            }
            else if (type == "tradeskillitems")
            {
                for (std::list<ObjectGuid>::const_iterator gi = gos.begin(); gi != gos.end(); ++gi)
                {
                    LootObject loot(bot, *gi);
                    if (loot.skillId != SKILL_NONE && bot->HasSkill(loot.skillId))
                    {
                        GameObject* go = ai->GetGameObject(*gi);
                        if (go && bot->GetDistance(go) > sPlayerbotAIConfig.tooCloseDistance)
                        {
                            matches.push_back(go);
                        }
                    }
                }
            }
            else if (type == "interactableitems")
            {
                matches = CollectValidGameObjects(ai, gos, [](GameObject* go)
                {
                    uint32 t = go->GetGOInfo()->type;
                    return t == GAMEOBJECT_TYPE_CHEST || t == GAMEOBJECT_TYPE_QUESTGIVER ||
                           t == GAMEOBJECT_TYPE_SPELL_FOCUS || t == GAMEOBJECT_TYPE_GOOBER;
                });
            }
            else if (type == "anyitems")
            {
                matches = CollectValidGameObjects(ai, gos, [](GameObject*) { return true; });
            }

            if (!matches.empty())
            {
                picks.push_back({matches[urand(0, matches.size() - 1)], weight});
            }
        }

        if (!picks.empty())
        {
            int totalWeight = 0;
            for (const auto& pick : picks)
            {
                totalWeight += pick.weight;
            }

            int roll = urand(0, totalWeight - 1);
            for (const auto& pick : picks)
            {
                if (roll < pick.weight)
                {
                    target = pick.target;
                    break;
                }
                roll -= pick.weight;
            }
        }
    }

    if (target)
    {
        bool moved = MoveNear(target);
        if (moved)
        {
            m_faceX = target->GetPositionX();
            m_faceY = target->GetPositionY();
            m_hasFaceTarget = true;
        }
        return moved;
    }

    float distance = sPlayerbotAIConfig.tooCloseDistance + sPlayerbotAIConfig.grindDistance * urand(3, 10) / 10.0f;

    for (int i = 0; i < 10; ++i)
    {
        float x = bot->GetPositionX();
        float y = bot->GetPositionY();
        float z = bot->GetPositionZ();
        x += urand(0, distance) - distance / 2;
        y += urand(0, distance) - distance / 2;
        bot->UpdateGroundPositionZ(x, y, z);

        bool moved = MoveNear(bot->GetMapId(), x, y, z);
        if (moved)
        {
            return true;
        }
    }

    return false;
}

bool MoveToLootAction::Execute(Event event)
{
    LootObject loot = AI_VALUE(LootObject, "loot target");
    if (!loot.IsLootPossible(bot))
    {
        return false;
    }

    return MoveNear(loot.GetWorldObject(bot));
}

bool MoveOutOfEnemyContactAction::Execute(Event event)
{
    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target)
    {
        return false;
    }

    return MoveNear(target, sPlayerbotAIConfig.meleeDistance);
}

bool MoveOutOfEnemyContactAction::isUseful()
{
    return AI_VALUE2(float, "distance", "current target") <= sPlayerbotAIConfig.contactDistance;
}

bool SetFacingTargetAction::Execute(Event event)
{
    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target)
    {
        return false;
    }

    bot->SetFacingTo(bot->GetAngle(target));
    ai->SetNextCheckDelay(sPlayerbotAIConfig.globalCoolDown);
    return true;
}

bool SetFacingTargetAction::isUseful()
{
    return !AI_VALUE2(bool, "facing", "current target");
}

bool JumpAction::Execute(Event event)
{
    string const param = event.getParam();

    if (param == "forward")
    {
        if (ai->IsJumping())
        {
            return false;
        }
        ai->StartJump(true);
        return true;
    }

    if (param == "master")
    {
        if (ai->IsJumping())
        {
            return false;
        }
        Player* master = ai->GetMaster();
        if (!master)
        {
            return false;
        }
        float angle = bot->GetAngle(master);
        bot->SetFacingTo(angle);
        ai->StartJump(true, angle);
        return true;
    }

    if (param == "here")
    {
        if (ai->IsJumping() || ai->IsPendingJump())
        {
            return false;
        }
        ai->RequestJump();
        return ai->IsPendingJump();
    }

    if (ai->IsJumping())
    {
        return false;
    }

    ai->StartJump(false);
    return true;
}

bool SwimToSurfaceAction::isUseful()
{
    return bot->IsUnderWater();
}

bool SwimToSurfaceAction::Execute(Event event)
{
    float x = bot->GetPositionX();
    float y = bot->GetPositionY();
    float z = bot->GetPositionZ();

    std::optional<float> waterLevel = bot->GetMap()->GetTerrain()->GetWaterLevel(x, y, z);
    if (!waterLevel)
    {
        return false;
    }

    MotionMaster &mm = *bot->GetMotionMaster();
    mm.Clear();
    mm.MovePoint(bot->GetMapId(), x, y, *waterLevel, true);
    ai->SetNextCheckDelay(sPlayerbotAIConfig.globalCoolDown);
    return true;
}
