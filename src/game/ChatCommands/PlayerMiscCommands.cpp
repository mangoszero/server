/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2022 MaNGOS <https://getmangos.eu>
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

#include "Chat.h"
#include "Language.h"
#include "World.h"
#include "Mail.h"

 /**********************************************************************
     CommandTable : commandTable
 /***********************************************************************/

enum
{
    EARTH_STONE_ITEM = 6948,
};

bool ChatHandler::HandleBankCommand(char* /*args*/)
{
    m_session->SendShowBank(m_session->GetPlayer()->GetObjectGuid());

    return true;
}

bool ChatHandler::HandleStableCommand(char* /*args*/)
{
    m_session->SendStablePet(m_session->GetPlayer()->GetObjectGuid());

    return true;
}

/**********************************************************************
    CommandTable : resetCommandTable
/***********************************************************************/

static bool HandleResetStatsOrLevelHelper(Player* player)
{
    ChrClassesEntry const* cEntry = sChrClassesStore.LookupEntry(player->getClass());
    if (!cEntry)
    {
        sLog.outError("Class %u not found in DBC (Wrong DBC files?)", player->getClass());
        return false;
    }

    uint8 powertype = cEntry->powerType;

    // reset m_form if no aura
    if (!player->HasAuraType(SPELL_AURA_MOD_SHAPESHIFT))
    {
        player->SetShapeshiftForm(FORM_NONE);
    }

    player->SetFloatValue(UNIT_FIELD_BOUNDINGRADIUS, DEFAULT_WORLD_OBJECT_SIZE);
    player->SetFloatValue(UNIT_FIELD_COMBATREACH, 1.5f);

    player->setFactionForRace(player->getRace());

    player->SetByteValue(UNIT_FIELD_BYTES_0, 3, powertype);

    // reset only if player not in some form;
    if (player->GetShapeshiftForm() == FORM_NONE)
    {
        player->InitDisplayIds();
    }

    // is it need, only in pre-2.x used and field byte removed later?
    if (powertype == POWER_RAGE || powertype == POWER_MANA)
    {
        player->SetByteValue(UNIT_FIELD_BYTES_1, 1, 0xEE);
    }

    player->SetByteValue(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_UNK3 | UNIT_BYTE2_FLAG_UNK5);

    player->SetUInt32Value(UNIT_FIELD_FLAGS, UNIT_FLAG_PVP_ATTACKABLE);

    //-1 is default value
    player->SetInt32Value(PLAYER_FIELD_WATCHED_FACTION_INDEX, -1);

    // player->SetUInt32Value(PLAYER_FIELD_BYTES, 0xEEE00000 );
    return true;
}

bool ChatHandler::HandleResetLevelCommand(char* args)
{
    Player* target;
    if (!ExtractPlayerTarget(&args, &target))
    {
        return false;
    }

    if (!HandleResetStatsOrLevelHelper(target))
    {
        return false;
    }

    // set starting level
    uint32 start_level = sWorld.getConfig(CONFIG_UINT32_START_PLAYER_LEVEL);

    target->SetLevel(start_level);
    target->InitStatsForLevel(true);
    target->InitTaxiNodes();
    target->InitTalentForLevel();
    target->SetUInt32Value(PLAYER_XP, 0);

    // reset level for pet
    if (Pet* pet = target->GetPet())
    {
        pet->SynchronizeLevelWithOwner();
    }

    return true;
}

bool ChatHandler::HandleResetStatsCommand(char* args)
{
    Player* target;
    if (!ExtractPlayerTarget(&args, &target))
    {
        return false;
    }

    if (!HandleResetStatsOrLevelHelper(target))
    {
        return false;
    }

    target->InitStatsForLevel(true);
    target->InitTalentForLevel();

    return true;
}

bool ChatHandler::HandleResetSpellsCommand(char* args)
{
    Player* target;
    ObjectGuid target_guid;
    std::string target_name;
    if (!ExtractPlayerTarget(&args, &target, &target_guid, &target_name))
    {
        return false;
    }

    if (target)
    {
        target->resetSpells();

        ChatHandler(target).SendSysMessage(LANG_RESET_SPELLS);
        if (!m_session || m_session->GetPlayer() != target)
        {
            PSendSysMessage(LANG_RESET_SPELLS_ONLINE, GetNameLink(target).c_str());
        }
    }
    else
    {
        CharacterDatabase.PExecute("UPDATE `characters` SET `at_login` = `at_login` | '%u' WHERE `guid` = '%u'", uint32(AT_LOGIN_RESET_SPELLS), target_guid.GetCounter());
        PSendSysMessage(LANG_RESET_SPELLS_OFFLINE, target_name.c_str());
    }

    return true;
}

bool ChatHandler::HandleResetTalentsCommand(char* args)
{
    Player* target;
    ObjectGuid target_guid;
    std::string target_name;
    if (!ExtractPlayerTarget(&args, &target, &target_guid, &target_name))
    {
        return false;
    }

    if (target)
    {
        target->resetTalents(true);

        ChatHandler(target).SendSysMessage(LANG_RESET_TALENTS);
        if (!m_session || m_session->GetPlayer() != target)
        {
            PSendSysMessage(LANG_RESET_TALENTS_ONLINE, GetNameLink(target).c_str());
        }
        return true;
    }
    else if (target_guid)
    {
        uint32 at_flags = AT_LOGIN_RESET_TALENTS;
        CharacterDatabase.PExecute("UPDATE `characters` SET `at_login` = `at_login` | '%u' WHERE `guid` = '%u'", at_flags, target_guid.GetCounter());
        std::string nameLink = playerLink(target_name);
        PSendSysMessage(LANG_RESET_TALENTS_OFFLINE, nameLink.c_str());
        return true;
    }

    SendSysMessage(LANG_NO_CHAR_SELECTED);
    SetSentErrorMessage(true);
    return false;
}

bool ChatHandler::HandleResetAllCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    std::string casename = args;

    AtLoginFlags atLogin;

    // Command specially created as single command to prevent using short case names
    if (casename == "spells")
    {
        atLogin = AT_LOGIN_RESET_SPELLS;
        sWorld.SendWorldText(LANG_RESETALL_SPELLS);
        if (!m_session)
        {
            SendSysMessage(LANG_RESETALL_SPELLS);
        }
    }
    else if (casename == "talents")
    {
        atLogin = AT_LOGIN_RESET_TALENTS;
        sWorld.SendWorldText(LANG_RESETALL_TALENTS);
        if (!m_session)
        {
            SendSysMessage(LANG_RESETALL_TALENTS);
        }
    }
    else
    {
        PSendSysMessage(LANG_RESETALL_UNKNOWN_CASE, args);
        SetSentErrorMessage(true);
        return false;
    }

    CharacterDatabase.PExecute("UPDATE `characters` SET `at_login` = `at_login` | '%u' WHERE (`at_login` & '%u') = '0'", atLogin, atLogin);
    sObjectAccessor.DoForAllPlayers([&atLogin](Player* plr) { plr->SetAtLoginFlag(atLogin); });
    return true;
}

int GetResetItemsBitMask(char* args)
{
    int optionsBitMask = RESET_ITEMS_COMMAND_FLAG_OPTION_NONE;

    if (strncmp(args, RESET_ITEMS_COMMAND_ARG_OPTION_EQUIPED, strlen(args)) == 0)
    {
        optionsBitMask |= RESET_ITEMS_COMMAND_FLAG_OPTION_EQUIPED;
        return optionsBitMask;
    }

    if (strncmp(args, RESET_ITEMS_COMMAND_ARG_OPTION_BAGS, strlen(args)) == 0)
    {
        optionsBitMask |= RESET_ITEMS_COMMAND_FLAG_OPTION_BAGS;
        return optionsBitMask;
    }

    if (strncmp(args, RESET_ITEMS_COMMAND_ARG_OPTION_BANK, strlen(args)) == 0)
    {
        optionsBitMask |= RESET_ITEMS_COMMAND_FLAG_OPTION_BANK;
        return optionsBitMask;
    }

    if (strncmp(args, RESET_ITEMS_COMMAND_ARG_OPTION_KEYRING, strlen(args)) == 0)
    {
        optionsBitMask |= RESET_ITEMS_COMMAND_FLAG_OPTION_KEYRING;
        return optionsBitMask;
    }

    if (strncmp(args, RESET_ITEMS_COMMAND_ARG_OPTION_BUYBACK, strlen(args)) == 0)
    {
        optionsBitMask |= RESET_ITEMS_COMMAND_FLAG_OPTION_BUYBACK;
        return optionsBitMask;
    }

    // now handle "all" or "allbags"
    // Make all yhen try to test allbags
    if (strncmp(args, RESET_ITEMS_COMMAND_ARG_OPTION_ALL, strlen(RESET_ITEMS_COMMAND_ARG_OPTION_ALL)) == 0)
    {
        // here we have at least "all" but the string is perhaps greater and indicats "allbags"
        optionsBitMask |= RESET_ITEMS_COMMAND_FLAG_OPTION_ALL;

        if(strlen(args) > strlen(RESET_ITEMS_COMMAND_ARG_OPTION_ALL))
        {
            if (strncmp(args, RESET_ITEMS_COMMAND_ARG_OPTION_ALL_BAGS, strlen(args)) == 0)
            {
                optionsBitMask |= RESET_ITEMS_COMMAND_FLAG_OPTION_ALL_BAGS;
            }
            else
            {
                return RESET_ITEMS_COMMAND_FLAG_OPTION_NONE;
            }
        }

        return optionsBitMask;
    }

    return optionsBitMask;
}

bool ChatHandler::HandleResetItemsCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    // Define all boolean by analysing args
    int optionBitMask = GetResetItemsBitMask(args);

    if (optionBitMask == RESET_ITEMS_COMMAND_FLAG_OPTION_NONE)
    {
        return false;
    }

    // Get Select player Or if no selection, use Current player
    Player * player = getSelectedPlayer();

    if(!player)
    {
        player = m_session->GetPlayer();
    }

    // Do not change swicth order because it can lead to non-empty bag deletion
    BITMASK_AND_SWITCH(optionBitMask)
    {
        case RESET_ITEMS_COMMAND_FLAG_OPTION_EQUIPED:
        {
            uint32 count = 0;
            for (int i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
            {
                if (Item* pItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
                {
                    if (pItem->GetEntry() == EARTH_STONE_ITEM)
                    {
                        // Do not delete earthstone
                        continue;
                    }
                    player->DestroyItem(INVENTORY_SLOT_BAG_0, i, true);
                    ++count;
                }
            }

            PSendSysMessage(LANG_COMMAND_RESET_ITEMS_EQUIPED, count, player->GetName());
            break;
        }

        case RESET_ITEMS_COMMAND_FLAG_OPTION_BAGS:
        {
            uint32 count = 0;
            // default bagpack :
            for (int i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
            {
                if (Item* pItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
                {
                    if (pItem->GetEntry() == EARTH_STONE_ITEM)
                    {
                        // Do not delete earthstone
                        continue;
                    }
                    player->DestroyItem(INVENTORY_SLOT_BAG_0, i, true);
                    ++count;
                }
            }

            // bagslots
            for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
            {
                if (Bag* pBag = (Bag*)player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
                {
                    for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
                    {
                        if (Item* pItem = pBag->GetItemByPos(j))
                        {
                            if (pItem->GetEntry() == EARTH_STONE_ITEM)
                            {
                                // Do not delete earthstone
                                continue;
                            }
                            player->DestroyItem(i, j, true);
                            ++count;
                        }
                    }
                }
            }
            PSendSysMessage(LANG_COMMAND_RESET_ITEMS_BAGS, count, player->GetName());
            break;
        }

        case RESET_ITEMS_COMMAND_FLAG_OPTION_BANK:
        {
            uint32 count = 0;
            // Normal bank slot
            for (int i = BANK_SLOT_ITEM_START; i < BANK_SLOT_ITEM_END; ++i)
            {
                if (Item* pItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
                {
                    if (pItem->GetEntry() == EARTH_STONE_ITEM)
                    {
                        // Do not delete earthstone
                        continue;
                    }
                    player->DestroyItem(INVENTORY_SLOT_BAG_0, i, true);
                    ++count;
                }
            }

            // Bak bagslots
            for (int i = BANK_SLOT_BAG_START; i < BANK_SLOT_BAG_END; ++i)
            {
                if (Bag* pBag = (Bag*)player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
                {
                    for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
                    {
                        if (Item* pItem = pBag->GetItemByPos(j))
                        {
                            if (pItem->GetEntry() == EARTH_STONE_ITEM)
                            {
                                // Do not delete earthstone
                                continue;
                            }
                            player->DestroyItem(i, j, true);
                            ++count;
                        }
                    }
                }
            }

            PSendSysMessage(LANG_COMMAND_RESET_ITEMS_BANK,count, player->GetName());
            break;
        }

        case RESET_ITEMS_COMMAND_FLAG_OPTION_KEYRING:
        {
            uint32 count = 0;
            for (int i = KEYRING_SLOT_START; i < KEYRING_SLOT_END; ++i)
            {
                if (Item* pItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
                {
                    if (pItem->GetEntry() == EARTH_STONE_ITEM)
                    {
                        // Do not delete earthstone
                        continue;
                    }
                    player->DestroyItem(INVENTORY_SLOT_BAG_0, i, true);
                    ++count;
                }
            }

            PSendSysMessage(LANG_COMMAND_RESET_ITEMS_KEYRING, count, player->GetName());
            break;
        }

        case RESET_ITEMS_COMMAND_FLAG_OPTION_BUYBACK:
        {
            uint32 count = 0;
            for (int i = BUYBACK_SLOT_START; i < BUYBACK_SLOT_END; ++i)
            {
                if (Item* pItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
                {
                    if (pItem->GetEntry() == EARTH_STONE_ITEM)
                    {
                        // Do not delete earthstone
                        continue;
                    }
                    player->DestroyItem(INVENTORY_SLOT_BAG_0, i, true);
                    ++count;
                }
            }

            PSendSysMessage(LANG_COMMAND_RESET_ITEMS_BUYBACK, count, player->GetName());

            break;
        }

        // Perhaps check if we have deleted earthstone if, so then re-add it
    }

    // Since bitmaskorepation is "AND" we have to manually test the last cases
    if (optionBitMask == RESET_ITEMS_COMMAND_FLAG_OPTION_ALL)
    {
        // Just text display
        PSendSysMessage(LANG_COMMAND_RESET_ITEMS_ALL, player->GetName());
    }

    if (optionBitMask == RESET_ITEMS_COMMAND_FLAG_OPTION_ALL_BAGS)
    {
        uint32 equipedBagsCount = 0;
        for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
        {
            if (Bag* pBag = (Bag*)player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            {
                player->DestroyItem(INVENTORY_SLOT_BAG_0, i, true);
                ++equipedBagsCount;
            }
        }

        uint32 bankBagscount = 0;
        // Bak bagslots
        for (int i = BANK_SLOT_BAG_START; i < BANK_SLOT_BAG_END; ++i)
        {
            if (Bag* pBag = (Bag*)player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            {
                // prevent no empty ?
                player->DestroyItem(INVENTORY_SLOT_BAG_0, i, true);
                ++bankBagscount;
            }
        }

        // Just text display
        PSendSysMessage(LANG_COMMAND_RESET_ITEMS_ALLBAGS, equipedBagsCount, bankBagscount,player->GetName());
    }


    return true;
}

int GetResetMailBitMask(char* args)
{
    int optionBitMask = RESET_MAIL_COMMAND_FLAG_OPTION_NONE;

    if (args == NULL)
    {
        return optionBitMask;
    }

    if (strncmp(args, RESET_MAIL_COMMAND_ARG_OPTION_COD, strlen(args)) == 0)
    {
        optionBitMask |= RESET_MAIL_COMMAND_FLAG_OPTION_COD;
        return optionBitMask;
    }

    if (strncmp(args, RESET_MAIL_COMMAND_ARG_OPTION_GM, strlen(args)) == 0)
    {
        optionBitMask |= RESET_MAIL_COMMAND_FLAG_OPTION_GM;
        return optionBitMask;
    }

    //Specific case for "from XXX"
    if (strncmp(args, RESET_MAIL_COMMAND_ARG_OPTION_FROM, strlen(RESET_MAIL_COMMAND_ARG_OPTION_FROM)) == 0)
    {
        optionBitMask |= RESET_MAIL_COMMAND_FLAG_OPTION_FROM;
        return optionBitMask;
    }

    if (strncmp(args, RESET_MAIL_COMMAND_ARG_OPTION_ALL, strlen(args)) == 0)
    {
        optionBitMask |= RESET_MAIL_COMMAND_FLAG_OPTION_ALL;
        return optionBitMask;
    }

    return optionBitMask;
}

/*
        HandleResetMailCommand
        Default behaviour :
       -------------------
        - delete checked mails (even if its is GM stationery and if it contains items in it, but not deleted COD)

       Options :
       ---------
        - cod : delete only cod mail (even if it is unchecked)
            TODO -> to improve => return cod to sender instead of delete
        - gm : delete only GM stationery emails (even if it is unchecked)
        - all : delete all mails (even if it is unchecked)
        - from XXXX : delete all mails from specific sender in the slected player mailbox, name or guid
          TODO  -> to improve, if unchecked return letter to sender to inform it was not read and purged by GM for tech. reason.

        TODO : future => handle reset mail for Offline char ?
*/
bool ChatHandler::HandleResetMailCommand(char* args)
{
    char* firstArg = ExtractArg(&args);

    int optionBitMask = GetResetMailBitMask(firstArg);

    // Get Select player Or if no selection, use Current player
    Player* player = getSelectedPlayer();

    if (!player)
    {
        player = m_session->GetPlayer();
    }

    uint8 totalDeletedMailCount = 0;
    uint8 deletedGMMailCount = 0;
    uint8 deletedCODMailCount = 0;
    uint8 deletedFromMailCount = 0;

    // Special chack if amil delete "from"
    // in order to retrieve player
    uint32 senderGuid = -1;
    std::string from_sender_name;

    if (optionBitMask & RESET_MAIL_COMMAND_FLAG_OPTION_FROM)
    {
        // Check if arg after "from" is player guid or playerName and if so check guid
        // Extract Uint32 from remaining arg text
        uint32  playerGuid = 0;
        Player* sender;
        ObjectGuid from_sender_guid;

        if (!ExtractPlayerTarget(&args, &sender, &from_sender_guid, &from_sender_name))
        {
            SendSysMessage(LANG_PLAYER_NOT_FOUND);
            return false;
        }

        senderGuid = from_sender_guid;
    }

    for (PlayerMails::iterator itr = player->GetMailBegin(); itr != player->GetMailEnd(); ++itr)
    {
        // Flag the mail as "deleted"
        Mail* m = (*itr);

        bool deleteMail = false;

        // DO not use the swith because it begins at 1
        if (optionBitMask == RESET_MAIL_COMMAND_FLAG_OPTION_NONE)
        {
            if (m->checked && !m->COD && m->state != MAIL_STATE_DELETED)
            {
                deleteMail = true;
            }
        }

        BITMASK_AND_SWITCH(optionBitMask)
        {
            case RESET_MAIL_COMMAND_FLAG_OPTION_COD:
            {
                if (m->COD && m->state != MAIL_STATE_DELETED)
                {
                    deleteMail = true;
                    ++deletedCODMailCount;
                }
                break;
            }

            case RESET_MAIL_COMMAND_FLAG_OPTION_GM:
            {
                if (m->stationery == MAIL_STATIONERY_GM && m->state != MAIL_STATE_DELETED)
                {
                    deleteMail = true;
                    ++deletedGMMailCount;
                }
                break;
            }

            case RESET_MAIL_COMMAND_FLAG_OPTION_FROM:
            {
                if (senderGuid != -1 && (m->sender == senderGuid) && m->state != MAIL_STATE_DELETED)
                {
                    deleteMail = true;
                    ++deletedFromMailCount;
                }
                break;
            }
        }

        if (deleteMail)
        {
            m->checked = m->checked | MAIL_CHECK_MASK_READ ;
            m->state = MAIL_STATE_DELETED;
            player->SendMailResult(m->messageID, MAIL_DELETED, MAIL_OK);
            ++totalDeletedMailCount;
        }
    }

    if (totalDeletedMailCount > 0)
    {
        player->m_mailsUpdated = true;
    }

    // Notification
     Player * gm = m_session->GetPlayer();

     BITMASK_AND_SWITCH(optionBitMask)
     {
            case RESET_MAIL_COMMAND_FLAG_OPTION_NONE:
            {
                // Nothing specific to display
                break;
            }

            case RESET_MAIL_COMMAND_FLAG_OPTION_COD:
            {
                PSendSysMessage(LANG_COMMAND_RESET_MAIL_COD, deletedCODMailCount, player->GetName());
                break;
            }

            case RESET_MAIL_COMMAND_FLAG_OPTION_GM:
            {
                PSendSysMessage(LANG_COMMAND_RESET_MAIL_GM, deletedGMMailCount, player->GetName());
                break;
            }

            case RESET_MAIL_COMMAND_FLAG_OPTION_FROM:
            {
                PSendSysMessage(LANG_COMMAND_RESET_MAIL_FROM, deletedFromMailCount, from_sender_name.c_str(), player->GetName());
                break;
            }
     }

     if (gm != player)
     {
         ChatHandler(player).PSendSysMessage(LANG_COMMAND_RESET_MAIL_PLAYER_NOTIF, m_session->GetPlayer()->GetName(), totalDeletedMailCount);
     }
     PSendSysMessage(LANG_COMMAND_RESET_MAIL_RECAP, totalDeletedMailCount, player->GetName());

    return true;
}
