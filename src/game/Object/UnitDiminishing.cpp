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
 * @brief Gets the current diminishing return level for a control group.
 *
 * @param group The diminishing return group.
 * @return The current diminishing level.
 */
DiminishingLevels Unit::GetDiminishing(DiminishingGroup group)
{
    for (Diminishing::iterator i = m_Diminishing.begin(); i != m_Diminishing.end(); ++i)
    {
        if (i->DRGroup != group)
        {
            continue;
        }

        if (!i->hitCount)
        {
            return DIMINISHING_LEVEL_1;
        }

        if (!i->hitTime)
        {
            return DIMINISHING_LEVEL_1;
        }

        // If last spell was casted more than 15 seconds ago - reset the count.
        if (i->stack == 0 && getMSTimeDiff(i->hitTime, GameTime::GetGameTimeMS()) > 15 * IN_MILLISECONDS)
        {
            i->hitCount = DIMINISHING_LEVEL_1;
            return DIMINISHING_LEVEL_1;
        }
        // or else increase the count.
        else
        {
            return DiminishingLevels(i->hitCount);
        }
    }
    return DIMINISHING_LEVEL_1;
}

/**
 * @brief Increments the diminishing return counter for a control group.
 *
 * @param group The diminishing return group.
 */
void Unit::IncrDiminishing(DiminishingGroup group)
{
    // Checking for existing in the table
    for (Diminishing::iterator i = m_Diminishing.begin(); i != m_Diminishing.end(); ++i)
    {
        if (i->DRGroup != group)
        {
            continue;
        }
        if (i->hitCount < DIMINISHING_LEVEL_IMMUNE)
        {
            i->hitCount += 1;
        }
        return;
    }
    m_Diminishing.push_back(DiminishingReturn(group, GameTime::GetGameTimeMS(), DIMINISHING_LEVEL_2));
}

/**
 * @brief Applies diminishing return rules to a control effect duration.
 *
 * @param group The diminishing return group.
 * @param duration The mutable duration in milliseconds.
 * @param caster The caster applying the effect.
 * @param Level The diminishing level being applied.
 * @param isReflected True when the effect was reflected.
 */
void Unit::ApplyDiminishingToDuration(DiminishingGroup group, int32& duration, Unit* caster, DiminishingLevels Level, bool isReflected)
{
    if (duration == -1 || group == DIMINISHING_NONE || (!isReflected && caster->IsFriendlyTo(this)))
    {
        return;
    }

    float mod = 1.0f;

    // Some diminishings applies to mobs too (for example, Stun)
    if ((GetDiminishingReturnsGroupType(group) == DRTYPE_PLAYER && GetTypeId() == TYPEID_PLAYER) || GetDiminishingReturnsGroupType(group) == DRTYPE_ALL)
    {
        DiminishingLevels diminish = Level;
        switch (diminish)
        {
            case DIMINISHING_LEVEL_1: break;
            case DIMINISHING_LEVEL_2: mod = 0.5f; break;
            case DIMINISHING_LEVEL_3: mod = 0.25f; break;
            case DIMINISHING_LEVEL_IMMUNE: mod = 0.0f; break;
            default: break;
        }
    }

    duration = int32(duration * mod);
}

/**
 * @brief Updates active stack tracking for a diminishing return group.
 *
 * @param group The diminishing return group.
 * @param apply True when an aura from the group is applied; false when it is removed.
 */
void Unit::ApplyDiminishingAura(DiminishingGroup group, bool apply)
{
    // Checking for existing in the table
    for (Diminishing::iterator i = m_Diminishing.begin(); i != m_Diminishing.end(); ++i)
    {
        if (i->DRGroup != group)
        {
            continue;
        }

        if (apply)
        {
            i->stack += 1;
        }
        else if (i->stack)
        {
            i->stack -= 1;
            // Remember time after last aura from group removed
            if (i->stack == 0)
            {
                i->hitTime = GameTime::GetGameTimeMS();
            }
        }
        break;
    }
}
