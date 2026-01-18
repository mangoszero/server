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

#include "Chat.h"
#include "ObjectMgr.h"
#include "World.h"
#include "Weather.h"
#include "SpellMgr.h"


 /**********************************************************************
     CommandTable : commandTable
 /***********************************************************************/

// show info of player
bool ChatHandler::HandlePInfoCommand(char* args)
{
    Player* target;
    ObjectGuid target_guid;
    std::string target_name;
    if (!ExtractPlayerTarget(&args, &target, &target_guid, &target_name))
    {
        return false;
    }

    uint32 accId;
    uint32 money;
    uint32 total_player_time;
    uint32 level;
    uint32 latency = 0;

    // get additional information from Player object
    if (target)
    {
        // check online security
        if (HasLowerSecurity(target))
        {
            return false;
        }

        accId = target->GetSession()->GetAccountId();
        money = target->GetMoney();
        total_player_time = target->GetTotalPlayedTime();
        level = target->getLevel();
        latency = target->GetSession()->GetLatency();
    }
    // get additional information from DB
    else
    {
        // check offline security
        if (HasLowerSecurity(NULL, target_guid))
        {
            return false;
        }

        //                                                     0          1      2      3
        QueryResult* result = CharacterDatabase.PQuery("SELECT `totaltime`, `level`, `money`, `account` FROM `characters` WHERE `guid` = '%u'", target_guid.GetCounter());
        if (!result)
        {
            return false;
        }

        Field* fields = result->Fetch();
        total_player_time = fields[0].GetUInt32();
        level = fields[1].GetUInt32();
        money = fields[2].GetUInt32();
        accId = fields[3].GetUInt32();
        delete result;
    }

    std::string username = GetMangosString(LANG_ERROR);
    std::string email = GetMangosString(LANG_ERROR);
    std::string last_ip = GetMangosString(LANG_ERROR);
    AccountTypes security = SEC_PLAYER;
    std::string last_login = GetMangosString(LANG_ERROR);

    QueryResult* result = LoginDatabase.PQuery("SELECT `username`,`gmlevel`,`email`,`last_ip`,`last_login` FROM `account` WHERE `id` = '%u'", accId);
    if (result)
    {
        Field* fields = result->Fetch();
        username = fields[0].GetCppString();
        security = (AccountTypes)fields[1].GetUInt32();

        if (GetAccessLevel() >= security)
        {
            if (security == SEC_ADMINISTRATOR)
            {
                email = fields[2].GetCppString();
            }
            else
            {
                email = "*hidden*";
            }
            last_ip = fields[3].GetCppString();
            last_login = fields[4].GetCppString();
        }
        else
        {
            email = "-";
            last_ip = "-";
            last_login = "-";
        }

        delete result;
    }

    std::string nameLink = playerLink(target_name);

    PSendSysMessage(LANG_PINFO_ACCOUNT, (target ? "" : GetMangosString(LANG_OFFLINE)), nameLink.c_str(), target_guid.GetCounter(), username.c_str(), accId, security, email.c_str(), last_ip.c_str(), last_login.c_str(), latency);

    std::string timeStr = secsToTimeString(total_player_time, TimeFormat::ShortText, true);
    uint32 gold = money / GOLD;
    uint32 silv = (money % GOLD) / SILVER;
    uint32 copp = (money % GOLD) % SILVER;
    PSendSysMessage(LANG_PINFO_LEVEL, timeStr.c_str(), level, gold, silv, copp);
    if (target)
    {
        uint32 mapId = target->GetMapId();
        uint32 zoneId = target->GetZoneId();
        float posX = target->GetPositionX();
        float posY = target->GetPositionY();
        float posZ = target->GetPositionZ();
        float orientation = target->GetOrientation();

        MapEntry const* mapEntry = sMapStore.LookupEntry(mapId);
        AreaTableEntry const* zoneEntry = GetAreaEntryByAreaID(zoneId);

        PSendSysMessage("Location: Map %u (%s), Zone %u (%s)",
                        mapId,
                        (mapEntry ? mapEntry->name[GetSessionDbcLocale()] : "<unknown>"),
                        zoneId,
                        (zoneEntry ? zoneEntry->area_name[GetSessionDbcLocale()] : "<unknown>"));

        PSendSysMessage("Coordinates: X=%.2f Y=%.2f Z=%.2f O=%.2f",
                        posX, posY, posZ, orientation);
    }
    return true;
}

bool ChatHandler::HandleWaterwalkCommand(char* args)
{
    bool value;
    if (!ExtractOnOff(&args, value))
    {
        SendSysMessage(LANG_USE_BOL);
        SetSentErrorMessage(true);
        return false;
    }

    Player* player = getSelectedPlayer();

    if (!player)
    {
        PSendSysMessage(LANG_NO_CHAR_SELECTED);
        SetSentErrorMessage(true);
        return false;
    }

    // check online security
    if (HasLowerSecurity(player))
    {
        return false;
    }

    if (value)
    {
        player->SetWaterWalk(true); // ON
    }
    else
    {
        player->SetWaterWalk(false); // OFF
    }

    PSendSysMessage(LANG_YOU_SET_WATERWALK, args, GetNameLink(player).c_str());
    if (needReportToTarget(player))
    {
        ChatHandler(player).PSendSysMessage(LANG_YOUR_WATERWALK_SET, args, GetNameLink().c_str());
    }
    return true;
}

// Enable\Dissable GM Mode
bool ChatHandler::HandleGMCommand(char* args)
{
    if (!*args)
    {
        if (m_session->GetPlayer()->isGameMaster())
        {
            m_session->SendNotification(LANG_GM_ON);
        }
        else
        {
            m_session->SendNotification(LANG_GM_OFF);
        }
        return true;
    }

    std::string argstr = (char*)args;

    if (argstr == "on")
    {
        m_session->GetPlayer()->SetGameMaster(true);
        m_session->SendNotification(LANG_GM_ON);
#ifdef _DEBUG_VMAPS
        VMAP::IVMapManager *vMapManager = VMAP::VMapFactory::createOrGetVMapManager();
        vMapManager->processCommand("stoplog");
#endif

        return true;
    }

    if (argstr == "off")
    {
        m_session->GetPlayer()->SetGameMaster(false);
        m_session->SendNotification(LANG_GM_OFF);
#ifdef _DEBUG_VMAPS
        VMAP::IVMapManager *vMapManager = VMAP::VMapFactory::createOrGetVMapManager();
        vMapManager->processCommand("startlog");
#endif

        return true;
    }

    SendSysMessage(LANG_USE_BOL);
    SetSentErrorMessage(true);
    return false;
}

// Enable\Dissable Invisible mode
bool ChatHandler::HandleGMVisibleCommand(char* args)
{
    if (!*args)
    {
        PSendSysMessage(LANG_YOU_ARE, m_session->GetPlayer()->isGMVisible() ?  GetMangosString(LANG_VISIBLE) : GetMangosString(LANG_INVISIBLE));
        return true;
    }

    bool value;
    if (!ExtractOnOff(&args, value))
    {
        SendSysMessage(LANG_USE_BOL);
        SetSentErrorMessage(true);
        return false;
    }

    Player* player = m_session->GetPlayer();
    SpellEntry const* invisibleAuraInfo = sSpellStore.LookupEntry(sWorld.getConfig(CONFIG_UINT32_GM_INVISIBLE_AURA));
    if (!invisibleAuraInfo || !IsSpellAppliesAura(invisibleAuraInfo))
    {
        invisibleAuraInfo = NULL;
    }

    if (value)
    {
        player->SetGMVisible(true);
        m_session->SendNotification(LANG_INVISIBLE_VISIBLE);
        if (invisibleAuraInfo)
        {
            player->RemoveAurasDueToSpell(invisibleAuraInfo->Id);
        }
    }
    else
    {
        m_session->SendNotification(LANG_INVISIBLE_INVISIBLE);
        player->SetGMVisible(false);
        if (invisibleAuraInfo)
        {
            player->CastSpell(player, invisibleAuraInfo, true);
        }
    }

    return true;
}

bool ChatHandler::HandleGMFlyCommand(char* args)
{
    bool value;
    if (!ExtractOnOff(&args, value))
    {
        SendSysMessage(LANG_USE_BOL);
        SetSentErrorMessage(true);
        return false;
    }

    Player* target = getSelectedPlayer();
    if (!target)
    {
        target = m_session->GetPlayer();
    }

    // [-ZERO] Need reimplement in another way
    // GM fly wil be achieved with the swimming moveflag
    // Warning : Still buggy when Jump
    target->SetCanFly(value);
    PSendSysMessage(LANG_COMMAND_FLYMODE_STATUS, GetNameLink(target).c_str(), args);
    return true;
}

bool ChatHandler::HandleGMListIngameCommand(char* /*args*/)
{
    std::list< std::pair<std::string, bool> > names;
    sObjectAccessor.DoForAllPlayers([&names, this](Player *player)
    {
        AccountTypes security = player->GetSession()->GetSecurity();
        if ((player->isGameMaster() || (security > SEC_PLAYER && security <= (AccountTypes)sWorld.getConfig(CONFIG_UINT32_GM_LEVEL_IN_GM_LIST))) &&
            (!m_session || player->IsVisibleGloballyFor(m_session->GetPlayer())))
        {
            names.push_back(std::make_pair<std::string, bool>(GetNameLink(player), player->isAcceptWhispers()));
        }
    });

    if (!names.empty())
    {
        SendSysMessage(LANG_GMS_ON_SRV);

        char const* accepts = GetMangosString(LANG_GM_ACCEPTS_WHISPER);
        char const* not_accept = GetMangosString(LANG_GM_NO_WHISPER);
        for (std::list<std::pair< std::string, bool> >::const_iterator iter = names.begin(); iter != names.end(); ++iter)
        {
            PSendSysMessage("%s - %s", iter->first.c_str(), iter->second ? accepts : not_accept);
        }
    }
    else
    {
        SendSysMessage(LANG_GMS_NOT_LOGGED);
    }

    return true;
}

/// Display the list of GMs
bool ChatHandler::HandleGMListFullCommand(char* /*args*/)
{
    ///- Get the accounts with GM Level >0
    QueryResult* result = LoginDatabase.Query("SELECT `username`,`gmlevel` FROM `account` WHERE `gmlevel` > 0");
    if (result)
    {
        SendSysMessage(LANG_GMLIST);
        SendSysMessage("========================");
        SendSysMessage(LANG_GMLIST_HEADER);
        SendSysMessage("========================");

        ///- Circle through them. Display username and GM level
        do
        {
            Field* fields = result->Fetch();
            PSendSysMessage("|%15s|%6s|", fields[0].GetString(), fields[1].GetString());
        }
        while (result->NextRow());

        PSendSysMessage("========================");
        delete result;
    }
    else
    {
        PSendSysMessage(LANG_GMLIST_EMPTY);
    }
    return true;
}

// change standstate
bool ChatHandler::HandleModifyStandStateCommand(char* args)
{
    uint32 anim_id;
    if (!ExtractUInt32(&args, anim_id))
    {
        return false;
    }

    if (!sEmotesStore.LookupEntry(anim_id))
    {
        return false;
    }

    m_session->GetPlayer()->HandleEmoteState(anim_id);

    return true;
}

bool ChatHandler::HandleChangeWeatherCommand(char* args)
{
    // Weather is OFF
    if (!sWorld.getConfig(CONFIG_BOOL_WEATHER))
    {
        SendSysMessage(LANG_WEATHER_DISABLED);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 type;
    if (!ExtractUInt32(&args, type))
    {
        return false;
    }

    // see enum WeatherType
    if (!Weather::IsValidWeatherType(type))
    {
        return false;
    }

    float grade;
    if (!ExtractFloat(&args, grade))
    {
        return false;
    }

    // clamp grade from 0 to 1
    if (grade < 0.0f)
    {
        grade = 0.0f;
    }
    else if (grade > 1.0f)
    {
        grade = 1.0f;
    }

    Player* player = m_session->GetPlayer();
    uint32 zoneId = player->GetZoneId();
    if (!sWeatherMgr.GetWeatherChances(zoneId))
    {
        SendSysMessage(LANG_NO_WEATHER);
        SetSentErrorMessage(true);
    }
    player->GetMap()->SetWeather(zoneId, (WeatherType)type, grade, false);

    return true;
}

// Internal shortcut function to freeze a player
bool freezePlayer(Player* player, WorldObject* caster)
{
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(SPELL_GM_FREEZE);
    return AddAuraToPlayer(spellInfo, player, caster);
}

// Internal shortcut function to freeze a player
void unFreezePlayer(Player* player)
{
    player->RemoveAurasDueToSpell(SPELL_GM_FREEZE);
}


bool ChatHandler::HandleFreezePlayerCommand(char* args)
{
    Player* targetPlayer = nullptr;

    // 1. Try to extract player name from args if not empty
    if (*args)
    {
        char* playerName = ExtractLiteralArg(&args);

        if (!ExtractPlayerTarget(&playerName, &targetPlayer))
        {
            SendSysMessage(LANG_COMMAND_FREEZE_PLAYER_PLAYER_NOT_FOUND);
            SetSentErrorMessage(true);
            return false;
        }
    }

    // 2. If arg is empty, gets the current selected target (returns current player if no unit selected)
    if (!targetPlayer)
    {
        Unit* selectedTtarget = getSelectedPlayer();

        if (!selectedTtarget)
        {
            SendSysMessage(LANG_NO_CHAR_SELECTED);
            SetSentErrorMessage(true);
            return false;
        }
        targetPlayer = (Player*)selectedTtarget;
    }


    const char* targetName = targetPlayer->GetName();
    Player * currentGM = m_session->GetPlayer();

    // Prevent freezing yourself !
    if (targetPlayer == currentGM)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        SendSysMessage(LANG_COMMAND_FREEZE_PLAYER_CANNOT_FREEZE_YOURSELF);
        SetSentErrorMessage(true);
        return false;
    }

    // Check if target can be freezed
    if (targetPlayer->GetSession()->GetSecurity() > m_session->GetSecurity())
    {
        PSendSysMessage(LANG_COMMAND_FREEZE_PLAYER_CANNOT_FREEZE_HIGHER_SECLEVEL, targetName);
        SetSentErrorMessage(true);
        return false;
    }

    freezePlayer(targetPlayer, currentGM);

    // Notif GM
    PSendSysMessage(LANG_COMMAND_FREEZE_PLAYER, targetName);

    // Send message to player to prevent he has been frozen
    ChatHandler(targetPlayer).SendSysMessage(LANG_COMMAND_FREEZE_PLAYER_YOU_HAVE_BEEN_FROZEN);

    return true;
}


bool ChatHandler::HandleUnfreezePlayerCommand(char* args)
{

    Player* targetPlayer = nullptr;

    // 1. Try to extract player name from args if not empty
    if (*args)
    {
        char* playerName = ExtractLiteralArg(&args);

        if (!ExtractPlayerTarget(&playerName, &targetPlayer))
        {
            SendSysMessage(LANG_COMMAND_UNFREEZE_PLAYER_PLAYER_NOT_FOUND);
            SetSentErrorMessage(true);
            return false;
        }
    }

    // 2. If arg is empty, gets the current selected target (returns current player if no unit selected)
    if (!targetPlayer)
    {
        Unit* selectedTtarget = getSelectedPlayer();

        if (!selectedTtarget)
        {
            SendSysMessage(LANG_NO_CHAR_SELECTED);
            SetSentErrorMessage(true);
            return false;
        }
        targetPlayer = (Player*)selectedTtarget;
    }

    unFreezePlayer(targetPlayer);

    // Notif GM
    PSendSysMessage(LANG_COMMAND_UNFREEZE_PLAYER, targetPlayer->GetName());

    // Send message to player to prevent he has been unfrozen
    ChatHandler(targetPlayer).SendSysMessage(LANG_COMMAND_FREEZE_PLAYER_YOU_HAVE_BEEN_UNFROZEN);

    return true;
}

