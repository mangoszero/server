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

/**
 * @file InstanceData.cpp
 * @brief Instance script data persistence base implementation
 *
 * This file provides the base implementation for saving/loading instance
 * script data. InstanceData is the abstract base class that all dungeon/raid
 * scripts inherit from to persist state (boss kills, encounters, etc.) to the
 * database.
 *
 * Data is stored in either:
 * - `instance` table for dungeons/raids (instanced maps)
 * - `world` table for continent/world state (non-instanced maps)
 *
 * @see InstanceData for the abstract interface
 * @see Map for the owning map instance
 */

#include "InstanceData.h"
#include "Database/DatabaseEnv.h"
#include "Map.h"

/**
 * @brief Save instance state to database
 *
 * Persists the instance script data to the appropriate database table.
 * Skips saving for:
 * - Battlegrounds/Arenas (no persistence needed)
 * - Instances with no data to save (Save() returns empty string)
 *
 * Data is escaped before storage to prevent SQL injection.
 *
 * @note Called periodically and on instance shutdown
 */
void InstanceData::SaveToDB() const
{
    // No reason to save BGs/Arenas
    if (instance->IsBattleGround())
    {
        return;
    }

    if (!Save())
    {
        return;
    }

    std::string data = Save();
    CharacterDatabase.escape_string(data);

    if (instance->Instanceable())
    {
        CharacterDatabase.PExecute("UPDATE `instance` SET `data` = '%s' WHERE `id` = '%u'", data.c_str(), instance->GetInstanceId());
    }
    else
    {
        CharacterDatabase.PExecute("UPDATE `world` SET `data` = '%s' WHERE `map` = '%u'", data.c_str(), instance->GetId());
    }
}

/**
 * @brief Check if instance condition criteria are met
 * @param source Player to check conditions for
 * @param instance_condition_id Condition identifier from database
 * @param conditionSource WorldObject triggering the condition check
 * @param conditionSourceType Type of the condition source
 * @return true if conditions are met, false otherwise
 *
 * Base implementation logs an error and returns false. Instance scripts
 * should override this to implement custom condition checking logic for
 * access requirements, quests, or other instance-specific conditions.
 *
 * @warning Derived classes must override this for condition system support
 */
bool InstanceData::CheckConditionCriteriaMeet(Player const* /*source*/, uint32 instance_condition_id, WorldObject const* /*conditionSource*/, uint32 conditionSourceType) const
{
    sLog.outError("Condition system call InstanceData::CheckConditionCriteriaMeet but instance script for map %u not have implementation for player condition criteria with internal id %u (called from %u)",
                  instance->GetId(), instance_condition_id, uint32(conditionSourceType));
    return false;
}
