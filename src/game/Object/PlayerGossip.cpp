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



#include "Player.h"
#include "Language.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "Opcodes.h"
#include "SpellMgr.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "UpdateMask.h"
#include "QuestDef.h"
#include "GossipDef.h"
#include "UpdateData.h"
#include "Channel.h"
#include "ChannelMgr.h"
#include "MapManager.h"
#include "MapPersistentStateMgr.h"
#include "InstanceData.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "ObjectMgr.h"
#include "ObjectAccessor.h"
#include "CreatureAI.h"
#include "Formulas.h"
#include "Group.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Pet.h"
#include "Util.h"
#include "Transports.h"
#include "Weather.h"
#include "BattleGround/BattleGround.h"
#include "BattleGround/BattleGroundMgr.h"
#include "BattleGround/BattleGroundAV.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "Chat.h"
#include "revision_data.h"
#include "Database/DatabaseImpl.h"
#include "Spell.h"
#include "ScriptMgr.h"
#include "SocialMgr.h"
#include "Mail.h"
#include "SpellAuras.h"
#include "DBCStores.h"
#include "SQLStorages.h"
#include "DisableMgr.h"
#include "CinematicFlyover.h"
#include <cmath>
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */
#ifdef ENABLE_PLAYERBOTS
#include "playerbot.h"
#endif /* ENABLE_PLAYERBOTS */

/**
 * @brief Builds the current gossip menu for a source object.
 *
 * @param pSource The gossip source object.
 * @param menuId The gossip menu identifier to prepare.
 */
void Player::PrepareGossipMenu(WorldObject* pSource, uint32 menuId)
{
    PlayerMenu* pMenu = PlayerTalkClass;
    pMenu->ClearMenus();

    pMenu->GetGossipMenu().SetMenuId(menuId);

    GossipMenuItemsMapBounds pMenuItemBounds = sObjectMgr.GetGossipMenuItemsMapBounds(menuId);

    // prepares quest menu when true
    bool canSeeQuests = menuId == GetDefaultGossipMenuForSource(pSource);

    // if canSeeQuests (the default, top level menu) and no menu options exist for this, use options from default options
    if (pMenuItemBounds.first == pMenuItemBounds.second && canSeeQuests)
    {
        pMenuItemBounds = sObjectMgr.GetGossipMenuItemsMapBounds(0);
    }

    for (GossipMenuItemsMap::const_iterator itr = pMenuItemBounds.first; itr != pMenuItemBounds.second; ++itr)
    {
        GossipMenuItems const& gossipMenu = itr->second;
        bool hasMenuItem = true;
        bool isGMSkipConditionCheck = false;

        if (gossipMenu.conditionId && !sObjectMgr.IsPlayerMeetToCondition(gossipMenu.conditionId, this, GetMap(), pSource, CONDITION_FROM_GOSSIP_OPTION))
        {
            if (isGameMaster())                             // Let GM always see menu items regardless of conditions
            {
                isGMSkipConditionCheck = true;
            }
            else
            {
                if (gossipMenu.option_id == GOSSIP_OPTION_QUESTGIVER)
                {
                    canSeeQuests = false;
                }
                continue;                                   // Skip this option
            }
        }

        if (pSource->GetTypeId() == TYPEID_UNIT)
        {
            Creature* pCreature = (Creature*)pSource;

            uint32 npcflags = pCreature->GetUInt32Value(UNIT_NPC_FLAGS);

            if (!(gossipMenu.npc_option_npcflag & npcflags))
            {
                continue;
            }

            switch (gossipMenu.option_id)
            {
                case GOSSIP_OPTION_GOSSIP:
                    break;
                case GOSSIP_OPTION_QUESTGIVER:
                    hasMenuItem = false;
                    break;
                case GOSSIP_OPTION_ARMORER:
                    hasMenuItem = false;                    // added in special mode
                    break;
                case GOSSIP_OPTION_SPIRITHEALER:
                    if (!IsDead())
                    {
                        hasMenuItem = false;
                    }
                    break;
                case GOSSIP_OPTION_VENDOR:
                {
                    VendorItemData const* vItems = pCreature->GetVendorItems();
                    VendorItemData const* tItems = pCreature->GetVendorTemplateItems();
                    if ((!vItems || vItems->Empty()) && (!tItems || tItems->Empty()))
                    {
                        sLog.outErrorDb("Creature %u (Entry: %u) have UNIT_NPC_FLAG_VENDOR but have empty trading item list.", pCreature->GetGUIDLow(), pCreature->GetEntry());
                        hasMenuItem = false;
                    }
                    break;
                }
                case GOSSIP_OPTION_TRAINER:
                    if (!pCreature->IsTrainerOf(this, false))
                    {
                        hasMenuItem = false;
                    }
                    break;
                case GOSSIP_OPTION_UNLEARNTALENTS:
                    if (!pCreature->CanTrainAndResetTalentsOf(this))
                    {
                        hasMenuItem = false;
                    }
                    break;
                case GOSSIP_OPTION_UNLEARNPETSKILLS:
                    if (!GetPet() || GetPet()->getPetType() != HUNTER_PET || GetPet()->m_spells.size() <= 1 || pCreature->GetCreatureInfo()->TrainerType != TRAINER_TYPE_PETS || pCreature->GetCreatureInfo()->TrainerClass != CLASS_HUNTER)
                    {
                        hasMenuItem = false;
                    }
                    break;
                case GOSSIP_OPTION_TAXIVENDOR:
                    if (GetSession()->SendLearnNewTaxiNode(pCreature))
                    {
                        return;
                    }
                    break;
                case GOSSIP_OPTION_BATTLEFIELD:
                    if (!pCreature->CanInteractWithBattleMaster(this, false))
                    {
                        hasMenuItem = false;
                    }
                    break;
                case GOSSIP_OPTION_STABLEPET:
                    if (getClass() != CLASS_HUNTER)
                    {
                        hasMenuItem = false;
                    }
                    break;
                case GOSSIP_OPTION_SPIRITGUIDE:
                case GOSSIP_OPTION_INNKEEPER:
                case GOSSIP_OPTION_BANKER:
                case GOSSIP_OPTION_PETITIONER:
                case GOSSIP_OPTION_TABARDDESIGNER:
                case GOSSIP_OPTION_AUCTIONEER:
                    break;                                  // no checks
                default:
                    sLog.outErrorDb("Creature entry %u have unknown gossip option %u for menu %u", pCreature->GetEntry(), gossipMenu.option_id, gossipMenu.menu_id);
                    hasMenuItem = false;
                    break;
            }
        }
        else if (pSource->GetTypeId() == TYPEID_GAMEOBJECT)
        {
            GameObject* pGo = (GameObject*)pSource;

            switch (gossipMenu.option_id)
            {
                case GOSSIP_OPTION_QUESTGIVER:
                    hasMenuItem = false;
                    break;
                case GOSSIP_OPTION_GOSSIP:
                    if (pGo->GetGoType() != GAMEOBJECT_TYPE_QUESTGIVER && pGo->GetGoType() != GAMEOBJECT_TYPE_GOOBER)
                    {
                        hasMenuItem = false;
                    }
                    break;
                default:
                    hasMenuItem = false;
                    break;
            }
        }

        if (hasMenuItem)
        {
            std::string strOptionText = gossipMenu.option_text;
            std::string strBoxText = gossipMenu.box_text;

            int loc_idx = GetSession()->GetSessionDbLocaleIndex();

            if (loc_idx >= 0)
            {
                uint32 idxEntry = MAKE_PAIR32(menuId, gossipMenu.id);

                if (GossipMenuItemsLocale const* no = sObjectMgr.GetGossipMenuItemsLocale(idxEntry))
                {
                    if (no->OptionText.size() > (size_t)loc_idx && !no->OptionText[loc_idx].empty())
                    {
                        strOptionText = no->OptionText[loc_idx];
                    }

                    if (no->BoxText.size() > (size_t)loc_idx && !no->BoxText[loc_idx].empty())
                    {
                        strBoxText = no->BoxText[loc_idx];
                    }
                }
            }

            if (isGMSkipConditionCheck)
            {
                strOptionText.append(" (");
                strOptionText.append(GetSession()->GetMangosString(LANG_GM_ON));
                strOptionText.append(")");
            }

            pMenu->GetGossipMenu().AddMenuItem(gossipMenu.option_icon, strOptionText, 0, gossipMenu.option_id, strBoxText, gossipMenu.box_coded);
            pMenu->GetGossipMenu().AddMenuItemData(gossipMenu.action_menu_id, gossipMenu.action_poi_id, gossipMenu.action_script_id);
        }
    }

    if (canSeeQuests)
    {
        PrepareQuestMenu(pSource->GetObjectGuid());
    }

    // some gossips aren't handled in normal way ... so we need to do it this way .. TODO: handle it in normal way ;-)
    /*if (pMenu->Empty())
    {
        if (pCreature->HasFlag(UNIT_NPC_FLAGS,UNIT_NPC_FLAG_TRAINER))
        {
            // output error message if need
            pCreature->IsTrainerOf(this, true);
        }

        if (pCreature->HasFlag(UNIT_NPC_FLAGS,UNIT_NPC_FLAG_BATTLEMASTER))
        {
            // output error message if need
            pCreature->CanInteractWithBattleMaster(this, true);
        }
    }*/
}

/**
 * @brief Sends the prepared gossip or quest menu for a source object.
 *
 * @param pSource The source object whose prepared menu should be sent.
 */
void Player::SendPreparedGossip(WorldObject* pSource)
{
    if (!pSource)
    {
        return;
    }

    if (pSource->GetTypeId() == TYPEID_UNIT)
    {
        // in case no gossip flag and quest menu not empty, open quest menu (client expect gossip menu with this flag)
        if (!((Creature*)pSource)->HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP) && !PlayerTalkClass->GetQuestMenu().Empty())
        {
            SendPreparedQuest(pSource->GetObjectGuid());
            return;
        }
    }
    else if (pSource->GetTypeId() == TYPEID_GAMEOBJECT)
    {
        // probably need to find a better way here
        if (!PlayerTalkClass->GetGossipMenu().GetMenuId() && !PlayerTalkClass->GetQuestMenu().Empty())
        {
            SendPreparedQuest(pSource->GetObjectGuid());
            return;
        }
    }

    // in case non empty gossip menu (that not included quests list size) show it
    // (quest entries from quest menu will be included in list)

    uint32 textId = GetGossipTextId(pSource);

    if (uint32 menuId = PlayerTalkClass->GetGossipMenu().GetMenuId())
    {
        textId = GetGossipTextId(menuId, pSource);
    }

    PlayerTalkClass->SendGossipMenu(textId, pSource->GetObjectGuid());
}

/**
 * @brief Handles selection of a gossip menu option.
 *
 * @param pSource The gossip source object.
 * @param gossipListId The selected menu item index.
 */
void Player::OnGossipSelect(WorldObject* pSource, uint32 gossipListId)
{
    GossipMenu& gossipmenu = PlayerTalkClass->GetGossipMenu();

    if (gossipListId >= gossipmenu.MenuItemCount())
    {
        return;
    }

    GossipMenuItem const&  menu_item = gossipmenu.GetItem(gossipListId);

    uint32 gossipOptionId = menu_item.m_gOptionId;
    ObjectGuid guid = pSource->GetObjectGuid();

    if (pSource->GetTypeId() == TYPEID_GAMEOBJECT)
    {
        if (gossipOptionId > GOSSIP_OPTION_QUESTGIVER)
        {
            sLog.outError("Player guid %u request invalid gossip option for GameObject entry %u", GetGUIDLow(), pSource->GetEntry());
            return;
        }
    }

    GossipMenuItemData const* pMenuData = gossipmenu.GetItemData(gossipListId);
    GossipMenuItemData menuData = {};

    // if pMenuData exist we need to keep a copy of actual data for the following code to process
    // call like PrepareGossipMenu or SendPreparedGossip might change the value
    if (pMenuData)
    {
        menuData = *pMenuData;
    }

    switch (gossipOptionId)
    {
        case GOSSIP_OPTION_GOSSIP:
        {
            if (menuData.m_gAction_poi)
            {
                PlayerTalkClass->SendPointOfInterest(menuData.m_gAction_poi);
            }

            // send new menu || close gossip || stay at current menu
            if (menuData.m_gAction_menu > 0)
            {
                PrepareGossipMenu(pSource, uint32(menuData.m_gAction_menu));
                SendPreparedGossip(pSource);
            }
            else if (menuData.m_gAction_menu < 0)
            {
                PlayerTalkClass->CloseGossip();
                TalkedToCreature(pSource->GetEntry(), pSource->GetObjectGuid());
            }

            break;
        }
        case GOSSIP_OPTION_SPIRITHEALER:
            if (IsDead())
            {
                ((Creature*)pSource)->CastSpell(((Creature*)pSource), 17251, true, NULL, NULL, GetObjectGuid());
            }
            break;
        case GOSSIP_OPTION_QUESTGIVER:
            PrepareQuestMenu(guid);
            SendPreparedQuest(guid);
            break;
        case GOSSIP_OPTION_VENDOR:
        case GOSSIP_OPTION_ARMORER:
            GetSession()->SendListInventory(guid);
            break;
        case GOSSIP_OPTION_STABLEPET:
            GetSession()->SendStablePet(guid);
            break;
        case GOSSIP_OPTION_TRAINER:
            GetSession()->SendTrainerList(guid);
            break;
        case GOSSIP_OPTION_UNLEARNTALENTS:
            PlayerTalkClass->CloseGossip();
            SendTalentWipeConfirm(guid);
            break;
        case GOSSIP_OPTION_UNLEARNPETSKILLS:
            PlayerTalkClass->CloseGossip();
            SendPetSkillWipeConfirm();
            break;
        case GOSSIP_OPTION_TAXIVENDOR:
            GetSession()->SendTaxiMenu(((Creature*)pSource));
            break;
        case GOSSIP_OPTION_INNKEEPER:
            PlayerTalkClass->CloseGossip();
            SetBindPoint(guid);
            break;
        case GOSSIP_OPTION_BANKER:
            GetSession()->SendShowBank(guid);
            break;
        case GOSSIP_OPTION_PETITIONER:
            PlayerTalkClass->CloseGossip();
            GetSession()->SendPetitionShowList(guid);
            break;
        case GOSSIP_OPTION_TABARDDESIGNER:
            PlayerTalkClass->CloseGossip();
            GetSession()->SendTabardVendorActivate(guid);
            break;
        case GOSSIP_OPTION_AUCTIONEER:
            GetSession()->SendAuctionHello(((Creature*)pSource));
            break;
        case GOSSIP_OPTION_SPIRITGUIDE:
            PrepareGossipMenu(pSource);
            SendPreparedGossip(pSource);
            break;
        case GOSSIP_OPTION_BATTLEFIELD:
        {
            BattleGroundTypeId bgTypeId = sBattleGroundMgr.GetBattleMasterBG(pSource->GetEntry());

            if (bgTypeId == BATTLEGROUND_TYPE_NONE)
            {
                sLog.outError("a user (guid %u) requested battlegroundlist from a npc who is no battlemaster", GetGUIDLow());
                return;
            }

            GetSession()->SendBattlegGroundList(guid, bgTypeId);
            break;
        }
    }

    if (pMenuData && menuData.m_gAction_script)
    {
        if (pSource->GetTypeId() == TYPEID_UNIT)
        {
            GetMap()->ScriptsStart(DBS_ON_GOSSIP, menuData.m_gAction_script, pSource, this, Map::SCRIPT_EXEC_PARAM_UNIQUE_BY_SOURCE);
        }
        else if (pSource->GetTypeId() == TYPEID_GAMEOBJECT)
        {
            GetMap()->ScriptsStart(DBS_ON_GOSSIP, menuData.m_gAction_script, this, pSource, Map::SCRIPT_EXEC_PARAM_UNIQUE_BY_TARGET);
        }
    }
}

/**
 * @brief Gets the default gossip text identifier for a source object.
 *
 * @param pSource The gossip source object.
 * @return The gossip text identifier to display.
 */
uint32 Player::GetGossipTextId(WorldObject* pSource)
{
    if (!pSource || pSource->GetTypeId() != TYPEID_UNIT)
    {
        return DEFAULT_GOSSIP_MESSAGE;
    }

    if (uint32 pos = sObjectMgr.GetNpcGossipTextId(((Creature*)pSource)->GetEntry()))
    {
        return pos;
    }

    return DEFAULT_GOSSIP_MESSAGE;
}

/**
 * @brief Resolves the gossip text identifier for a specific gossip menu.
 *
 * @param menuId The gossip menu identifier.
 * @param pSource The gossip source object.
 * @return The resolved gossip text identifier.
 */
uint32 Player::GetGossipTextId(uint32 menuId, WorldObject* pSource)
{
    uint32 textId = DEFAULT_GOSSIP_MESSAGE;

    if (!menuId)
    {
        return textId;
    }

    uint32 scriptId = 0;
    uint32 lastConditionId = 0;

    GossipMenusMapBounds pMenuBounds = sObjectMgr.GetGossipMenusMapBounds(menuId);
    for (GossipMenusMap::const_iterator itr = pMenuBounds.first; itr != pMenuBounds.second; ++itr)
    {
        GossipMenus const& gossipMenu = itr->second;
        // Take the text that has the highest conditionId of all fitting
        // No condition and no text with condition found OR higher and fitting condition found
        if ((!gossipMenu.conditionId && !lastConditionId) ||
            (gossipMenu.conditionId > lastConditionId && sObjectMgr.IsPlayerMeetToCondition(gossipMenu.conditionId, this, GetMap(), pSource, CONDITION_FROM_GOSSIP_MENU)))
        {
            lastConditionId = gossipMenu.conditionId;
            textId = gossipMenu.text_id;
            scriptId = gossipMenu.script_id;
        }
    }

    // Start related script
    if (scriptId)
    {
        GetMap()->ScriptsStart(DBS_ON_GOSSIP, scriptId, this, pSource, Map::SCRIPT_EXEC_PARAM_UNIQUE_BY_TARGET);
    }

    return textId;
}

/**
 * @brief Gets the default gossip menu identifier for a source object.
 *
 * @param pSource The gossip source object.
 * @return The default gossip menu identifier, or zero if none exists.
 */
uint32 Player::GetDefaultGossipMenuForSource(WorldObject* pSource)
{
    if (pSource->GetTypeId() == TYPEID_UNIT)
    {
        return ((Creature*)pSource)->GetCreatureInfo()->GossipMenuId;
    }
    else if (pSource->GetTypeId() == TYPEID_GAMEOBJECT)
    {
        return((GameObject*)pSource)->GetGOInfo()->GetGossipMenuId();
    }

    return 0;
}
