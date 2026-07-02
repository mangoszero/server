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



#include "ObjectMgr.h"
#include "Database/DatabaseEnv.h"
#include "Policies/Singleton.h"
#include "DBCStores.h"
#include "Log.h"
#include "AuctionHouseBot/AhBotSystemOwner.h"
#include "LivingWorldAnchorPolicy.h"
#include "MotionGenerators/MotionMaster.h"
#include "SQLStorages.h"
#include "MapManager.h"
#include "ObjectGuid.h"
#include "ScriptMgr.h"
#include "SpellMgr.h"
#include "World.h"
#include "Group.h"
#include "Transports.h"
#include "ProgressBar.h"
#include "Language.h"
#include "PoolManager.h"
#include "GameEventMgr.h"
#include "Chat.h"
#include "MapPersistentStateMgr.h"
#include "SpellAuras.h"
#include "Util.h"
#include "GossipDef.h"
#include "Mail.h"
#include "Formulas.h"
#include "InstanceData.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "DisableMgr.h"
#include "ItemEnchantmentMgr.h"

/**
 * @brief Finds the nearest reachable taxi node for a location and faction.
 *
 * @param x The world x coordinate.
 * @param y The world y coordinate.
 * @param z The world z coordinate.
 * @param mapid The map id.
 * @param team The player's faction.
 * @return The nearest taxi node id, or 0 if none matches.
 */
uint32 ObjectMgr::GetNearestTaxiNode(float x, float y, float z, uint32 mapid, Team team)
{
    bool found = false;
    float dist;
    uint32 id = 0;

    for (uint32 i = 1; i < sTaxiNodesStore.GetNumRows(); ++i)
    {
        TaxiNodesEntry const* node = sTaxiNodesStore.LookupEntry(i);
        if (!node || node->map_id != mapid || !node->MountCreatureID[team == ALLIANCE ? 1 : 0])
        {
            continue;
        }

        uint8  field   = (uint8)((i - 1) / 32);
        uint32 submask = 1 << ((i - 1) % 32);

        // skip not taxi network nodes
        if ((sTaxiNodesMask[field] & submask) == 0)
        {
            continue;
        }

        float dist2 = (node->x - x) * (node->x - x) + (node->y - y) * (node->y - y) + (node->z - z) * (node->z - z);
        if (found)
        {
            if (dist2 < dist)
            {
                dist = dist2;
                id = i;
            }
        }
        else
        {
            found = true;
            dist = dist2;
            id = i;
        }
    }

    return id;
}

/**
 * @brief Resolves the taxi path id and cost between two taxi nodes.
 *
 * @param source The source taxi node id.
 * @param destination The destination taxi node id.
 * @param path Receives the taxi path id.
 * @param cost Receives the travel cost.
 */
void ObjectMgr::GetTaxiPath(uint32 source, uint32 destination, uint32& path, uint32& cost)
{
    TaxiPathSetBySource::iterator src_i = sTaxiPathSetBySource.find(source);
    if (src_i == sTaxiPathSetBySource.end())
    {
        path = 0;
        cost = 0;
        return;
    }

    TaxiPathSetForSource& pathSet = src_i->second;

    TaxiPathSetForSource::iterator dest_i = pathSet.find(destination);
    if (dest_i == pathSet.end())
    {
        path = 0;
        cost = 0;
        return;
    }

    cost = dest_i->second.price;
    path = dest_i->second.ID;
}

/**
 * @brief Gets the taxi mount display id for a node and faction.
 *
 * @param id The taxi node id.
 * @param team The player's faction.
 * @param allowed_alt_team true to allow fallback to the opposite faction mount.
 * @return The creature display id used for the taxi mount, or 0 if unavailable.
 */
uint32 ObjectMgr::GetTaxiMountDisplayId(uint32 id, Team team, bool allowed_alt_team /* = false */)
{
    uint16 mount_entry = 0;

    // select mount creature id
    TaxiNodesEntry const* node = sTaxiNodesStore.LookupEntry(id);
    if (node)
    {
        if (team == ALLIANCE)
        {
            mount_entry = node->MountCreatureID[1];
            if (!mount_entry && allowed_alt_team)
            {
                mount_entry = node->MountCreatureID[0];
            }
        }
        else if (team == HORDE)
        {
            mount_entry = node->MountCreatureID[0];

            if (!mount_entry && allowed_alt_team)
            {
                mount_entry = node->MountCreatureID[1];
            }
        }
    }

    CreatureInfo const* mount_info = GetCreatureTemplate(mount_entry);
    if (!mount_info)
    {
        return 0;
    }

    uint16 mount_id = Creature::ChooseDisplayId(mount_info);
    if (!mount_id)
    {
        return 0;
    }

    CreatureModelInfo const* minfo = GetCreatureModelRandomGender(mount_id);
    if (minfo)
    {
        mount_id = minfo->modelid;
    }

    return mount_id;
}
