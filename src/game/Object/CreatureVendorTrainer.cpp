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



#include "Creature.h"
#include "ObjectMgr.h"
#include "LivingWorldAnchorPolicy.h"
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "World.h"
#include "ScriptMgr.h"
#include "ObjectGuid.h"
#include "SQLStorages.h"
#include "SpellMgr.h"
#include "GossipDef.h"
#include "Player.h"
#include "GameEventMgr.h"
#include "PoolManager.h"
#include "Opcodes.h"
#include "Log.h"
#include "LootMgr.h"
#include "MapManager.h"
#include "CreatureAI.h"
#include "CreatureAISelector.h"
#include "InstanceData.h"
#include "MapPersistentStateMgr.h"
#include "BattleGround/BattleGroundMgr.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "Spell.h"
#include "Util.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "movement/MoveSplineInit.h"
#include "CreatureLinkingMgr.h"
#include "DisableMgr.h"
#include "MovementGenerator.h"
#include "Policies/Singleton.h"
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */

/**
 * @brief Gets vendor items directly assigned to this creature entry.
 *
 * @return The vendor item list, or null if none exists.
 */
VendorItemData const* Creature::GetVendorItems() const
{
    return sObjectMgr.GetNpcVendorItemList(GetEntry());
}

/**
 * @brief Gets vendor items from this creature's vendor template.
 *
 * @return The vendor template item list, or null if none exists.
 */
VendorItemData const* Creature::GetVendorTemplateItems() const
{
    uint32 VendorTemplateId = GetCreatureInfo()->VendorTemplateId;
    return VendorTemplateId ? sObjectMgr.GetNpcVendorTemplateItemList(VendorTemplateId) : NULL;
}

/**
 * @brief Gets the current available stock count for a vendor item.
 *
 * @param vItem The vendor item definition.
 * @return The currently available count.
 */
uint32 Creature::GetVendorItemCurrentCount(VendorItem const* vItem)
{
    if (!vItem->maxcount)
    {
        return vItem->maxcount;
    }

    VendorItemCounts::iterator itr = m_vendorItemCounts.begin();
    for (; itr != m_vendorItemCounts.end(); ++itr)
    {
        if (itr->itemId == vItem->item)
        {
            break;
        }
    }

    if (itr == m_vendorItemCounts.end())
    {
        return vItem->maxcount;
    }

    VendorItemCount* vCount = &*itr;

    time_t ptime = time(NULL);

    if (vCount->lastIncrementTime + vItem->incrtime <= ptime)
    {
        ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(vItem->item);

        uint32 diff = uint32((ptime - vCount->lastIncrementTime) / vItem->incrtime);
        if ((vCount->count + diff * pProto->BuyCount) >= vItem->maxcount)
        {
            m_vendorItemCounts.erase(itr);
            return vItem->maxcount;
        }

        vCount->count += diff * pProto->BuyCount;
        vCount->lastIncrementTime = ptime;
    }

    return vCount->count;
}

/**
 * @brief Updates and consumes stock count for a limited vendor item.
 *
 * @param vItem The vendor item definition.
 * @param used_count The amount being purchased.
 * @return The remaining count after the update.
 */
uint32 Creature::UpdateVendorItemCurrentCount(VendorItem const* vItem, uint32 used_count)
{
    if (!vItem->maxcount)
    {
        return 0;
    }

    VendorItemCounts::iterator itr = m_vendorItemCounts.begin();
    for (; itr != m_vendorItemCounts.end(); ++itr)
    {
        if (itr->itemId == vItem->item)
        {
            break;
        }
    }

    if (itr == m_vendorItemCounts.end())
    {
        uint32 new_count = vItem->maxcount > used_count ? vItem->maxcount - used_count : 0;
        m_vendorItemCounts.push_back(VendorItemCount(vItem->item, new_count));
        return new_count;
    }

    VendorItemCount* vCount = &*itr;

    time_t ptime = time(NULL);

    if (vCount->lastIncrementTime + vItem->incrtime <= ptime)
    {
        ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(vItem->item);

        uint32 diff = uint32((ptime - vCount->lastIncrementTime) / vItem->incrtime);
        if ((vCount->count + diff * pProto->BuyCount) < vItem->maxcount)
        {
            vCount->count += diff * pProto->BuyCount;
        }
        else
        {
            vCount->count = vItem->maxcount;
        }
    }

    vCount->count = vCount->count > used_count ? vCount->count - used_count : 0;
    vCount->lastIncrementTime = ptime;
    return vCount->count;
}

/**
 * @brief Gets trainer spells from the trainer template bound to this creature.
 *
 * @return The trainer template spell list, or null if none exists.
 */
TrainerSpellData const* Creature::GetTrainerTemplateSpells() const
{
    uint32 TrainerTemplateId = GetCreatureInfo()->TrainerTemplateId;
    return TrainerTemplateId ? sObjectMgr.GetNpcTrainerTemplateSpells(TrainerTemplateId) : NULL;
}

/**
 * @brief Gets trainer spells directly assigned to this creature entry.
 *
 * @return The trainer spell list, or null if none exists.
 */
TrainerSpellData const* Creature::GetTrainerSpells() const
{
    return sObjectMgr.GetNpcTrainerSpells(GetEntry());
}
