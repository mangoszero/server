/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2020 MaNGOS <https://getmangos.eu>
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

#include "InstanceData.h"
#include "Database/DatabaseEnv.h"
#include "Map.h"

void InstanceData::SaveToDB() const
{
    // no reason to save BGs/Arenas
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

bool InstanceData::CheckConditionCriteriaMeet(Player const* /*source*/, uint32 instance_condition_id, WorldObject const* /*conditionSource*/, uint32 conditionSourceType) const
{
    sLog.outError("Condition system call InstanceData::CheckConditionCriteriaMeet but instance script for map %u not have implementation for player condition criteria with internal id %u (called from %u)",
                  instance->GetId(), instance_condition_id, uint32(conditionSourceType));
    return false;
}
