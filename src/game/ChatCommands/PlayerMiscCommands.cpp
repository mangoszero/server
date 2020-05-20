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

#include "Chat.h"
#include "Language.h"
#include "World.h"

 /**********************************************************************
     CommandTable : commandTable
 /***********************************************************************/

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
