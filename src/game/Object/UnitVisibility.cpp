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



#include "Unit.h"
#include "Log.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "World.h"
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "SpellMgr.h"
#include "QuestDef.h"
#include "Player.h"
#include "Creature.h"
#include "Spell.h"
#include "Group.h"
#include "SpellAuras.h"
#include "MapManager.h"
#include "ObjectAccessor.h"
#include "CreatureAI.h"
#include "TemporarySummon.h"
#include "Formulas.h"
#include "Pet.h"
#include "Util.h"
#include "Totem.h"
#include "BattleGround/BattleGround.h"
#include "InstanceData.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "MapPersistentStateMgr.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "VMapFactory.h"
#include "MovementGenerator.h"
#include "movement/MoveSplineInit.h"
#include "movement/MoveSpline.h"
#include "CreatureLinkingMgr.h"
#include "GameTime.h"
#include <math.h>
#include <stdarg.h>
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */
#ifdef ENABLE_ELUNA
#include "ElunaConfig.h"
#endif /* ENABLE_ELUNA */
#ifdef ENABLE_ELUNA
#include "ElunaEventMgr.h"
#endif /* ENABLE_ELUNA */

/**
 * @brief Checks whether the unit is visible to or detectable by another unit.
 *
 * @param u The observing unit.
 * @param viewPoint The viewpoint used for checks.
 * @param detect True to include stealth detection logic.
 * @param inVisibleList True when evaluating retention in an existing visible list.
 * @param is3dDistance True to use 3D distance checks.
 * @return True if the unit should be seen or detected; otherwise, false.
 */
bool Unit::IsVisibleForOrDetect(Unit const* u, WorldObject const* viewPoint, bool detect, bool inVisibleList, bool is3dDistance) const
{
    if (!u || !IsInMap(u))
    {
        return false;
    }

    // Always can see self
    if (u == this)
    {
        return true;
    }

    // player visible for other player if not logout and at same transport
    // including case when player is out of world
    bool at_same_transport =
        GetTypeId() == TYPEID_PLAYER &&  u->GetTypeId() == TYPEID_PLAYER &&
        !((Player*)this)->GetSession()->PlayerLogout() && !((Player*)u)->GetSession()->PlayerLogout() &&
        !((Player*)this)->GetSession()->PlayerLoading() && !((Player*)u)->GetSession()->PlayerLoading() &&
        ((Player*)this)->GetTransport() && ((Player*)this)->GetTransport() == ((Player*)u)->GetTransport();

    // not in world
    if (!at_same_transport && (!IsInWorld() || !u->IsInWorld()))
    {
        return false;
    }

    // forbidden to seen (while Removing corpse)
    if (m_Visibility == VISIBILITY_REMOVE_CORPSE)
    {
        return false;
    }

    Map& _map = *u->GetMap();
    // Grid dead/alive checks
    if (u->GetTypeId() == TYPEID_PLAYER)
    {
        // non visible at grid for any stealth state
        if (!IsVisibleInGridForPlayer((Player*)u))
        {
            return false;
        }

        // if player is dead then he can't detect anyone in any cases
        if (!u->IsAlive())
        {
            detect = false;
        }
    }
    else
    {
        // all dead creatures/players not visible for any creatures
        if (!u->IsAlive() || !IsAlive())
        {
            return false;
        }
    }

    // different visible distance checks
    if (u->IsTaxiFlying())                                  // what see player in flight
    {
        // use object grey distance for all (only see objects any way)
        if (!IsWithinDistInMap(viewPoint, World::GetMaxVisibleDistanceInFlight() + (inVisibleList ? World::GetVisibleObjectGreyDistance() : 0.0f), is3dDistance))
        {
            return false;
        }
    }
    else if (!at_same_transport)                            // distance for show player/pet/creature (no transport case)
    {
        // Honor a per-viewpoint visibility distance override (e.g. the cinematic
        // flyover body), otherwise use the map default.
        float visibilityDistance = viewPoint->GetVisibilityDistanceOverride();
        if (visibilityDistance <= 0.0f)
        {
            visibilityDistance = _map.GetVisibilityDistance();
        }

        // Any units far than max visible distance for viewer or not in our map are not visible too
        if (!IsWithinDistInMap(viewPoint, visibilityDistance + (inVisibleList ? World::GetVisibleUnitGreyDistance() : 0.0f), is3dDistance))
        {
            return false;
        }
    }

    // always seen by owner
    if (GetCharmerOrOwnerGuid() == u->GetObjectGuid())
    {
        return true;
    }

    // IsInvisibleForAlive() those units can only be seen by dead or if other
    // unit is also invisible for alive.. if an IsInvisibleForAlive unit dies we
    // should be able to see it too
    if (u->IsAlive() && IsAlive() && IsInvisibleForAlive() != u->IsInvisibleForAlive())
    {
        if (u->GetTypeId() != TYPEID_PLAYER || !((Player*)u)->isGameMaster())
        {
            return false;
        }
    }

    // Visible units, always are visible for all units, except for units under invisibility
    if (m_Visibility == VISIBILITY_ON && u->m_invisibilityMask == 0)
    {
        return true;
    }

    // GMs see any players, not higher GMs and all units
    if (u->GetTypeId() == TYPEID_PLAYER && ((Player*)u)->isGameMaster())
    {
        if (GetTypeId() == TYPEID_PLAYER)
        {
            return ((Player*)this)->GetSession()->GetSecurity() <= ((Player*)u)->GetSession()->GetSecurity();
        }
        else
        {
            return true;
        }
    }

    // non faction visibility non-breakable for non-GMs
    if (m_Visibility == VISIBILITY_OFF)
    {
        return false;
    }

    // grouped players should always see stealthed party members
    if (GetTypeId() == TYPEID_PLAYER && u->GetTypeId() == TYPEID_PLAYER)
    {
        if (((Player*)this)->IsGroupVisibleFor(((Player*)u)) && u->IsFriendlyTo(this))
        {
            return true;
        }
    }

    // raw invisibility
    bool invisible = (m_invisibilityMask != 0 || u->m_invisibilityMask != 0);

    // detectable invisibility case
    if (invisible &&
        // Invisible units, always are visible for units under same invisibility type
        ((m_invisibilityMask & u->m_invisibilityMask) != 0 ||
        // Invisible units, always are visible for unit that can detect this invisibility (have appropriate level for detect)
        u->CanDetectInvisibilityOf(this) ||
        // Units that can detect invisibility always are visible for units that can be detected
        CanDetectInvisibilityOf(u)))
    {
        invisible = false;
    }

    // special cases for always overwrite invisibility/stealth
    if (invisible || m_Visibility == VISIBILITY_GROUP_STEALTH)
    {
        if (u->IsHostileTo(this))
        {
            // Hunter mark functionality
            AuraList const& auras = GetAurasByType(SPELL_AURA_MOD_STALKED);
            for (AuraList::const_iterator iter = auras.begin(); iter != auras.end(); ++iter)
            {
                if ((*iter)->GetCasterGuid() == u->GetObjectGuid())
                {
                    return true;
                }
            }
        }

        // none other cases for detect invisibility, so invisible
        if (invisible)
        {
            return false;
        }
    }

    // unit got in stealth in this moment and must ignore old detected state
    if (m_Visibility == VISIBILITY_GROUP_NO_DETECT)
    {
        return false;
    }

    // GM invisibility checks early, invisibility if any detectable, so if not stealth then visible
    if (m_Visibility != VISIBILITY_GROUP_STEALTH)
    {
        return true;
    }

    // NOW ONLY STEALTH CASE

    // if in non-detect mode then invisible for unit
    // mobs always detect players (detect == true)... return 'false' for those mobs which have (detect == false)
    // players detect players only in Player::HandleStealthedUnitsDetection()
    if (!detect)
    {
        return (u->GetTypeId() == TYPEID_PLAYER) ? ((Player*)u)->HaveAtClient(this) : false;
    }

    // Special cases

    // If is attacked then stealth is lost, some creature can use stealth too
    if (!getAttackers().empty())
    {
        return true;
    }

    // If there is collision rogue is seen regardless of level difference
    if (IsWithinDist(u, 0.24f))
    {
        return true;
    }

    // If a mob or player is stunned he will not be able to detect stealth
    if (u->hasUnitState(UNIT_STAT_STUNNED) && (u != this))
    {
        return false;
    }

    // set max ditance
    float visibleDistance = (u->GetTypeId() == TYPEID_PLAYER) ? MAX_PLAYER_STEALTH_DETECT_RANGE : ((Creature const*)u)->GetAttackDistance(this);

    // Always invisible from back (when stealth detection is on), also filter max distance cases
    bool IsInFront = viewPoint->IsInFrontInMap(this, visibleDistance);
    if (!IsInFront)
    {
        return false;
    }

    // Calculation if target is in front

    // Visible distance based on stealth value (stealth rank 4 300MOD, 10.5 - 3 = 7.5)
    visibleDistance = (10.5f - (GetTotalAuraModifier(SPELL_AURA_MOD_STEALTH) / 100.0f)) /2;

    // Visible distance is modified by
    //-Level Diff (every level diff = 1.0f in visible distance)
    visibleDistance += int32(u->GetLevelForTarget(this)) - int32(GetLevelForTarget(u));

    // This allows to check talent tree and will add addition stealth dependent on used points)
    int32 stealthMod = GetTotalAuraModifier(SPELL_AURA_MOD_STEALTH_LEVEL);
    if (stealthMod < 0)
    {
        stealthMod = 0;
    }

    //-Stealth Mod(positive like Master of Deception) and Stealth Detection(negative like paranoia)
    // based on wowwiki every 5 mod we have 1 more level diff in calculation
    visibleDistance += (int32(u->GetTotalAuraModifier(SPELL_AURA_MOD_STEALTH_DETECT)) - stealthMod) / 5.0f;
    visibleDistance = visibleDistance > MAX_PLAYER_STEALTH_DETECT_RANGE ? MAX_PLAYER_STEALTH_DETECT_RANGE : visibleDistance;

    // recheck new distance
    if (visibleDistance <= 0 || !IsWithinDist(viewPoint, visibleDistance))
    {
        return false;
    }

    // Now check is target visible with LoS
    float ox, oy, oz;
    viewPoint->GetPosition(ox, oy, oz);
    return IsWithinLOS(ox, oy, oz);
}

/**
 * @brief Refreshes visibility, viewpoint auras, and AI visibility notifications.
 */
void Unit::UpdateVisibilityAndView()
{

    static const AuraType auratypes[] = {SPELL_AURA_BIND_SIGHT, SPELL_AURA_FAR_SIGHT, SPELL_AURA_NONE};
    for (AuraType const* type = &auratypes[0]; *type != SPELL_AURA_NONE; ++type)
    {
        AuraList& alist = m_modAuras[*type];
        if (alist.empty())
        {
            continue;
        }

        for (AuraList::iterator it = alist.begin(); it != alist.end();)
        {
            Aura* aura = (*it);
            Unit* owner = aura->GetCaster();

            if (!owner || !IsVisibleForOrDetect(owner, this, false))
            {
                alist.erase(it);
                RemoveAura(aura);
                it = alist.begin();
            }
            else
            {
                ++it;
            }
        }
    }

    GetViewPoint().Call_UpdateVisibilityForOwner();
    UpdateObjectVisibility();
    ScheduleAINotify(0);
    GetViewPoint().Event_ViewPointVisibilityChanged();
}

/**
 * @brief Sets the unit visibility mode and refreshes visibility if needed.
 *
 * @param x The new visibility mode.
 */
void Unit::SetVisibility(UnitVisibility x)
{
    m_Visibility = x;

    if (IsInWorld())
    {
        UpdateVisibilityAndView();
    }
}
