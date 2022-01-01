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
#include "AccountMgr.h"
#include "GameEventMgr.h"
#include "World.h"
#include "ObjectMgr.h"
#include "SQLStorages.h"

 /**********************************************************************
      CommandTable : lookupCommandTable
 /***********************************************************************/


bool ChatHandler::LookupPlayerSearchCommand(QueryResult* result, uint32* limit)
{
    if (!result)
    {
        PSendSysMessage(LANG_NO_PLAYERS_FOUND);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 limit_original = limit ? *limit : 100;

    uint32 limit_local = limit_original;

    if (!limit)
    {
        limit = &limit_local;
    }

    do
    {
        if (limit && *limit == 0)
        {
            break;
        }

        Field* fields = result->Fetch();
        uint32 acc_id = fields[0].GetUInt32();
        std::string acc_name = fields[1].GetCppString();

        ///- Get the characters for account id
        QueryResult* chars = CharacterDatabase.PQuery("SELECT `guid`, `name`, `race`, `class`, `level` FROM `characters` WHERE `account` = %u", acc_id);
        if (chars)
        {
            if (chars->GetRowCount())
            {
                PSendSysMessage(LANG_LOOKUP_PLAYER_ACCOUNT, acc_name.c_str(), acc_id);
                ShowPlayerListHelper(chars, limit, true, false);
            }
            else
            {
                delete chars;
            }
        }
    }
    while (result->NextRow());

    delete result;

    if (*limit == limit_original)                           // empty accounts only
    {
        PSendSysMessage(LANG_NO_PLAYERS_FOUND);
        SetSentErrorMessage(true);
        return false;
    }

    return true;
}

void ChatHandler::ShowQuestListHelper(uint32 questId, int32 loc_idx, Player* target /*= NULL*/)
{
    Quest const* qinfo = sObjectMgr.GetQuestTemplate(questId);
    if (!qinfo)
    {
        return;
    }

    std::string title = qinfo->GetTitle();
    sObjectMgr.GetQuestLocaleStrings(questId, loc_idx, &title);

    char const* statusStr = "";

    if (target)
    {
        QuestStatus status = target->GetQuestStatus(qinfo->GetQuestId());

        if (status == QUEST_STATUS_COMPLETE)
        {
            if (target->GetQuestRewardStatus(qinfo->GetQuestId()))
            {
                statusStr = GetMangosString(LANG_COMMAND_QUEST_REWARDED);
            }
            else
            {
                statusStr = GetMangosString(LANG_COMMAND_QUEST_COMPLETE);
            }
        }
        else if (status == QUEST_STATUS_INCOMPLETE)
        {
            statusStr = GetMangosString(LANG_COMMAND_QUEST_ACTIVE);
        }
    }

    if (m_session)
    {
        PSendSysMessage(LANG_QUEST_LIST_CHAT, qinfo->GetQuestId(), qinfo->GetQuestId(), qinfo->GetQuestLevel(), title.c_str(), statusStr);
    }
    else
    {
        PSendSysMessage(LANG_QUEST_LIST_CONSOLE, qinfo->GetQuestId(), title.c_str(), statusStr);
    }
}

void ChatHandler::ShowItemListHelper(uint32 itemId, int loc_idx, Player* target /*=NULL*/)
{
    ItemPrototype const* itemProto = sItemStorage.LookupEntry<ItemPrototype >(itemId);
    if (!itemProto)
    {
        return;
    }

    std::string name = itemProto->Name1;
    sObjectMgr.GetItemLocaleStrings(itemProto->ItemId, loc_idx, &name);

    char const* usableStr = "";

    if (target)
    {
        if (target->CanUseItem(itemProto))
        {
            usableStr = GetMangosString(LANG_COMMAND_ITEM_USABLE);
        }
    }

    if (m_session)
    {
        PSendSysMessage(LANG_ITEM_LIST_CHAT, itemId, itemId, name.c_str(), usableStr);
    }
    else
    {
        PSendSysMessage(LANG_ITEM_LIST_CONSOLE, itemId, name.c_str(), usableStr);
    }
}

bool ChatHandler::HandleLookupAreaCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    std::string namepart = args;
    std::wstring wnamepart;

    if (!Utf8toWStr(namepart, wnamepart))
    {
        return false;
    }

    uint32 counter = 0;                                     // Counter for figure out that we found smth.

    // converting string that we try to find to lower case
    wstrToLower(wnamepart);

    // Search in AreaTable.dbc
    for (uint32 areaid = 0; areaid <= sAreaStore.GetNumRows(); ++areaid)
    {
        AreaTableEntry const* areaEntry = sAreaStore.LookupEntry(areaid);
        if (areaEntry)
        {
            int loc = GetSessionDbcLocale();
            std::string name = areaEntry->area_name[loc];
            if (name.empty())
            {
                continue;
            }

            if (!Utf8FitTo(name, wnamepart))
            {
                loc = 0;
                for (; loc < MAX_LOCALE; ++loc)
                {
                    if (loc == GetSessionDbcLocale())
                    {
                        continue;
                    }

                    name = areaEntry->area_name[loc];
                    if (name.empty())
                    {
                        continue;
                    }

                    if (Utf8FitTo(name, wnamepart))
                    {
                        break;
                    }
                }
            }

            if (loc < MAX_LOCALE)
            {
                // send area in "id - [name]" format
                std::ostringstream ss;
                if (m_session)
                {
                    ss << areaEntry->ID << " - |cffffffff|Harea:" << areaEntry->ID << "|h[" << name << " " << localeNames[loc] << "]|h|r";
                }
                else
                {
                    ss << areaEntry->ID << " - " << name << " " << localeNames[loc];
                }

                SendSysMessage(ss.str().c_str());

                ++counter;
            }
        }
    }

    if (counter == 0)                                      // if counter == 0 then we found nth
    {
        SendSysMessage(LANG_COMMAND_NOAREAFOUND);
    }

    return true;
}

// Find tele in game_tele order by name
bool ChatHandler::HandleLookupTeleCommand(char* args)
{
    if (!*args)
    {
        SendSysMessage(LANG_COMMAND_TELE_PARAMETER);
        SetSentErrorMessage(true);
        return false;
    }

    std::string namepart = args;
    std::wstring wnamepart;

    if (!Utf8toWStr(namepart, wnamepart))
    {
        return false;
    }

    // converting string that we try to find to lower case
    wstrToLower(wnamepart);

    std::ostringstream reply;

    GameTeleMap const& teleMap = sObjectMgr.GetGameTeleMap();
    for (GameTeleMap::const_iterator itr = teleMap.begin(); itr != teleMap.end(); ++itr)
    {
        GameTele const* tele = &itr->second;

        if (tele->wnameLow.find(wnamepart) == std::wstring::npos)
        {
            continue;
        }

        if (m_session)
        {
            reply << "  |cffffffff|Htele:" << itr->first << "|h[" << tele->name << "]|h|r\n";
        }
        else
        {
            reply << "  " << itr->first << " " << tele->name << "\n";
        }
    }

    if (reply.str().empty())
    {
        SendSysMessage(LANG_COMMAND_TELE_NOLOCATION);
    }
    else
    {
        PSendSysMessage(LANG_COMMAND_TELE_LOCATION, reply.str().c_str());
    }

    return true;
}

bool ChatHandler::HandleLookupFactionCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    // Can be NULL at console call
    Player* target = getSelectedPlayer();

    std::string namepart = args;
    std::wstring wnamepart;

    if (!Utf8toWStr(namepart, wnamepart))
    {
        return false;
    }

    // converting string that we try to find to lower case
    wstrToLower(wnamepart);

    uint32 counter = 0;                                     // Counter for figure out that we found smth.

    for (uint32 id = 0; id < sFactionStore.GetNumRows(); ++id)
    {
        FactionEntry const* factionEntry = sFactionStore.LookupEntry(id);
        if (factionEntry)
        {
            int loc = GetSessionDbcLocale();
            std::string name = factionEntry->name[loc];
            if (name.empty())
            {
                continue;
            }

            if (!Utf8FitTo(name, wnamepart))
            {
                loc = 0;
                for (; loc < MAX_LOCALE; ++loc)
                {
                    if (loc == GetSessionDbcLocale())
                    {
                        continue;
                    }

                    name = factionEntry->name[loc];
                    if (name.empty())
                    {
                        continue;
                    }

                    if (Utf8FitTo(name, wnamepart))
                    {
                        break;
                    }
                }
            }

            if (loc < MAX_LOCALE)
            {
                FactionState const* repState = target ? target->GetReputationMgr().GetState(factionEntry) : NULL;
                ShowFactionListHelper(factionEntry, LocaleConstant(loc), repState, target);
                ++counter;
            }
        }
    }

    if (counter == 0)                                       // if counter == 0 then we found nth
    {
        SendSysMessage(LANG_COMMAND_FACTION_NOTFOUND);
    }
    return true;
}

bool ChatHandler::HandleLookupEventCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    std::string namepart = args;
    std::wstring wnamepart;

    // converting string that we try to find to lower case
    if (!Utf8toWStr(namepart, wnamepart))
    {
        return false;
    }

    wstrToLower(wnamepart);

    uint32 counter = 0;

    GameEventMgr::GameEventDataMap const& events = sGameEventMgr.GetEventMap();

    for (uint32 id = 1; id < events.size(); ++id)
    {
        if (!sGameEventMgr.IsValidEvent(id))
        {
            continue;
        }

        GameEventData const& eventData = events[id];

        std::string descr = eventData.description;
        if (descr.empty())
        {
            continue;
        }

        if (Utf8FitTo(descr, wnamepart))
        {
            char const* active = sGameEventMgr.IsActiveEvent(id) ? GetMangosString(LANG_ACTIVE) : "";

            if (m_session)
            {
                PSendSysMessage(LANG_EVENT_ENTRY_LIST_CHAT, id, id, eventData.description.c_str(), active);
            }
            else
            {
                PSendSysMessage(LANG_EVENT_ENTRY_LIST_CONSOLE, id, eventData.description.c_str(), active);
            }

            ++counter;
        }
    }

    if (counter == 0)
    {
        SendSysMessage(LANG_NOEVENTFOUND);
    }

    return true;
}

bool ChatHandler::HandleLookupAccountEmailCommand(char* args)
{
    char* emailStr = ExtractQuotedOrLiteralArg(&args);
    if (!emailStr)
    {
        return false;
    }

    uint32 limit;
    if (!ExtractOptUInt32(&args, limit, 100))
    {
        return false;
    }

    std::string email = emailStr;
    LoginDatabase.escape_string(email);
    //                                                 0   1         2        3        4
    QueryResult* result = LoginDatabase.PQuery("SELECT `id`, `username`, `last_ip`, `gmlevel`, `expansion` FROM `account` WHERE `email` " _LIKE_ " " _CONCAT3_("'%%'", "'%s'", "'%%'"), email.c_str());

    return ShowAccountListHelper(result, &limit);
}

bool ChatHandler::HandleLookupAccountIpCommand(char* args)
{
    char* ipStr = ExtractQuotedOrLiteralArg(&args);
    if (!ipStr)
    {
        return false;
    }

    uint32 limit;
    if (!ExtractOptUInt32(&args, limit, 100))
    {
        return false;
    }

    std::string ip = ipStr;
    LoginDatabase.escape_string(ip);

    //                                                 0   1         2        3        4
    QueryResult* result = LoginDatabase.PQuery("SELECT `id`, `username`, `last_ip`, `gmlevel`, `expansion` FROM `account` WHERE `last_ip` " _LIKE_ " " _CONCAT3_("'%%'", "'%s'", "'%%'"), ip.c_str());

    return ShowAccountListHelper(result, &limit);
}

bool ChatHandler::HandleLookupAccountNameCommand(char* args)
{
    char* accountStr = ExtractQuotedOrLiteralArg(&args);
    if (!accountStr)
    {
        return false;
    }

    uint32 limit;
    if (!ExtractOptUInt32(&args, limit, 100))
    {
        return false;
    }

    std::string account = accountStr;
    if (!AccountMgr::normalizeString(account))
    {
        return false;
    }

    LoginDatabase.escape_string(account);
    //                                                  0     1           2          3          4
    QueryResult* result = LoginDatabase.PQuery("SELECT `id`, `username`, `last_ip`, `gmlevel`, `expansion` FROM `account` WHERE `username` " _LIKE_ " " _CONCAT3_("'%%'", "'%s'", "'%%'"), account.c_str());

    return ShowAccountListHelper(result, &limit);
}

bool ChatHandler::ShowAccountListHelper(QueryResult* result, uint32* limit, bool title, bool error)
{
    if (!result)
    {
        if (error)
        {
            SendSysMessage(LANG_ACCOUNT_LIST_EMPTY);
        }
        return true;
    }

    ///- Display the list of account/characters online
    if (!m_session && title)                                // not output header for online case
    {
        SendSysMessage(LANG_ACCOUNT_LIST_BAR);
        SendSysMessage(LANG_ACCOUNT_LIST_HEADER);
        SendSysMessage(LANG_ACCOUNT_LIST_BAR);
    }

    ///- Circle through accounts
    do
    {
        // check limit
        if (limit)
        {
            if (*limit == 0)
            {
                break;
            }
            --* limit;
        }

        Field* fields = result->Fetch();
        uint32 account = fields[0].GetUInt32();

        WorldSession* session = sWorld.FindSession(account);
        Player* player = session ? session->GetPlayer() : NULL;
        char const* char_name = player ? player->GetName() : " - ";

        if (m_session)
            PSendSysMessage(LANG_ACCOUNT_LIST_LINE_CHAT,
                            account, fields[1].GetString(), char_name, fields[2].GetString(), fields[3].GetUInt32(), fields[4].GetUInt32());
        else
            PSendSysMessage(LANG_ACCOUNT_LIST_LINE_CONSOLE,
                            account, fields[1].GetString(), char_name, fields[2].GetString(), fields[3].GetUInt32(), fields[4].GetUInt32());
    }
    while (result->NextRow());

    delete result;

    if (!m_session)                                         // not output header for online case
    {
        SendSysMessage(LANG_ACCOUNT_LIST_BAR);
    }

    return true;
}

bool ChatHandler::HandleLookupPlayerIpCommand(char* args)
{
    char* ipStr = ExtractQuotedOrLiteralArg(&args);
    if (!ipStr)
    {
        return false;
    }

    uint32 limit;
    if (!ExtractOptUInt32(&args, limit, 100))
    {
        return false;
    }

    std::string ip = ipStr;
    LoginDatabase.escape_string(ip);

    QueryResult* result = LoginDatabase.PQuery("SELECT `id`,`username` FROM `account` WHERE `last_ip` " _LIKE_ " " _CONCAT3_("'%%'", "'%s'", "'%%'"), ip.c_str());

    return LookupPlayerSearchCommand(result, &limit);
}

bool ChatHandler::HandleLookupPlayerAccountCommand(char* args)
{
    char* accountStr = ExtractQuotedOrLiteralArg(&args);
    if (!accountStr)
    {
        return false;
    }

    uint32 limit;
    if (!ExtractOptUInt32(&args, limit, 100))
    {
        return false;
    }

    std::string account = accountStr;
    if (!AccountMgr::normalizeString(account))
    {
        return false;
    }

    LoginDatabase.escape_string(account);

    QueryResult* result = LoginDatabase.PQuery("SELECT `id`,`username` FROM `account` WHERE `username` " _LIKE_ " " _CONCAT3_("'%%'", "'%s'", "'%%'"), account.c_str());

    return LookupPlayerSearchCommand(result, &limit);
}

bool ChatHandler::HandleLookupPlayerEmailCommand(char* args)
{
    char* emailStr = ExtractQuotedOrLiteralArg(&args);
    if (!emailStr)
    {
        return false;
    }

    uint32 limit;
    if (!ExtractOptUInt32(&args, limit, 100))
    {
        return false;
    }

    std::string email = emailStr;
    LoginDatabase.escape_string(email);

    QueryResult* result = LoginDatabase.PQuery("SELECT `id`,`username` FROM `account` WHERE `email` " _LIKE_ " " _CONCAT3_("'%%'", "'%s'", "'%%'"), email.c_str());

    return LookupPlayerSearchCommand(result, &limit);
}

bool ChatHandler::HandleLookupPoolCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    std::string namepart = args;
    strToLower(namepart);

    uint32 counter = 0;

    // spawn pools for expected map or for not initialized shared pools state for non-instanceable maps
    for (uint16 pool_id = 0; pool_id < sPoolMgr.GetMaxPoolId(); ++pool_id)
    {
        PoolTemplateData const& pool_template = sPoolMgr.GetPoolTemplate(pool_id);

        std::string desc = pool_template.description;
        strToLower(desc);

        if (desc.find(namepart) == std::wstring::npos)
        {
            continue;
        }

        ShowPoolListHelper(pool_id);
        ++counter;
    }

    if (counter == 0)
    {
        SendSysMessage(LANG_NO_POOL);
    }

    return true;
}

bool ChatHandler::HandleLookupItemCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    std::string namepart = args;
    std::wstring wnamepart;

    // converting string that we try to find to lower case
    if (!Utf8toWStr(namepart, wnamepart))
    {
        return false;
    }

    wstrToLower(wnamepart);

    Player* pl = m_session ? m_session->GetPlayer() : NULL;

    uint32 counter = 0;

    // Search in `item_template`
    for (uint32 id = 0; id < sItemStorage.GetMaxEntry(); ++id)
    {
        ItemPrototype const* pProto = sItemStorage.LookupEntry<ItemPrototype >(id);
        if (!pProto)
        {
            continue;
        }

        int loc_idx = GetSessionDbLocaleIndex();

        std::string name;                                   // "" for let later only single time check default locale name directly
        sObjectMgr.GetItemLocaleStrings(id, loc_idx, &name);
        if ((name.empty() || !Utf8FitTo(name, wnamepart)) && !Utf8FitTo(pProto->Name1, wnamepart))
        {
            continue;
        }

        ShowItemListHelper(id, loc_idx, pl);
        ++counter;
    }

    if (counter == 0)
    {
        SendSysMessage(LANG_COMMAND_NOITEMFOUND);
    }

    return true;
}

bool ChatHandler::HandleLookupItemSetCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    std::string namepart = args;
    std::wstring wnamepart;

    if (!Utf8toWStr(namepart, wnamepart))
    {
        return false;
    }

    // converting string that we try to find to lower case
    wstrToLower(wnamepart);

    uint32 counter = 0;                                     // Counter for figure out that we found smth.

    // Search in ItemSet.dbc
    for (uint32 id = 0; id < sItemSetStore.GetNumRows(); ++id)
    {
        ItemSetEntry const* set = sItemSetStore.LookupEntry(id);
        if (set)
        {
            int loc = GetSessionDbcLocale();
            std::string name = set->name[loc];
            if (name.empty())
            {
                continue;
            }

            if (!Utf8FitTo(name, wnamepart))
            {
                loc = 0;
                for (; loc < MAX_LOCALE; ++loc)
                {
                    if (loc == GetSessionDbcLocale())
                    {
                        continue;
                    }

                    name = set->name[loc];
                    if (name.empty())
                    {
                        continue;
                    }

                    if (Utf8FitTo(name, wnamepart))
                    {
                        break;
                    }
                }
            }

            if (loc < MAX_LOCALE)
            {
                // send item set in "id - [namedlink locale]" format
                if (m_session)
                {
                    PSendSysMessage(LANG_ITEMSET_LIST_CHAT, id, id, name.c_str(), localeNames[loc]);
                }
                else
                {
                    PSendSysMessage(LANG_ITEMSET_LIST_CONSOLE, id, name.c_str(), localeNames[loc]);
                }
                ++counter;
            }
        }
    }
    if (counter == 0)                                       // if counter == 0 then we found nth
    {
        SendSysMessage(LANG_COMMAND_NOITEMSETFOUND);
    }
    return true;
}

bool ChatHandler::HandleLookupSkillCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    // can be NULL in console call
    Player* target = getSelectedPlayer();

    std::string namepart = args;
    std::wstring wnamepart;

    if (!Utf8toWStr(namepart, wnamepart))
    {
        return false;
    }

    // converting string that we try to find to lower case
    wstrToLower(wnamepart);

    uint32 counter = 0;                                     // Counter for figure out that we found smth.

    // Search in SkillLine.dbc
    for (uint32 id = 0; id < sSkillLineStore.GetNumRows(); ++id)
    {
        SkillLineEntry const* skillInfo = sSkillLineStore.LookupEntry(id);
        if (skillInfo)
        {
            int loc = GetSessionDbcLocale();
            std::string name = skillInfo->name[loc];
            if (name.empty())
            {
                continue;
            }

            if (!Utf8FitTo(name, wnamepart))
            {
                loc = 0;
                for (; loc < MAX_LOCALE; ++loc)
                {
                    if (loc == GetSessionDbcLocale())
                    {
                        continue;
                    }

                    name = skillInfo->name[loc];
                    if (name.empty())
                    {
                        continue;
                    }

                    if (Utf8FitTo(name, wnamepart))
                    {
                        break;
                    }
                }
            }

            if (loc < MAX_LOCALE)
            {
                char valStr[50] = "";
                char const* knownStr = "";
                if (target && target->HasSkill(id))
                {
                    knownStr = GetMangosString(LANG_KNOWN);
                    uint32 curValue = target->GetPureSkillValue(id);
                    uint32 maxValue = target->GetPureMaxSkillValue(id);
                    uint32 permValue = target->GetSkillPermBonusValue(id);
                    uint32 tempValue = target->GetSkillTempBonusValue(id);

                    char const* valFormat = GetMangosString(LANG_SKILL_VALUES);
                    snprintf(valStr, 50, valFormat, curValue, maxValue, permValue, tempValue);
                }

                // send skill in "id - [namedlink locale]" format
                if (m_session)
                {
                    PSendSysMessage(LANG_SKILL_LIST_CHAT, id, id, name.c_str(), localeNames[loc], knownStr, valStr);
                }
                else
                {
                    PSendSysMessage(LANG_SKILL_LIST_CONSOLE, id, name.c_str(), localeNames[loc], knownStr, valStr);
                }

                ++counter;
            }
        }
    }
    if (counter == 0)                                       // if counter == 0 then we found nth
    {
        SendSysMessage(LANG_COMMAND_NOSKILLFOUND);
    }
    return true;
}


bool ChatHandler::HandleLookupSpellCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    // can be NULL at console call
    Player* target = getSelectedPlayer();

    std::string namepart = args;
    std::wstring wnamepart;

    if (!Utf8toWStr(namepart, wnamepart))
    {
        return false;
    }

    // converting string that we try to find to lower case
    wstrToLower(wnamepart);

    uint32 counter = 0;                                     // Counter for figure out that we found smth.

    // Search in Spell.dbc
    for (uint32 id = 0; id < sSpellStore.GetNumRows(); ++id)
    {
        SpellEntry const* spellInfo = sSpellStore.LookupEntry(id);
        if (spellInfo)
        {
            int loc = GetSessionDbcLocale();
            std::string name = spellInfo->SpellName[loc];
            if (name.empty())
            {
                continue;
            }

            if (!Utf8FitTo(name, wnamepart))
            {
                loc = 0;
                for (; loc < MAX_LOCALE; ++loc)
                {
                    if (loc == GetSessionDbcLocale())
                    {
                        continue;
                    }

                    name = spellInfo->SpellName[loc];
                    if (name.empty())
                    {
                        continue;
                    }

                    if (Utf8FitTo(name, wnamepart))
                    {
                        break;
                    }
                }
            }

            if (loc < MAX_LOCALE)
            {
                ShowSpellListHelper(target, spellInfo, LocaleConstant(loc));
                ++counter;
            }
        }
    }
    if (counter == 0)                                       // if counter == 0 then we found nth
    {
        SendSysMessage(LANG_COMMAND_NOSPELLFOUND);
    }
    return true;
}

bool ChatHandler::HandleLookupQuestCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    // can be NULL at console call
    Player* target = getSelectedPlayer();

    std::string namepart = args;
    std::wstring wnamepart;

    // converting string that we try to find to lower case
    if (!Utf8toWStr(namepart, wnamepart))
    {
        return false;
    }

    wstrToLower(wnamepart);

    uint32 counter = 0;

    int loc_idx = GetSessionDbLocaleIndex();

    ObjectMgr::QuestMap const& qTemplates = sObjectMgr.GetQuestTemplates();
    for (ObjectMgr::QuestMap::const_iterator iter = qTemplates.begin(); iter != qTemplates.end(); ++iter)
    {
        Quest* qinfo = iter->second;

        std::string title;                                  // "" for avoid repeating check default locale
        sObjectMgr.GetQuestLocaleStrings(qinfo->GetQuestId(), loc_idx, &title);

        if ((title.empty() || !Utf8FitTo(title, wnamepart)) && !Utf8FitTo(qinfo->GetTitle(), wnamepart))
        {
            continue;
        }

        ShowQuestListHelper(qinfo->GetQuestId(), loc_idx, target);
        ++counter;
    }

    if (counter == 0)
    {
        SendSysMessage(LANG_COMMAND_NOQUESTFOUND);
    }

    return true;
}

bool ChatHandler::HandleLookupCreatureCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    std::string namepart = args;
    std::wstring wnamepart;

    // converting string that we try to find to lower case
    if (!Utf8toWStr(namepart, wnamepart))
    {
        return false;
    }

    wstrToLower(wnamepart);

    uint32 counter = 0;

    for (uint32 id = 0; id < sCreatureStorage.GetMaxEntry(); ++id)
    {
        CreatureInfo const* cInfo = sCreatureStorage.LookupEntry<CreatureInfo>(id);
        if (!cInfo)
        {
            continue;
        }

        int loc_idx = GetSessionDbLocaleIndex();

        char const* name = "";                              // "" for avoid repeating check for default locale
        sObjectMgr.GetCreatureLocaleStrings(id, loc_idx, &name);
        if (!*name || !Utf8FitTo(name, wnamepart))
        {
            name = cInfo->Name;
            if (!Utf8FitTo(name, wnamepart))
            {
                continue;
            }
        }

        if (m_session)
        {
            PSendSysMessage(LANG_CREATURE_ENTRY_LIST_CHAT, id, id, name);
        }
        else
        {
            PSendSysMessage(LANG_CREATURE_ENTRY_LIST_CONSOLE, id, name);
        }

        ++counter;
    }

    if (counter == 0)
    {
        SendSysMessage(LANG_COMMAND_NOCREATUREFOUND);
    }

    return true;
}

bool ChatHandler::HandleLookupObjectCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    std::string namepart = args;
    std::wstring wnamepart;

    // converting string that we try to find to lower case
    if (!Utf8toWStr(namepart, wnamepart))
    {
        return false;
    }

    wstrToLower(wnamepart);

    uint32 counter = 0;

    for (SQLStorageBase::SQLSIterator<GameObjectInfo> itr = sGOStorage.getDataBegin<GameObjectInfo>(); itr < sGOStorage.getDataEnd<GameObjectInfo>(); ++itr)
    {
        int loc_idx = GetSessionDbLocaleIndex();
        if (loc_idx >= 0)
        {
            GameObjectLocale const* gl = sObjectMgr.GetGameObjectLocale(itr->id);
            if (gl)
            {
                if ((int32)gl->Name.size() > loc_idx && !gl->Name[loc_idx].empty())
                {
                    std::string name = gl->Name[loc_idx];

                    if (Utf8FitTo(name, wnamepart))
                    {
                        if (m_session)
                        {
                            PSendSysMessage(LANG_GO_ENTRY_LIST_CHAT, itr->id, itr->id, name.c_str());
                        }
                        else
                        {
                            PSendSysMessage(LANG_GO_ENTRY_LIST_CONSOLE, itr->id, name.c_str());
                        }
                        ++counter;
                        continue;
                    }
                }
            }
        }

        std::string name = itr->name;
        if (name.empty())
        {
            continue;
        }

        if (Utf8FitTo(name, wnamepart))
        {
            if (m_session)
            {
                PSendSysMessage(LANG_GO_ENTRY_LIST_CHAT, itr->id, itr->id, name.c_str());
            }
            else
            {
                PSendSysMessage(LANG_GO_ENTRY_LIST_CONSOLE, itr->id, name.c_str());
            }
            ++counter;
        }
    }

    if (counter == 0)
    {
        SendSysMessage(LANG_COMMAND_NOGAMEOBJECTFOUND);
    }

    return true;
}

bool ChatHandler::HandleLookupTaxiNodeCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    std::string namepart = args;
    std::wstring wnamepart;

    if (!Utf8toWStr(namepart, wnamepart))
    {
        return false;
    }

    // converting string that we try to find to lower case
    wstrToLower(wnamepart);

    uint32 counter = 0;                                     // Counter for figure out that we found smth.

    // Search in TaxiNodes.dbc
    for (uint32 id = 0; id < sTaxiNodesStore.GetNumRows(); ++id)
    {
        TaxiNodesEntry const* nodeEntry = sTaxiNodesStore.LookupEntry(id);
        if (nodeEntry)
        {
            int loc = GetSessionDbcLocale();
            std::string name = nodeEntry->name[loc];
            if (name.empty())
            {
                continue;
            }

            if (!Utf8FitTo(name, wnamepart))
            {
                loc = 0;
                for (; loc < MAX_LOCALE; ++loc)
                {
                    if (loc == GetSessionDbcLocale())
                    {
                        continue;
                    }

                    name = nodeEntry->name[loc];
                    if (name.empty())
                    {
                        continue;
                    }

                    if (Utf8FitTo(name, wnamepart))
                    {
                        break;
                    }
                }
            }

            if (loc < MAX_LOCALE)
            {
                // send taxinode in "id - [name] (Map:m X:x Y:y Z:z)" format
                if (m_session)
                    PSendSysMessage(LANG_TAXINODE_ENTRY_LIST_CHAT, id, id, name.c_str(), localeNames[loc],
                                    nodeEntry->map_id, nodeEntry->x, nodeEntry->y, nodeEntry->z);
                else
                    PSendSysMessage(LANG_TAXINODE_ENTRY_LIST_CONSOLE, id, name.c_str(), localeNames[loc],
                                    nodeEntry->map_id, nodeEntry->x, nodeEntry->y, nodeEntry->z);
                ++counter;
            }
        }
    }
    if (counter == 0)                                       // if counter == 0 then we found nth
    {
        SendSysMessage(LANG_COMMAND_NOTAXINODEFOUND);
    }
    return true;
}
