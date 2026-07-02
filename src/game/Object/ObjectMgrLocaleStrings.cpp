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
#include "AuctionHouseBot/AhBotSystemOwner.h"
#include "LivingWorldAnchorPolicy.h"
#include "MotionGenerators/MotionMaster.h"
#include "SQLStorages.h"
#include "Log.h"
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
 * @brief Gets localized creature name and subname strings for a locale index.
 *
 * @param entry The creature entry id.
 * @param loc_idx The internal locale index.
 * @param namePtr Receives the localized name if available.
 * @param subnamePtr Receives the localized subname if available.
 */
void ObjectMgr::GetCreatureLocaleStrings(uint32 entry, int32 loc_idx, char const** namePtr, char const** subnamePtr) const
{
    if (loc_idx >= 0)
    {
        if (CreatureLocale const *il = GetCreatureLocale(entry))
        {
            if (namePtr && il->Name.size() > size_t(loc_idx) && !il->Name[loc_idx].empty())
            {
                *namePtr = il->Name[loc_idx].c_str();
            }

            if (subnamePtr && il->SubName.size() > size_t(loc_idx) && !il->SubName[loc_idx].empty())
            {
                *subnamePtr = il->SubName[loc_idx].c_str();
            }
        }
    }
}

/**
 * @brief Gets localized item name and description strings for a locale index.
 *
 * @param entry The item entry id.
 * @param loc_idx The internal locale index.
 * @param namePtr Receives the localized item name if available.
 * @param descriptionPtr Receives the localized description if available.
 */
void ObjectMgr::GetItemLocaleStrings(uint32 entry, int32 loc_idx, std::string* namePtr, std::string* descriptionPtr) const
{
    if (loc_idx >= 0)
    {
        if (ItemLocale const *il = GetItemLocale(entry))
        {
            if (namePtr && il->Name.size() > size_t(loc_idx) && !il->Name[loc_idx].empty())
            {
                *namePtr = il->Name[loc_idx];
            }

            if (descriptionPtr && il->Description.size() > size_t(loc_idx) && !il->Description[loc_idx].empty())
            {
                *descriptionPtr = il->Description[loc_idx];
            }
        }
    }
}

/**
 * @brief Gets the localized quest title for a locale index.
 *
 * @param entry The quest entry id.
 * @param loc_idx The internal locale index.
 * @param titlePtr Receives the localized title if available.
 */
void ObjectMgr::GetQuestLocaleStrings(uint32 entry, int32 loc_idx, std::string* titlePtr) const
{
    if (loc_idx >= 0)
    {
        if (QuestLocale const *il = GetQuestLocale(entry))
        {
            if (titlePtr && il->Title.size() > size_t(loc_idx) && !il->Title[loc_idx].empty())
            {
                *titlePtr = il->Title[loc_idx];
            }
        }
    }
}

/**
 * @brief Gets all localized npc text option strings for a locale index.
 *
 * @param entry The npc text entry id.
 * @param loc_idx The internal locale index.
 * @param text0_Ptr Receives the first text column array if available.
 * @param text1_Ptr Receives the second text column array if available.
 */
void ObjectMgr::GetNpcTextLocaleStringsAll(uint32 entry, int32 loc_idx, ObjectMgr::NpcTextArray* text0_Ptr, ObjectMgr::NpcTextArray* text1_Ptr) const
{
    if (loc_idx >= 0)
    {
        if (NpcTextLocale const *nl = GetNpcTextLocale(entry))
        {
            if (text0_Ptr)
            {
                for (int i = 0; i < MAX_GOSSIP_TEXT_OPTIONS; ++i)
                {
                    if (nl->Text_0[i].size() > (size_t)loc_idx && !nl->Text_0[i][loc_idx].empty())
                    {
                        (*text0_Ptr)[i] = nl->Text_0[i][loc_idx];
                    }
                }
            }

            if (text1_Ptr)
            {
                for (int i = 0; i < MAX_GOSSIP_TEXT_OPTIONS; ++i)
                {
                    if (nl->Text_1[i].size() > (size_t)loc_idx && !nl->Text_1[i][loc_idx].empty())
                    {
                        (*text1_Ptr)[i] = nl->Text_1[i][loc_idx];
                    }
                }
            }
        }
    }
}

/**
 * @brief Gets the first localized npc text option pair for a locale index.
 *
 * @param entry The npc text entry id.
 * @param loc_idx The internal locale index.
 * @param text0_0_Ptr Receives the first localized text string.
 * @param text1_0_Ptr Receives the second localized text string.
 */
void ObjectMgr::GetNpcTextLocaleStrings0(uint32 entry, int32 loc_idx, std::string* text0_0_Ptr, std::string* text1_0_Ptr) const
{
    if (loc_idx >= 0)
    {
        if (NpcTextLocale const *nl = GetNpcTextLocale(entry))
        {
            if (text0_0_Ptr)
            {
                if (nl->Text_0[0].size() > (size_t)loc_idx && !nl->Text_0[0][loc_idx].empty())
                {
                    *text0_0_Ptr = nl->Text_0[0][loc_idx];
                }
            }

            if (text1_0_Ptr)
            {
                if (nl->Text_1[0].size() > (size_t)loc_idx && !nl->Text_1[0][loc_idx].empty())
                {
                    *text1_0_Ptr = nl->Text_1[0][loc_idx];
                }
            }
        }
    }
}
