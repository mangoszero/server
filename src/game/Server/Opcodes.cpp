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

/** \file
    \ingroup u2w
*/

#include "Opcodes.h"
#include "Policies/Singleton.h"

INSTANTIATE_SINGLETON_1(Opcodes);

OpcodeHandler const Opcodes::emptyHandler =
{
    "<none>",
    STATUS_UNHANDLED,
    PROCESS_INPLACE,
    &WorldSession::Handle_NULL
};


Opcodes::Opcodes()
{
    /// Build Opcodes map
    BuildOpcodeList();
}

Opcodes::~Opcodes()
{
    /// Clear Opcodes
    mOpcodeMap.clear();
}


/**
 * Registers all handlers for the different packets that may come in. See the source for more info.
 * \see Opcodes::StoreOpcode
 */
void Opcodes::BuildOpcodeList()
{
    /// Correspondence between opcodes and their names
    /*0x000*/  StoreOpcode(MSG_NULL_ACTION,                   "MSG_NULL_ACTION",                  STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x001*/  StoreOpcode(CMSG_BOOTME,                       "CMSG_BOOTME",                      STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x002*/  StoreOpcode(CMSG_DBLOOKUP,                     "CMSG_DBLOOKUP",                    STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x003*/  StoreOpcode(SMSG_DBLOOKUP,                     "SMSG_DBLOOKUP",                    STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x004*/  StoreOpcode(CMSG_QUERY_OBJECT_POSITION,        "CMSG_QUERY_OBJECT_POSITION",       STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x005*/  StoreOpcode(SMSG_QUERY_OBJECT_POSITION,        "SMSG_QUERY_OBJECT_POSITION",       STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x006*/  StoreOpcode(CMSG_QUERY_OBJECT_ROTATION,        "CMSG_QUERY_OBJECT_ROTATION",       STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x007*/  StoreOpcode(SMSG_QUERY_OBJECT_ROTATION,        "SMSG_QUERY_OBJECT_ROTATION",       STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x008*/  StoreOpcode(CMSG_WORLD_TELEPORT,               "CMSG_WORLD_TELEPORT",              STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleWorldTeleportOpcode);
    /*0x009*/  StoreOpcode(CMSG_TELEPORT_TO_UNIT,             "CMSG_TELEPORT_TO_UNIT",            STATUS_LOGGEDIN,  PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x00A*/  StoreOpcode(CMSG_ZONE_MAP,                     "CMSG_ZONE_MAP",                    STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x00B*/  StoreOpcode(SMSG_ZONE_MAP,                     "SMSG_ZONE_MAP",                    STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x00C*/  StoreOpcode(CMSG_DEBUG_CHANGECELLZONE,         "CMSG_DEBUG_CHANGECELLZONE",        STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x00D*/  StoreOpcode(CMSG_EMBLAZON_TABARD_OBSOLETE,     "CMSG_EMBLAZON_TABARD_OBSOLETE",    STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x00E*/  StoreOpcode(CMSG_UNEMBLAZON_TABARD_OBSOLETE,   "CMSG_UNEMBLAZON_TABARD_OBSOLETE",  STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x00F*/  StoreOpcode(CMSG_RECHARGE,                     "CMSG_RECHARGE",                    STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x010*/  StoreOpcode(CMSG_LEARN_SPELL,                  "CMSG_LEARN_SPELL",                 STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x011*/  StoreOpcode(CMSG_CREATEMONSTER,                "CMSG_CREATEMONSTER",               STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x012*/  StoreOpcode(CMSG_DESTROYMONSTER,               "CMSG_DESTROYMONSTER",              STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x013*/  StoreOpcode(CMSG_CREATEITEM,                   "CMSG_CREATEITEM",                  STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x014*/  StoreOpcode(CMSG_CREATEGAMEOBJECT,             "CMSG_CREATEGAMEOBJECT",            STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x015*/  StoreOpcode(SMSG_CHECK_FOR_BOTS,               "SMSG_CHECK_FOR_BOTS",              STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x016*/  StoreOpcode(CMSG_MAKEMONSTERATTACKGUID,        "CMSG_MAKEMONSTERATTACKGUID",       STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x017*/  StoreOpcode(CMSG_BOT_DETECTED2,                "CMSG_BOT_DETECTED2",               STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x018*/  StoreOpcode(CMSG_FORCEACTION,                  "CMSG_FORCEACTION",                 STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x019*/  StoreOpcode(CMSG_FORCEACTIONONOTHER,           "CMSG_FORCEACTIONONOTHER",          STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x01A*/  StoreOpcode(CMSG_FORCEACTIONSHOW,              "CMSG_FORCEACTIONSHOW",             STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x01B*/  StoreOpcode(SMSG_FORCEACTIONSHOW,              "SMSG_FORCEACTIONSHOW",             STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x01C*/  StoreOpcode(CMSG_PETGODMODE,                   "CMSG_PETGODMODE",                  STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x01D*/  StoreOpcode(SMSG_PETGODMODE,                   "SMSG_PETGODMODE",                  STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x01E*/  StoreOpcode(SMSG_DEBUGINFOSPELLMISS_OBSOLETE,  "SMSG_DEBUGINFOSPELLMISS_OBSOLETE", STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x01F*/  StoreOpcode(CMSG_WEATHER_SPEED_CHEAT,          "CMSG_WEATHER_SPEED_CHEAT",         STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x020*/  StoreOpcode(CMSG_UNDRESSPLAYER,                "CMSG_UNDRESSPLAYER",               STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x021*/  StoreOpcode(CMSG_BEASTMASTER,                  "CMSG_BEASTMASTER",                 STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x022*/  StoreOpcode(CMSG_GODMODE,                      "CMSG_GODMODE",                     STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x023*/  StoreOpcode(SMSG_GODMODE,                      "SMSG_GODMODE",                     STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x024*/  StoreOpcode(CMSG_CHEAT_SETMONEY,               "CMSG_CHEAT_SETMONEY",              STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x025*/  StoreOpcode(CMSG_LEVEL_CHEAT,                  "CMSG_LEVEL_CHEAT",                 STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x026*/  StoreOpcode(CMSG_PET_LEVEL_CHEAT,              "CMSG_PET_LEVEL_CHEAT",             STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x027*/  StoreOpcode(CMSG_SET_WORLDSTATE,               "CMSG_SET_WORLDSTATE",              STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x028*/  StoreOpcode(CMSG_COOLDOWN_CHEAT,               "CMSG_COOLDOWN_CHEAT",              STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x029*/  StoreOpcode(CMSG_USE_SKILL_CHEAT,              "CMSG_USE_SKILL_CHEAT",             STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x02A*/  StoreOpcode(CMSG_FLAG_QUEST,                   "CMSG_FLAG_QUEST",                  STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x02B*/  StoreOpcode(CMSG_FLAG_QUEST_FINISH,            "CMSG_FLAG_QUEST_FINISH",           STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x02C*/  StoreOpcode(CMSG_CLEAR_QUEST,                  "CMSG_CLEAR_QUEST",                 STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x02D*/  StoreOpcode(CMSG_SEND_EVENT,                   "CMSG_SEND_EVENT",                  STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x02E*/  StoreOpcode(CMSG_DEBUG_AISTATE,                "CMSG_DEBUG_AISTATE",               STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x02F*/  StoreOpcode(SMSG_DEBUG_AISTATE,                "SMSG_DEBUG_AISTATE",               STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x030*/  StoreOpcode(CMSG_DISABLE_PVP_CHEAT,            "CMSG_DISABLE_PVP_CHEAT",           STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x031*/  StoreOpcode(CMSG_ADVANCE_SPAWN_TIME,           "CMSG_ADVANCE_SPAWN_TIME",          STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x032*/  StoreOpcode(CMSG_PVP_PORT_OBSOLETE,            "CMSG_PVP_PORT_OBSOLETE",           STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x033*/  StoreOpcode(CMSG_AUTH_SRP6_BEGIN,              "CMSG_AUTH_SRP6_BEGIN",             STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x034*/  StoreOpcode(CMSG_AUTH_SRP6_PROOF,              "CMSG_AUTH_SRP6_PROOF",             STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x035*/  StoreOpcode(CMSG_AUTH_SRP6_RECODE,             "CMSG_AUTH_SRP6_RECODE",            STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x036*/  StoreOpcode(CMSG_CHAR_CREATE,                  "CMSG_CHAR_CREATE",                 STATUS_AUTHED,    PROCESS_THREADUNSAFE, &WorldSession::HandleCharCreateOpcode);
    /*0x037*/  StoreOpcode(CMSG_CHAR_ENUM,                    "CMSG_CHAR_ENUM",                   STATUS_AUTHED,    PROCESS_THREADUNSAFE, &WorldSession::HandleCharEnumOpcode);
    /*0x038*/  StoreOpcode(CMSG_CHAR_DELETE,                  "CMSG_CHAR_DELETE",                 STATUS_AUTHED,    PROCESS_THREADUNSAFE, &WorldSession::HandleCharDeleteOpcode);
    /*0x039*/  StoreOpcode(SMSG_AUTH_SRP6_RESPONSE,           "SMSG_AUTH_SRP6_RESPONSE",          STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x03A*/  StoreOpcode(SMSG_CHAR_CREATE,                  "SMSG_CHAR_CREATE",                 STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x03B*/  StoreOpcode(SMSG_CHAR_ENUM,                    "SMSG_CHAR_ENUM",                   STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x03C*/  StoreOpcode(SMSG_CHAR_DELETE,                  "SMSG_CHAR_DELETE",                 STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x03D*/  StoreOpcode(CMSG_PLAYER_LOGIN,                 "CMSG_PLAYER_LOGIN",                STATUS_AUTHED,    PROCESS_INPLACE,      &WorldSession::HandlePlayerLoginOpcode);
    /*0x03E*/  StoreOpcode(SMSG_NEW_WORLD,                    "SMSG_NEW_WORLD",                   STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x03F*/  StoreOpcode(SMSG_TRANSFER_PENDING,             "SMSG_TRANSFER_PENDING",            STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x040*/  StoreOpcode(SMSG_TRANSFER_ABORTED,             "SMSG_TRANSFER_ABORTED",            STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x041*/  StoreOpcode(SMSG_CHARACTER_LOGIN_FAILED,       "SMSG_CHARACTER_LOGIN_FAILED",      STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x042*/  StoreOpcode(SMSG_LOGIN_SETTIMESPEED,           "SMSG_LOGIN_SETTIMESPEED",          STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x043*/  StoreOpcode(SMSG_GAMETIME_UPDATE,              "SMSG_GAMETIME_UPDATE",             STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x044*/  StoreOpcode(CMSG_GAMETIME_SET,                 "CMSG_GAMETIME_SET",                STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x045*/  StoreOpcode(SMSG_GAMETIME_SET,                 "SMSG_GAMETIME_SET",                STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x046*/  StoreOpcode(CMSG_GAMESPEED_SET,                "CMSG_GAMESPEED_SET",               STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x047*/  StoreOpcode(SMSG_GAMESPEED_SET,                "SMSG_GAMESPEED_SET",               STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x048*/  StoreOpcode(CMSG_SERVERTIME,                   "CMSG_SERVERTIME",                  STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x049*/  StoreOpcode(SMSG_SERVERTIME,                   "SMSG_SERVERTIME",                  STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x04A*/  StoreOpcode(CMSG_PLAYER_LOGOUT,                "CMSG_PLAYER_LOGOUT",               STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandlePlayerLogoutOpcode);
    /*[-ZERO] Need check */ /*0x04B*/  StoreOpcode(CMSG_LOGOUT_REQUEST,               "CMSG_LOGOUT_REQUEST",              STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleLogoutRequestOpcode);
    /*0x04C*/  StoreOpcode(SMSG_LOGOUT_RESPONSE,              "SMSG_LOGOUT_RESPONSE",             STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x04D*/  StoreOpcode(SMSG_LOGOUT_COMPLETE,              "SMSG_LOGOUT_COMPLETE",             STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x04E*/  StoreOpcode(CMSG_LOGOUT_CANCEL,                "CMSG_LOGOUT_CANCEL",               STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleLogoutCancelOpcode);
    /*0x04F*/  StoreOpcode(SMSG_LOGOUT_CANCEL_ACK,            "SMSG_LOGOUT_CANCEL_ACK",           STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x050*/  StoreOpcode(CMSG_NAME_QUERY,                   "CMSG_NAME_QUERY",                  STATUS_AUTHED,    PROCESS_THREADUNSAFE, &WorldSession::HandleNameQueryOpcode);
    /*0x051*/  StoreOpcode(SMSG_NAME_QUERY_RESPONSE,          "SMSG_NAME_QUERY_RESPONSE",         STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x052*/  StoreOpcode(CMSG_PET_NAME_QUERY,               "CMSG_PET_NAME_QUERY",              STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandlePetNameQueryOpcode);
    /*0x053*/  StoreOpcode(SMSG_PET_NAME_QUERY_RESPONSE,      "SMSG_PET_NAME_QUERY_RESPONSE",     STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x054*/  StoreOpcode(CMSG_GUILD_QUERY,                  "CMSG_GUILD_QUERY",                 STATUS_AUTHED,    PROCESS_THREADUNSAFE, &WorldSession::HandleGuildQueryOpcode);
    /*0x055*/  StoreOpcode(SMSG_GUILD_QUERY_RESPONSE,         "SMSG_GUILD_QUERY_RESPONSE",        STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x056*/  StoreOpcode(CMSG_ITEM_QUERY_SINGLE,            "CMSG_ITEM_QUERY_SINGLE",           STATUS_LOGGEDIN,  PROCESS_INPLACE,      &WorldSession::HandleItemQuerySingleOpcode);
    /*0x057*/  StoreOpcode(CMSG_ITEM_QUERY_MULTIPLE,          "CMSG_ITEM_QUERY_MULTIPLE",         STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x058*/  StoreOpcode(SMSG_ITEM_QUERY_SINGLE_RESPONSE,   "SMSG_ITEM_QUERY_SINGLE_RESPONSE",  STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x059*/  StoreOpcode(SMSG_ITEM_QUERY_MULTIPLE_RESPONSE, "SMSG_ITEM_QUERY_MULTIPLE_RESPONSE", STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x05A*/  StoreOpcode(CMSG_PAGE_TEXT_QUERY,              "CMSG_PAGE_TEXT_QUERY",             STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandlePageTextQueryOpcode);
    /*0x05B*/  StoreOpcode(SMSG_PAGE_TEXT_QUERY_RESPONSE,     "SMSG_PAGE_TEXT_QUERY_RESPONSE",    STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x05C*/  StoreOpcode(CMSG_QUEST_QUERY,                  "CMSG_QUEST_QUERY",                 STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleQuestQueryOpcode);
    /*0x05D*/  StoreOpcode(SMSG_QUEST_QUERY_RESPONSE,         "SMSG_QUEST_QUERY_RESPONSE",        STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x05E*/  StoreOpcode(CMSG_GAMEOBJECT_QUERY,             "CMSG_GAMEOBJECT_QUERY",            STATUS_LOGGEDIN,  PROCESS_INPLACE,      &WorldSession::HandleGameObjectQueryOpcode);
    /*0x05F*/  StoreOpcode(SMSG_GAMEOBJECT_QUERY_RESPONSE,    "SMSG_GAMEOBJECT_QUERY_RESPONSE",   STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x060*/  StoreOpcode(CMSG_CREATURE_QUERY,               "CMSG_CREATURE_QUERY",              STATUS_LOGGEDIN,  PROCESS_INPLACE,      &WorldSession::HandleCreatureQueryOpcode);
    /*0x061*/  StoreOpcode(SMSG_CREATURE_QUERY_RESPONSE,      "SMSG_CREATURE_QUERY_RESPONSE",     STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x062*/  StoreOpcode(CMSG_WHO,                          "CMSG_WHO",                         STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleWhoOpcode);
    /*0x063*/  StoreOpcode(SMSG_WHO,                          "SMSG_WHO",                         STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x064*/  StoreOpcode(CMSG_WHOIS,                        "CMSG_WHOIS",                       STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleWhoisOpcode);
    /*0x065*/  StoreOpcode(SMSG_WHOIS,                        "SMSG_WHOIS",                       STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x066*/  StoreOpcode(CMSG_FRIEND_LIST,                  "CMSG_FRIEND_LIST",                 STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleFriendListOpcode);
    /*0x067*/  StoreOpcode(SMSG_FRIEND_LIST,                  "SMSG_FRIEND_LIST",                 STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x068*/  StoreOpcode(SMSG_FRIEND_STATUS,                "SMSG_FRIEND_STATUS",               STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x069*/  StoreOpcode(CMSG_ADD_FRIEND,                   "CMSG_ADD_FRIEND",                  STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleAddFriendOpcode);
    /*0x06A*/  StoreOpcode(CMSG_DEL_FRIEND,                   "CMSG_DEL_FRIEND",                  STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleDelFriendOpcode);
    /*0x06B*/  StoreOpcode(SMSG_IGNORE_LIST,                  "SMSG_IGNORE_LIST",                 STATUS_NEVER,     PROCESS_THREADUNSAFE, &WorldSession::Handle_ServerSide);
    /*0x06C*/  StoreOpcode(CMSG_ADD_IGNORE,                   "CMSG_ADD_IGNORE",                  STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleAddIgnoreOpcode);
    /*0x06D*/  StoreOpcode(CMSG_DEL_IGNORE,                   "CMSG_DEL_IGNORE",                  STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleDelIgnoreOpcode);
    /*[-ZERO] Need check */ /*0x06E*/  StoreOpcode(CMSG_GROUP_INVITE,                 "CMSG_GROUP_INVITE",                STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleGroupInviteOpcode);
    /*0x06F*/  StoreOpcode(SMSG_GROUP_INVITE,                 "SMSG_GROUP_INVITE",                STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x070*/  StoreOpcode(CMSG_GROUP_CANCEL,                 "CMSG_GROUP_CANCEL",                STATUS_LOGGEDIN,  PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x071*/  StoreOpcode(SMSG_GROUP_CANCEL,                 "SMSG_GROUP_CANCEL",                STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x072*/  StoreOpcode(CMSG_GROUP_ACCEPT,                 "CMSG_GROUP_ACCEPT",                STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleGroupAcceptOpcode);
    /*[-ZERO] Need check */ /*0x073*/  StoreOpcode(CMSG_GROUP_DECLINE,                "CMSG_GROUP_DECLINE",               STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleGroupDeclineOpcode);
    /*0x074*/  StoreOpcode(SMSG_GROUP_DECLINE,                "SMSG_GROUP_DECLINE",               STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x075*/  StoreOpcode(CMSG_GROUP_UNINVITE,               "CMSG_GROUP_UNINVITE",              STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleGroupUninviteOpcode);
    /*[-ZERO] Need check */ /*0x076*/  StoreOpcode(CMSG_GROUP_UNINVITE_GUID,          "CMSG_GROUP_UNINVITE_GUID",         STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleGroupUninviteGuidOpcode);
    /*0x077*/  StoreOpcode(SMSG_GROUP_UNINVITE,               "SMSG_GROUP_UNINVITE",              STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x078*/  StoreOpcode(CMSG_GROUP_SET_LEADER,             "CMSG_GROUP_SET_LEADER",            STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleGroupSetLeaderOpcode);
    /*0x079*/  StoreOpcode(SMSG_GROUP_SET_LEADER,             "SMSG_GROUP_SET_LEADER",            STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x07A*/  StoreOpcode(CMSG_LOOT_METHOD,                  "CMSG_LOOT_METHOD",                 STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleLootMethodOpcode);
    /*[-ZERO] Need check */ /*0x07B*/  StoreOpcode(CMSG_GROUP_DISBAND,                "CMSG_GROUP_DISBAND",               STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleGroupDisbandOpcode);
    /*0x07C*/  StoreOpcode(SMSG_GROUP_DESTROYED,              "SMSG_GROUP_DESTROYED",             STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x07D*/  StoreOpcode(SMSG_GROUP_LIST,                   "SMSG_GROUP_LIST",                  STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x07E*/  StoreOpcode(SMSG_PARTY_MEMBER_STATS,           "SMSG_PARTY_MEMBER_STATS",          STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x07F*/  StoreOpcode(SMSG_PARTY_COMMAND_RESULT,         "SMSG_PARTY_COMMAND_RESULT",        STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x080*/  StoreOpcode(UMSG_UPDATE_GROUP_MEMBERS,         "UMSG_UPDATE_GROUP_MEMBERS",        STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*[-ZERO] Need check */ /*0x081*/  StoreOpcode(CMSG_GUILD_CREATE,                 "CMSG_GUILD_CREATE",                STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleGuildCreateOpcode);
    /*[-ZERO] Need check */ /*0x082*/  StoreOpcode(CMSG_GUILD_INVITE,                 "CMSG_GUILD_INVITE",                STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleGuildInviteOpcode);
    /*0x083*/  StoreOpcode(SMSG_GUILD_INVITE,                 "SMSG_GUILD_INVITE",                STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x084*/  StoreOpcode(CMSG_GUILD_ACCEPT,                 "CMSG_GUILD_ACCEPT",                STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleGuildAcceptOpcode);
    /*[-ZERO] Need check */ /*0x085*/  StoreOpcode(CMSG_GUILD_DECLINE,                "CMSG_GUILD_DECLINE",               STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleGuildDeclineOpcode);
    /*0x086*/  StoreOpcode(SMSG_GUILD_DECLINE,                "SMSG_GUILD_DECLINE",               STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x087*/  StoreOpcode(CMSG_GUILD_INFO,                   "CMSG_GUILD_INFO",                  STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleGuildInfoOpcode);
    /*0x088*/  StoreOpcode(SMSG_GUILD_INFO,                   "SMSG_GUILD_INFO",                  STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x089*/  StoreOpcode(CMSG_GUILD_ROSTER,                 "CMSG_GUILD_ROSTER",                STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleGuildRosterOpcode);
    /*0x08A*/  StoreOpcode(SMSG_GUILD_ROSTER,                 "SMSG_GUILD_ROSTER",                STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x08B*/  StoreOpcode(CMSG_GUILD_PROMOTE,                "CMSG_GUILD_PROMOTE",               STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleGuildPromoteOpcode);
    /*[-ZERO] Need check */ /*0x08C*/  StoreOpcode(CMSG_GUILD_DEMOTE,                 "CMSG_GUILD_DEMOTE",                STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleGuildDemoteOpcode);
    /*[-ZERO] Need check */ /*0x08D*/  StoreOpcode(CMSG_GUILD_LEAVE,                  "CMSG_GUILD_LEAVE",                 STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleGuildLeaveOpcode);
    /*[-ZERO] Need check */ /*0x08E*/  StoreOpcode(CMSG_GUILD_REMOVE,                 "CMSG_GUILD_REMOVE",                STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleGuildRemoveOpcode);
    /*[-ZERO] Need check */ /*0x08F*/  StoreOpcode(CMSG_GUILD_DISBAND,                "CMSG_GUILD_DISBAND",               STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleGuildDisbandOpcode);
    /*[-ZERO] Need check */ /*0x090*/  StoreOpcode(CMSG_GUILD_LEADER,                 "CMSG_GUILD_LEADER",                STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleGuildLeaderOpcode);
    /*[-ZERO] Need check */ /*0x091*/  StoreOpcode(CMSG_GUILD_MOTD,                   "CMSG_GUILD_MOTD",                  STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleGuildMOTDOpcode);
    /*0x092*/  StoreOpcode(SMSG_GUILD_EVENT,                  "SMSG_GUILD_EVENT",                 STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x093*/  StoreOpcode(SMSG_GUILD_COMMAND_RESULT,         "SMSG_GUILD_COMMAND_RESULT",        STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x094*/  StoreOpcode(UMSG_UPDATE_GUILD,                 "UMSG_UPDATE_GUILD",                STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x095*/  StoreOpcode(CMSG_MESSAGECHAT,                  "CMSG_MESSAGECHAT",                 STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleMessagechatOpcode);
    /*0x096*/  StoreOpcode(SMSG_MESSAGECHAT,                  "SMSG_MESSAGECHAT",                 STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x097*/  StoreOpcode(CMSG_JOIN_CHANNEL,                 "CMSG_JOIN_CHANNEL",                STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleJoinChannelOpcode);
    /*[-ZERO] Need check */ /*0x098*/  StoreOpcode(CMSG_LEAVE_CHANNEL,                "CMSG_LEAVE_CHANNEL",               STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleLeaveChannelOpcode);
    /*0x099*/  StoreOpcode(SMSG_CHANNEL_NOTIFY,               "SMSG_CHANNEL_NOTIFY",              STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x09A*/  StoreOpcode(CMSG_CHANNEL_LIST,                 "CMSG_CHANNEL_LIST",                STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleChannelListOpcode);
    /*0x09B*/  StoreOpcode(SMSG_CHANNEL_LIST,                 "SMSG_CHANNEL_LIST",                STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x09C*/  StoreOpcode(CMSG_CHANNEL_PASSWORD,             "CMSG_CHANNEL_PASSWORD",            STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleChannelPasswordOpcode);
    /*[-ZERO] Need check */ /*0x09D*/  StoreOpcode(CMSG_CHANNEL_SET_OWNER,            "CMSG_CHANNEL_SET_OWNER",           STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleChannelSetOwnerOpcode);
    /*[-ZERO] Need check */ /*0x09E*/  StoreOpcode(CMSG_CHANNEL_OWNER,                "CMSG_CHANNEL_OWNER",               STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleChannelOwnerOpcode);
    /*[-ZERO] Need check */ /*0x09F*/  StoreOpcode(CMSG_CHANNEL_MODERATOR,            "CMSG_CHANNEL_MODERATOR",           STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleChannelModeratorOpcode);
    /*[-ZERO] Need check */ /*0x0A0*/  StoreOpcode(CMSG_CHANNEL_UNMODERATOR,          "CMSG_CHANNEL_UNMODERATOR",         STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleChannelUnmoderatorOpcode);
    /*[-ZERO] Need check */ /*0x0A1*/  StoreOpcode(CMSG_CHANNEL_MUTE,                 "CMSG_CHANNEL_MUTE",                STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleChannelMuteOpcode);
    /*[-ZERO] Need check */ /*0x0A2*/  StoreOpcode(CMSG_CHANNEL_UNMUTE,               "CMSG_CHANNEL_UNMUTE",              STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleChannelUnmuteOpcode);
    /*[-ZERO] Need check */ /*0x0A3*/  StoreOpcode(CMSG_CHANNEL_INVITE,               "CMSG_CHANNEL_INVITE",              STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleChannelInviteOpcode);
    /*[-ZERO] Need check */ /*0x0A4*/  StoreOpcode(CMSG_CHANNEL_KICK,                 "CMSG_CHANNEL_KICK",                STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleChannelKickOpcode);
    /*[-ZERO] Need check */ /*0x0A5*/  StoreOpcode(CMSG_CHANNEL_BAN,                  "CMSG_CHANNEL_BAN",                 STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleChannelBanOpcode);
    /*[-ZERO] Need check */ /*0x0A6*/  StoreOpcode(CMSG_CHANNEL_UNBAN,                "CMSG_CHANNEL_UNBAN",               STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleChannelUnbanOpcode);
    /*[-ZERO] Need check */ /*0x0A7*/  StoreOpcode(CMSG_CHANNEL_ANNOUNCEMENTS,        "CMSG_CHANNEL_ANNOUNCEMENTS",       STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleChannelAnnouncementsOpcode);
    /*[-ZERO] Need check */ /*0x0A8*/  StoreOpcode(CMSG_CHANNEL_MODERATE,             "CMSG_CHANNEL_MODERATE",            STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleChannelModerateOpcode);
    /*0x0A9*/  StoreOpcode(SMSG_UPDATE_OBJECT,                "SMSG_UPDATE_OBJECT",               STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x0AA*/  StoreOpcode(SMSG_DESTROY_OBJECT,               "SMSG_DESTROY_OBJECT",              STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x0AB*/  StoreOpcode(CMSG_USE_ITEM,                     "CMSG_USE_ITEM",                    STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleUseItemOpcode);
    /*[-ZERO] Need check */ /*0x0AC*/  StoreOpcode(CMSG_OPEN_ITEM,                    "CMSG_OPEN_ITEM",                   STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleOpenItemOpcode);
    /*[-ZERO] Need check */ /*0x0AD*/  StoreOpcode(CMSG_READ_ITEM,                    "CMSG_READ_ITEM",                   STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleReadItemOpcode);
    /*0x0AE*/  StoreOpcode(SMSG_READ_ITEM_OK,                 "SMSG_READ_ITEM_OK",                STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x0AF*/  StoreOpcode(SMSG_READ_ITEM_FAILED,             "SMSG_READ_ITEM_FAILED",            STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x0B0*/  StoreOpcode(SMSG_ITEM_COOLDOWN,                "SMSG_ITEM_COOLDOWN",               STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x0B1*/  StoreOpcode(CMSG_GAMEOBJ_USE,                  "CMSG_GAMEOBJ_USE",                 STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleGameObjectUseOpcode);
    /*0x0B2*/  StoreOpcode(CMSG_GAMEOBJ_CHAIR_USE_OBSOLETE,   "CMSG_GAMEOBJ_CHAIR_USE_OBSOLETE",  STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x0B3*/  StoreOpcode(SMSG_GAMEOBJECT_CUSTOM_ANIM,       "SMSG_GAMEOBJECT_CUSTOM_ANIM",      STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x0B4*/  StoreOpcode(CMSG_AREATRIGGER,                  "CMSG_AREATRIGGER",                 STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE,      &WorldSession::HandleAreaTriggerOpcode);
    /*0x0B5*/  StoreOpcode(MSG_MOVE_START_FORWARD,            "MSG_MOVE_START_FORWARD",           STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleMovementOpcodes);
    /*0x0B6*/  StoreOpcode(MSG_MOVE_START_BACKWARD,           "MSG_MOVE_START_BACKWARD",          STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleMovementOpcodes);
    /*0x0B7*/  StoreOpcode(MSG_MOVE_STOP,                     "MSG_MOVE_STOP",                    STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleMovementOpcodes);
    /*0x0B8*/  StoreOpcode(MSG_MOVE_START_STRAFE_LEFT,        "MSG_MOVE_START_STRAFE_LEFT",       STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleMovementOpcodes);
    /*0x0B9*/  StoreOpcode(MSG_MOVE_START_STRAFE_RIGHT,       "MSG_MOVE_START_STRAFE_RIGHT",      STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleMovementOpcodes);
    /*0x0BA*/  StoreOpcode(MSG_MOVE_STOP_STRAFE,              "MSG_MOVE_STOP_STRAFE",             STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleMovementOpcodes);
    /*0x0BB*/  StoreOpcode(MSG_MOVE_JUMP,                     "MSG_MOVE_JUMP",                    STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleMovementOpcodes);
    /*0x0BC*/  StoreOpcode(MSG_MOVE_START_TURN_LEFT,          "MSG_MOVE_START_TURN_LEFT",         STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleMovementOpcodes);
    /*0x0BD*/  StoreOpcode(MSG_MOVE_START_TURN_RIGHT,         "MSG_MOVE_START_TURN_RIGHT",        STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleMovementOpcodes);
    /*0x0BE*/  StoreOpcode(MSG_MOVE_STOP_TURN,                "MSG_MOVE_STOP_TURN",               STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleMovementOpcodes);
    /*0x0BF*/  StoreOpcode(MSG_MOVE_START_PITCH_UP,           "MSG_MOVE_START_PITCH_UP",          STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleMovementOpcodes);
    /*0x0C0*/  StoreOpcode(MSG_MOVE_START_PITCH_DOWN,         "MSG_MOVE_START_PITCH_DOWN",        STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleMovementOpcodes);
    /*0x0C1*/  StoreOpcode(MSG_MOVE_STOP_PITCH,               "MSG_MOVE_STOP_PITCH",              STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleMovementOpcodes);
    /*0x0C2*/  StoreOpcode(MSG_MOVE_SET_RUN_MODE,             "MSG_MOVE_SET_RUN_MODE",            STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleMovementOpcodes);
    /*0x0C3*/  StoreOpcode(MSG_MOVE_SET_WALK_MODE,            "MSG_MOVE_SET_WALK_MODE",           STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleMovementOpcodes);
    /*0x0C4*/  StoreOpcode(MSG_MOVE_TOGGLE_LOGGING,           "MSG_MOVE_TOGGLE_LOGGING",          STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x0C5*/  StoreOpcode(MSG_MOVE_TELEPORT,                 "MSG_MOVE_TELEPORT",                STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x0C6*/  StoreOpcode(MSG_MOVE_TELEPORT_CHEAT,           "MSG_MOVE_TELEPORT_CHEAT",          STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x0C7*/  StoreOpcode(MSG_MOVE_TELEPORT_ACK,             "MSG_MOVE_TELEPORT_ACK",            STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleMoveTeleportAckOpcode);
    /*0x0C8*/  StoreOpcode(MSG_MOVE_TOGGLE_FALL_LOGGING,      "MSG_MOVE_TOGGLE_FALL_LOGGING",     STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x0C9*/  StoreOpcode(MSG_MOVE_FALL_LAND,                "MSG_MOVE_FALL_LAND",               STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleMovementOpcodes);
    /*0x0CA*/  StoreOpcode(MSG_MOVE_START_SWIM,               "MSG_MOVE_START_SWIM",              STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleMovementOpcodes);
    /*0x0CB*/  StoreOpcode(MSG_MOVE_STOP_SWIM,                "MSG_MOVE_STOP_SWIM",               STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleMovementOpcodes);
    /*0x0CC*/  StoreOpcode(MSG_MOVE_SET_RUN_SPEED_CHEAT,      "MSG_MOVE_SET_RUN_SPEED_CHEAT",     STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x0CD*/  StoreOpcode(MSG_MOVE_SET_RUN_SPEED,            "MSG_MOVE_SET_RUN_SPEED",           STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x0CE*/  StoreOpcode(MSG_MOVE_SET_RUN_BACK_SPEED_CHEAT, "MSG_MOVE_SET_RUN_BACK_SPEED_CHEAT", STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x0CF*/  StoreOpcode(MSG_MOVE_SET_RUN_BACK_SPEED,       "MSG_MOVE_SET_RUN_BACK_SPEED",      STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x0D0*/  StoreOpcode(MSG_MOVE_SET_WALK_SPEED_CHEAT,     "MSG_MOVE_SET_WALK_SPEED_CHEAT",    STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x0D1*/  StoreOpcode(MSG_MOVE_SET_WALK_SPEED,           "MSG_MOVE_SET_WALK_SPEED",          STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x0D2*/  StoreOpcode(MSG_MOVE_SET_SWIM_SPEED_CHEAT,     "MSG_MOVE_SET_SWIM_SPEED_CHEAT",    STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x0D3*/  StoreOpcode(MSG_MOVE_SET_SWIM_SPEED,           "MSG_MOVE_SET_SWIM_SPEED",          STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x0D4*/  StoreOpcode(MSG_MOVE_SET_SWIM_BACK_SPEED_CHEAT, "MSG_MOVE_SET_SWIM_BACK_SPEED_CHEAT",      STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x0D5*/  StoreOpcode(MSG_MOVE_SET_SWIM_BACK_SPEED,      "MSG_MOVE_SET_SWIM_BACK_SPEED",     STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x0D6*/  StoreOpcode(MSG_MOVE_SET_ALL_SPEED_CHEAT,      "MSG_MOVE_SET_ALL_SPEED_CHEAT",     STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x0D7*/  StoreOpcode(MSG_MOVE_SET_TURN_RATE_CHEAT,      "MSG_MOVE_SET_TURN_RATE_CHEAT",     STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*[-ZERO] No effect */ /*0x0D8*/  StoreOpcode(MSG_MOVE_SET_TURN_RATE,            "MSG_MOVE_SET_TURN_RATE",           STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x0D9*/  StoreOpcode(MSG_MOVE_TOGGLE_COLLISION_CHEAT,   "MSG_MOVE_TOGGLE_COLLISION_CHEAT",  STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x0DA*/  StoreOpcode(MSG_MOVE_SET_FACING,               "MSG_MOVE_SET_FACING",              STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleMovementOpcodes);
    /*0x0DB*/  StoreOpcode(MSG_MOVE_SET_PITCH,                "MSG_MOVE_SET_PITCH",               STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleMovementOpcodes);
    /*0x0DC*/  StoreOpcode(MSG_MOVE_WORLDPORT_ACK,            "MSG_MOVE_WORLDPORT_ACK",           STATUS_TRANSFER,  PROCESS_THREADUNSAFE, &WorldSession::HandleMoveWorldportAckOpcode);
    /*0x0DD*/  StoreOpcode(SMSG_MONSTER_MOVE,                 "SMSG_MONSTER_MOVE",                STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x0DE*/  StoreOpcode(SMSG_MOVE_WATER_WALK,              "SMSG_MOVE_WATER_WALK",             STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x0DF*/  StoreOpcode(SMSG_MOVE_LAND_WALK,               "SMSG_MOVE_LAND_WALK",              STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x0E0*/  StoreOpcode(MSG_MOVE_SET_RAW_POSITION_ACK,     "MSG_MOVE_SET_RAW_POSITION_ACK",    STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x0E1*/  StoreOpcode(CMSG_MOVE_SET_RAW_POSITION,        "CMSG_MOVE_SET_RAW_POSITION",       STATUS_LOGGEDIN,     PROCESS_THREADUNSAFE,      &WorldSession::HandleMoveSetRawPosition);
    /*0x0E2*/  StoreOpcode(SMSG_FORCE_RUN_SPEED_CHANGE,       "SMSG_FORCE_RUN_SPEED_CHANGE",      STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x0E3*/  StoreOpcode(CMSG_FORCE_RUN_SPEED_CHANGE_ACK,   "CMSG_FORCE_RUN_SPEED_CHANGE_ACK",  STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleForceSpeedChangeAckOpcodes);
    /*0x0E4*/  StoreOpcode(SMSG_FORCE_RUN_BACK_SPEED_CHANGE,  "SMSG_FORCE_RUN_BACK_SPEED_CHANGE", STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x0E5*/  StoreOpcode(CMSG_FORCE_RUN_BACK_SPEED_CHANGE_ACK, "CMSG_FORCE_RUN_BACK_SPEED_CHANGE_ACK", STATUS_LOGGEDIN, PROCESS_THREADSAFE, &WorldSession::HandleForceSpeedChangeAckOpcodes);
    /*0x0E6*/  StoreOpcode(SMSG_FORCE_SWIM_SPEED_CHANGE,      "SMSG_FORCE_SWIM_SPEED_CHANGE",     STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x0E7*/  StoreOpcode(CMSG_FORCE_SWIM_SPEED_CHANGE_ACK,  "CMSG_FORCE_SWIM_SPEED_CHANGE_ACK", STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleForceSpeedChangeAckOpcodes);
    /*0x0E8*/  StoreOpcode(SMSG_FORCE_MOVE_ROOT,              "SMSG_FORCE_MOVE_ROOT",             STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x0E9*/  StoreOpcode(CMSG_FORCE_MOVE_ROOT_ACK,          "CMSG_FORCE_MOVE_ROOT_ACK",         STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleMoveRootAck);
    /*0x0EA*/  StoreOpcode(SMSG_FORCE_MOVE_UNROOT,            "SMSG_FORCE_MOVE_UNROOT",           STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x0EB*/  StoreOpcode(CMSG_FORCE_MOVE_UNROOT_ACK,        "CMSG_FORCE_MOVE_UNROOT_ACK",       STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleMoveUnRootAck);
    /*0x0EC*/  StoreOpcode(MSG_MOVE_ROOT,                     "MSG_MOVE_ROOT",                    STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x0ED*/  StoreOpcode(MSG_MOVE_UNROOT,                   "MSG_MOVE_UNROOT",                  STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x0EE*/  StoreOpcode(MSG_MOVE_HEARTBEAT,                "MSG_MOVE_HEARTBEAT",               STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleMovementOpcodes);
    /*[-ZERO] Need check */ /*0x0EF*/  StoreOpcode(SMSG_MOVE_KNOCK_BACK,              "SMSG_MOVE_KNOCK_BACK",             STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x0F0*/  StoreOpcode(CMSG_MOVE_KNOCK_BACK_ACK,          "CMSG_MOVE_KNOCK_BACK_ACK",         STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleMoveKnockBackAck);
    /*[-ZERO] Need check */ /*0x0F1*/  StoreOpcode(MSG_MOVE_KNOCK_BACK,               "MSG_MOVE_KNOCK_BACK",              STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x0F2*/  StoreOpcode(SMSG_MOVE_FEATHER_FALL,            "SMSG_MOVE_FEATHER_FALL",           STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x0F3*/  StoreOpcode(SMSG_MOVE_NORMAL_FALL,             "SMSG_MOVE_NORMAL_FALL",            STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x0F4*/  StoreOpcode(SMSG_MOVE_SET_HOVER,               "SMSG_MOVE_SET_HOVER",              STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x0F5*/  StoreOpcode(SMSG_MOVE_UNSET_HOVER,             "SMSG_MOVE_UNSET_HOVER",            STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x0F6*/  StoreOpcode(CMSG_MOVE_HOVER_ACK,               "CMSG_MOVE_HOVER_ACK",              STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleMoveHoverAck);
    /*0x0F7*/  StoreOpcode(MSG_MOVE_HOVER,                    "MSG_MOVE_HOVER",                   STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x0F8*/  StoreOpcode(CMSG_TRIGGER_CINEMATIC_CHEAT,      "CMSG_TRIGGER_CINEMATIC_CHEAT",     STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x0F9*/  StoreOpcode(CMSG_OPENING_CINEMATIC,            "CMSG_OPENING_CINEMATIC",           STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x0FA*/  StoreOpcode(SMSG_TRIGGER_CINEMATIC,            "SMSG_TRIGGER_CINEMATIC",           STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x0FB*/  StoreOpcode(CMSG_NEXT_CINEMATIC_CAMERA,        "CMSG_NEXT_CINEMATIC_CAMERA",       STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleNextCinematicCamera);
    /*0x0FC*/  StoreOpcode(CMSG_COMPLETE_CINEMATIC,           "CMSG_COMPLETE_CINEMATIC",          STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleCompleteCinematic);
    /*0x0FD*/  StoreOpcode(SMSG_TUTORIAL_FLAGS,               "SMSG_TUTORIAL_FLAGS",              STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x0FE*/  StoreOpcode(CMSG_TUTORIAL_FLAG,                "CMSG_TUTORIAL_FLAG",               STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleTutorialFlagOpcode);
    /*0x0FF*/  StoreOpcode(CMSG_TUTORIAL_CLEAR,               "CMSG_TUTORIAL_CLEAR",              STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleTutorialClearOpcode);
    /*0x100*/  StoreOpcode(CMSG_TUTORIAL_RESET,               "CMSG_TUTORIAL_RESET",              STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleTutorialResetOpcode);
    /*0x101*/  StoreOpcode(CMSG_STANDSTATECHANGE,             "CMSG_STANDSTATECHANGE",            STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleStandStateChangeOpcode);
    /*[-ZERO] Need check */ /*0x102*/  StoreOpcode(CMSG_EMOTE,                        "CMSG_EMOTE",                       STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleEmoteOpcode);
    /*0x103*/  StoreOpcode(SMSG_EMOTE,                        "SMSG_EMOTE",                       STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x104*/  StoreOpcode(CMSG_TEXT_EMOTE,                   "CMSG_TEXT_EMOTE",                  STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleTextEmoteOpcode);
    /*0x105*/  StoreOpcode(SMSG_TEXT_EMOTE,                   "SMSG_TEXT_EMOTE",                  STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x106*/  StoreOpcode(CMSG_AUTOEQUIP_GROUND_ITEM,        "CMSG_AUTOEQUIP_GROUND_ITEM",       STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x107*/  StoreOpcode(CMSG_AUTOSTORE_GROUND_ITEM,        "CMSG_AUTOSTORE_GROUND_ITEM",       STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x108*/  StoreOpcode(CMSG_AUTOSTORE_LOOT_ITEM,          "CMSG_AUTOSTORE_LOOT_ITEM",         STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleAutostoreLootItemOpcode);
    /*0x109*/  StoreOpcode(CMSG_STORE_LOOT_IN_SLOT,           "CMSG_STORE_LOOT_IN_SLOT",          STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x10A*/  StoreOpcode(CMSG_AUTOEQUIP_ITEM,               "CMSG_AUTOEQUIP_ITEM",              STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleAutoEquipItemOpcode);
    /*0x10B*/  StoreOpcode(CMSG_AUTOSTORE_BAG_ITEM,           "CMSG_AUTOSTORE_BAG_ITEM",          STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleAutoStoreBagItemOpcode);
    /*0x10C*/  StoreOpcode(CMSG_SWAP_ITEM,                    "CMSG_SWAP_ITEM",                   STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleSwapItem);
    /*0x10D*/  StoreOpcode(CMSG_SWAP_INV_ITEM,                "CMSG_SWAP_INV_ITEM",               STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleSwapInvItemOpcode);
    /*0x10E*/  StoreOpcode(CMSG_SPLIT_ITEM,                   "CMSG_SPLIT_ITEM",                  STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleSplitItemOpcode);
    /*0x10F*/  StoreOpcode(CMSG_AUTOEQUIP_ITEM_SLOT,          "CMSG_AUTOEQUIP_ITEM_SLOT",         STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleAutoEquipItemSlotOpcode);
    /*0x110*/  StoreOpcode(OBSOLETE_DROP_ITEM,                "OBSOLETE_DROP_ITEM",               STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x111*/  StoreOpcode(CMSG_DESTROYITEM,                  "CMSG_DESTROYITEM",                 STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleDestroyItemOpcode);
    /*0x112*/  StoreOpcode(SMSG_INVENTORY_CHANGE_FAILURE,     "SMSG_INVENTORY_CHANGE_FAILURE",    STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x113*/  StoreOpcode(SMSG_OPEN_CONTAINER,               "SMSG_OPEN_CONTAINER",              STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x114*/  StoreOpcode(CMSG_INSPECT,                      "CMSG_INSPECT",                     STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleInspectOpcode);
    /*0x115*/  StoreOpcode(SMSG_INSPECT,                      "SMSG_INSPECT",                     STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x116*/  StoreOpcode(CMSG_INITIATE_TRADE,               "CMSG_INITIATE_TRADE",              STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleInitiateTradeOpcode);
    /*[-ZERO] Need check */ /*0x117*/  StoreOpcode(CMSG_BEGIN_TRADE,                  "CMSG_BEGIN_TRADE",                 STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleBeginTradeOpcode);
    /*[-ZERO] Need check */ /*0x118*/  StoreOpcode(CMSG_BUSY_TRADE,                   "CMSG_BUSY_TRADE",                  STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleBusyTradeOpcode);
    /*[-ZERO] Need check */ /*0x119*/  StoreOpcode(CMSG_IGNORE_TRADE,                 "CMSG_IGNORE_TRADE",                STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleIgnoreTradeOpcode);
    /*[-ZERO] Need check */ /*0x11A*/  StoreOpcode(CMSG_ACCEPT_TRADE,                 "CMSG_ACCEPT_TRADE",                STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleAcceptTradeOpcode);
    /*[-ZERO] Need check */ /*0x11B*/  StoreOpcode(CMSG_UNACCEPT_TRADE,               "CMSG_UNACCEPT_TRADE",              STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleUnacceptTradeOpcode);
    /*[-ZERO] Need check */ /*0x11C*/  StoreOpcode(CMSG_CANCEL_TRADE,                 "CMSG_CANCEL_TRADE",                STATUS_LOGGEDIN_OR_RECENTLY_LOGGEDOUT, PROCESS_THREADUNSAFE, &WorldSession::HandleCancelTradeOpcode);
    /*[-ZERO] Need check */ /*0x11D*/  StoreOpcode(CMSG_SET_TRADE_ITEM,               "CMSG_SET_TRADE_ITEM",              STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleSetTradeItemOpcode);
    /*[-ZERO] Need check */ /*0x11E*/  StoreOpcode(CMSG_CLEAR_TRADE_ITEM,             "CMSG_CLEAR_TRADE_ITEM",            STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleClearTradeItemOpcode);
    /*[-ZERO] Need check */ /*0x11F*/  StoreOpcode(CMSG_SET_TRADE_GOLD,               "CMSG_SET_TRADE_GOLD",              STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleSetTradeGoldOpcode);
    /*0x120*/  StoreOpcode(SMSG_TRADE_STATUS,                 "SMSG_TRADE_STATUS",                STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x121*/  StoreOpcode(SMSG_TRADE_STATUS_EXTENDED,        "SMSG_TRADE_STATUS_EXTENDED",       STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x122*/  StoreOpcode(SMSG_INITIALIZE_FACTIONS,          "SMSG_INITIALIZE_FACTIONS",         STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x123*/  StoreOpcode(SMSG_SET_FACTION_VISIBLE,          "SMSG_SET_FACTION_VISIBLE",         STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x124*/  StoreOpcode(SMSG_SET_FACTION_STANDING,         "SMSG_SET_FACTION_STANDING",        STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x125*/  StoreOpcode(CMSG_SET_FACTION_ATWAR,            "CMSG_SET_FACTION_ATWAR",           STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleSetFactionAtWarOpcode);
    /*0x126*/  StoreOpcode(CMSG_SET_FACTION_CHEAT,            "CMSG_SET_FACTION_CHEAT",           STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_Deprecated);
    /*0x127*/  StoreOpcode(SMSG_SET_PROFICIENCY,              "SMSG_SET_PROFICIENCY",             STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x128*/  StoreOpcode(CMSG_SET_ACTION_BUTTON,            "CMSG_SET_ACTION_BUTTON",           STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleSetActionButtonOpcode);
    /*0x129*/  StoreOpcode(SMSG_ACTION_BUTTONS,               "SMSG_ACTION_BUTTONS",              STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x12A*/  StoreOpcode(SMSG_INITIAL_SPELLS,               "SMSG_INITIAL_SPELLS",              STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x12B*/  StoreOpcode(SMSG_LEARNED_SPELL,                "SMSG_LEARNED_SPELL",               STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x12C*/  StoreOpcode(SMSG_SUPERCEDED_SPELL,             "SMSG_SUPERCEDED_SPELL",            STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x12D*/  StoreOpcode(CMSG_NEW_SPELL_SLOT,               "CMSG_NEW_SPELL_SLOT",              STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x12E*/  StoreOpcode(CMSG_CAST_SPELL,                   "CMSG_CAST_SPELL",                  STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleCastSpellOpcode);
    /*0x12F*/  StoreOpcode(CMSG_CANCEL_CAST,                  "CMSG_CANCEL_CAST",                 STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleCancelCastOpcode);
    /*[-ZERO] Need check */ /*0x130*/  StoreOpcode(SMSG_CAST_FAILED,                  "SMSG_CAST_FAILED",                 STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x131*/  StoreOpcode(SMSG_SPELL_START,                  "SMSG_SPELL_START",                 STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x132*/  StoreOpcode(SMSG_SPELL_GO,                     "SMSG_SPELL_GO",                    STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x133*/  StoreOpcode(SMSG_SPELL_FAILURE,                "SMSG_SPELL_FAILURE",               STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x134*/  StoreOpcode(SMSG_SPELL_COOLDOWN,               "SMSG_SPELL_COOLDOWN",              STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x135*/  StoreOpcode(SMSG_COOLDOWN_EVENT,               "SMSG_COOLDOWN_EVENT",              STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x136*/  StoreOpcode(CMSG_CANCEL_AURA,                  "CMSG_CANCEL_AURA",                 STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleCancelAuraOpcode);
    /*0x137*/  StoreOpcode(SMSG_UPDATE_AURA_DURATION,         "SMSG_UPDATE_AURA_DURATION",        STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x138*/  StoreOpcode(SMSG_PET_CAST_FAILED,              "SMSG_PET_CAST_FAILED",             STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x139*/  StoreOpcode(MSG_CHANNEL_START,                 "MSG_CHANNEL_START",                STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x13A*/  StoreOpcode(MSG_CHANNEL_UPDATE,                "MSG_CHANNEL_UPDATE",               STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*[-ZERO] Need check */ /*0x13B*/  StoreOpcode(CMSG_CANCEL_CHANNELLING,           "CMSG_CANCEL_CHANNELLING",          STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleCancelChanneling);
    /*0x13C*/  StoreOpcode(SMSG_AI_REACTION,                  "SMSG_AI_REACTION",                 STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x13D*/  StoreOpcode(CMSG_SET_SELECTION,                "CMSG_SET_SELECTION",               STATUS_LOGGEDIN,  PROCESS_INPLACE,      &WorldSession::HandleSetSelectionOpcode);
    /*[-ZERO] Need check */ /*0x13E*/  StoreOpcode(CMSG_SET_TARGET_OBSOLETE,          "CMSG_SET_TARGET_OBSOLETE",         STATUS_LOGGEDIN,  PROCESS_INPLACE,      &WorldSession::HandleSetTargetOpcode);
    /*0x13F*/  StoreOpcode(CMSG_UNUSED,                       "CMSG_UNUSED",                      STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x140*/  StoreOpcode(CMSG_UNUSED2,                      "CMSG_UNUSED2",                     STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*[-ZERO] Need check */ /*0x141*/  StoreOpcode(CMSG_ATTACKSWING,                  "CMSG_ATTACKSWING",                 STATUS_LOGGEDIN,  PROCESS_INPLACE,      &WorldSession::HandleAttackSwingOpcode);
    /*[-ZERO] Need check */ /*0x142*/  StoreOpcode(CMSG_ATTACKSTOP,                   "CMSG_ATTACKSTOP",                  STATUS_LOGGEDIN,  PROCESS_INPLACE,      &WorldSession::HandleAttackStopOpcode);
    /*0x143*/  StoreOpcode(SMSG_ATTACKSTART,                  "SMSG_ATTACKSTART",                 STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x144*/  StoreOpcode(SMSG_ATTACKSTOP,                   "SMSG_ATTACKSTOP",                  STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x145*/  StoreOpcode(SMSG_ATTACKSWING_NOTINRANGE,       "SMSG_ATTACKSWING_NOTINRANGE",      STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x146*/  StoreOpcode(SMSG_ATTACKSWING_BADFACING,        "SMSG_ATTACKSWING_BADFACING",       STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x147*/  StoreOpcode(SMSG_ATTACKSWING_NOTSTANDING,      "SMSG_ATTACKSWING_NOTSTANDING",     STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x148*/  StoreOpcode(SMSG_ATTACKSWING_DEADTARGET,       "SMSG_ATTACKSWING_DEADTARGET",      STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x149*/  StoreOpcode(SMSG_ATTACKSWING_CANT_ATTACK,      "SMSG_ATTACKSWING_CANT_ATTACK",     STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x14A*/  StoreOpcode(SMSG_ATTACKERSTATEUPDATE,          "SMSG_ATTACKERSTATEUPDATE",         STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x14B*/  StoreOpcode(SMSG_VICTIMSTATEUPDATE_OBSOLETE,   "SMSG_VICTIMSTATEUPDATE_OBSOLETE",  STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x14C*/  StoreOpcode(SMSG_DAMAGE_DONE_OBSOLETE,         "SMSG_DAMAGE_DONE_OBSOLETE",        STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x14D*/  StoreOpcode(SMSG_DAMAGE_TAKEN_OBSOLETE,        "SMSG_DAMAGE_TAKEN_OBSOLETE",       STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x14E*/  StoreOpcode(SMSG_CANCEL_COMBAT,                "SMSG_CANCEL_COMBAT",               STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x14F*/  StoreOpcode(SMSG_PLAYER_COMBAT_XP_GAIN_OBSOLETE,    "SMSG_PLAYER_COMBAT_XP_GAIN_OBSOLETE",     STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x150*/  StoreOpcode(SMSG_SPELLHEALLOG,                 "SMSG_SPELLHEALLOG",                STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x151*/  StoreOpcode(SMSG_SPELLENERGIZELOG,             "SMSG_SPELLENERGIZELOG",            STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x152*/  StoreOpcode(CMSG_SHEATHE_OBSOLETE,             "CMSG_SHEATHE_OBSOLETE",            STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x153*/  StoreOpcode(CMSG_SAVE_PLAYER,                  "CMSG_SAVE_PLAYER",                 STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x154*/  StoreOpcode(CMSG_SETDEATHBINDPOINT,            "CMSG_SETDEATHBINDPOINT",           STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x155*/  StoreOpcode(SMSG_BINDPOINTUPDATE,              "SMSG_BINDPOINTUPDATE",             STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x156*/  StoreOpcode(CMSG_GETDEATHBINDZONE,             "CMSG_GETDEATHBINDZONE",            STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*[ZERO] Structure */ /*0x157*/  StoreOpcode(SMSG_BINDZONEREPLY,                "SMSG_BINDZONEREPLY",               STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x158*/  StoreOpcode(SMSG_PLAYERBOUND,                  "SMSG_PLAYERBOUND",                 STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x159*/  StoreOpcode(SMSG_CLIENT_CONTROL_UPDATE,        "SMSG_CLIENT_CONTROL_UPDATE",       STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x15A*/  StoreOpcode(CMSG_REPOP_REQUEST,                "CMSG_REPOP_REQUEST",               STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleRepopRequestOpcode);
    /*0x15B*/  StoreOpcode(SMSG_RESURRECT_REQUEST,            "SMSG_RESURRECT_REQUEST",           STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x15C*/  StoreOpcode(CMSG_RESURRECT_RESPONSE,           "CMSG_RESURRECT_RESPONSE",          STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleResurrectResponseOpcode);
    /*[-ZERO] Need check */ /*0x15D*/  StoreOpcode(CMSG_LOOT,                         "CMSG_LOOT",                        STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleLootOpcode);
    /*[-ZERO] Need check */ /*0x15E*/  StoreOpcode(CMSG_LOOT_MONEY,                   "CMSG_LOOT_MONEY",                  STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleLootMoneyOpcode);
    /*[-ZERO] Need check */ /*0x15F*/  StoreOpcode(CMSG_LOOT_RELEASE,                 "CMSG_LOOT_RELEASE",                STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleLootReleaseOpcode);
    /*0x160*/  StoreOpcode(SMSG_LOOT_RESPONSE,                "SMSG_LOOT_RESPONSE",               STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x161*/  StoreOpcode(SMSG_LOOT_RELEASE_RESPONSE,        "SMSG_LOOT_RELEASE_RESPONSE",       STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x162*/  StoreOpcode(SMSG_LOOT_REMOVED,                 "SMSG_LOOT_REMOVED",                STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x163*/  StoreOpcode(SMSG_LOOT_MONEY_NOTIFY,            "SMSG_LOOT_MONEY_NOTIFY",           STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x164*/  StoreOpcode(SMSG_LOOT_ITEM_NOTIFY,             "SMSG_LOOT_ITEM_NOTIFY",            STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x165*/  StoreOpcode(SMSG_LOOT_CLEAR_MONEY,             "SMSG_LOOT_CLEAR_MONEY",            STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x166*/  StoreOpcode(SMSG_ITEM_PUSH_RESULT,             "SMSG_ITEM_PUSH_RESULT",            STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x167*/  StoreOpcode(SMSG_DUEL_REQUESTED,               "SMSG_DUEL_REQUESTED",              STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x168*/  StoreOpcode(SMSG_DUEL_OUTOFBOUNDS,             "SMSG_DUEL_OUTOFBOUNDS",            STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x169*/  StoreOpcode(SMSG_DUEL_INBOUNDS,                "SMSG_DUEL_INBOUNDS",               STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x16A*/  StoreOpcode(SMSG_DUEL_COMPLETE,                "SMSG_DUEL_COMPLETE",               STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x16B*/  StoreOpcode(SMSG_DUEL_WINNER,                  "SMSG_DUEL_WINNER",                 STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x16C*/  StoreOpcode(CMSG_DUEL_ACCEPTED,                "CMSG_DUEL_ACCEPTED",               STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleDuelAcceptedOpcode);
    /*[-ZERO] Need check */ /*0x16D*/  StoreOpcode(CMSG_DUEL_CANCELLED,               "CMSG_DUEL_CANCELLED",              STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleDuelCancelledOpcode);
    /*0x16E*/  StoreOpcode(SMSG_MOUNTRESULT,                  "SMSG_MOUNTRESULT",                 STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x16F*/  StoreOpcode(SMSG_DISMOUNTRESULT,               "SMSG_DISMOUNTRESULT",              STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x170*/  StoreOpcode(SMSG_PUREMOUNT_CANCELLED_OBSOLETE, "SMSG_PUREMOUNT_CANCELLED_OBSOLETE", STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x171*/  StoreOpcode(CMSG_MOUNTSPECIAL_ANIM,            "CMSG_MOUNTSPECIAL_ANIM",           STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleMountSpecialAnimOpcode);
    /*0x172*/  StoreOpcode(SMSG_MOUNTSPECIAL_ANIM,            "SMSG_MOUNTSPECIAL_ANIM",           STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x173*/  StoreOpcode(SMSG_PET_TAME_FAILURE,             "SMSG_PET_TAME_FAILURE",            STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x174*/  StoreOpcode(CMSG_PET_SET_ACTION,               "CMSG_PET_SET_ACTION",              STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandlePetSetAction);
    /*[-ZERO] Need check */ /*0x175*/  StoreOpcode(CMSG_PET_ACTION,                   "CMSG_PET_ACTION",                  STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandlePetAction);
    /*[-ZERO] Need check */ /*0x176*/  StoreOpcode(CMSG_PET_ABANDON,                  "CMSG_PET_ABANDON",                 STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandlePetAbandon);
    /*[-ZERO] Need check */ /*0x177*/  StoreOpcode(CMSG_PET_RENAME,                   "CMSG_PET_RENAME",                  STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandlePetRename);
    /*0x178*/  StoreOpcode(SMSG_PET_NAME_INVALID,             "SMSG_PET_NAME_INVALID",            STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x179*/  StoreOpcode(SMSG_PET_SPELLS,                   "SMSG_PET_SPELLS",                  STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x17A*/  StoreOpcode(SMSG_PET_MODE,                     "SMSG_PET_MODE",                    STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x17B*/  StoreOpcode(CMSG_GOSSIP_HELLO,                 "CMSG_GOSSIP_HELLO",                STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleGossipHelloOpcode);
    /*[-ZERO] Need check */ /*0x17C*/  StoreOpcode(CMSG_GOSSIP_SELECT_OPTION,         "CMSG_GOSSIP_SELECT_OPTION",        STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleGossipSelectOptionOpcode);
    /*0x17D*/  StoreOpcode(SMSG_GOSSIP_MESSAGE,               "SMSG_GOSSIP_MESSAGE",              STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x17E*/  StoreOpcode(SMSG_GOSSIP_COMPLETE,              "SMSG_GOSSIP_COMPLETE",             STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x17F*/  StoreOpcode(CMSG_NPC_TEXT_QUERY,               "CMSG_NPC_TEXT_QUERY",              STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleNpcTextQueryOpcode);
    /*0x180*/  StoreOpcode(SMSG_NPC_TEXT_UPDATE,              "SMSG_NPC_TEXT_UPDATE",             STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x181*/  StoreOpcode(SMSG_NPC_WONT_TALK,                "SMSG_NPC_WONT_TALK",               STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x182*/  StoreOpcode(CMSG_QUESTGIVER_STATUS_QUERY,      "CMSG_QUESTGIVER_STATUS_QUERY",     STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleQuestgiverStatusQueryOpcode);
    /*0x183*/  StoreOpcode(SMSG_QUESTGIVER_STATUS,            "SMSG_QUESTGIVER_STATUS",           STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x184*/  StoreOpcode(CMSG_QUESTGIVER_HELLO,             "CMSG_QUESTGIVER_HELLO",            STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleQuestgiverHelloOpcode);
    /*0x185*/  StoreOpcode(SMSG_QUESTGIVER_QUEST_LIST,        "SMSG_QUESTGIVER_QUEST_LIST",       STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x186*/  StoreOpcode(CMSG_QUESTGIVER_QUERY_QUEST,       "CMSG_QUESTGIVER_QUERY_QUEST",      STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleQuestgiverQueryQuestOpcode);
    /*[-ZERO] Need check */ /*0x187*/  StoreOpcode(CMSG_QUESTGIVER_QUEST_AUTOLAUNCH,  "CMSG_QUESTGIVER_QUEST_AUTOLAUNCH", STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleQuestgiverQuestAutoLaunch);
    /*0x188*/  StoreOpcode(SMSG_QUESTGIVER_QUEST_DETAILS,     "SMSG_QUESTGIVER_QUEST_DETAILS",    STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x189*/  StoreOpcode(CMSG_QUESTGIVER_ACCEPT_QUEST,      "CMSG_QUESTGIVER_ACCEPT_QUEST",     STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleQuestgiverAcceptQuestOpcode);
    /*[-ZERO] Need check */ /*0x18A*/  StoreOpcode(CMSG_QUESTGIVER_COMPLETE_QUEST,    "CMSG_QUESTGIVER_COMPLETE_QUEST",   STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleQuestgiverCompleteQuest);
    /*0x18B*/  StoreOpcode(SMSG_QUESTGIVER_REQUEST_ITEMS,     "SMSG_QUESTGIVER_REQUEST_ITEMS",    STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x18C*/  StoreOpcode(CMSG_QUESTGIVER_REQUEST_REWARD,    "CMSG_QUESTGIVER_REQUEST_REWARD",   STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleQuestgiverRequestRewardOpcode);
    /*0x18D*/  StoreOpcode(SMSG_QUESTGIVER_OFFER_REWARD,      "SMSG_QUESTGIVER_OFFER_REWARD",     STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x18E*/  StoreOpcode(CMSG_QUESTGIVER_CHOOSE_REWARD,     "CMSG_QUESTGIVER_CHOOSE_REWARD",    STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleQuestgiverChooseRewardOpcode);
    /*0x18F*/  StoreOpcode(SMSG_QUESTGIVER_QUEST_INVALID,     "SMSG_QUESTGIVER_QUEST_INVALID",    STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x190*/  StoreOpcode(CMSG_QUESTGIVER_CANCEL,            "CMSG_QUESTGIVER_CANCEL",           STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleQuestgiverCancel);
    /*0x191*/  StoreOpcode(SMSG_QUESTGIVER_QUEST_COMPLETE,    "SMSG_QUESTGIVER_QUEST_COMPLETE",   STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x192*/  StoreOpcode(SMSG_QUESTGIVER_QUEST_FAILED,      "SMSG_QUESTGIVER_QUEST_FAILED",     STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x193*/  StoreOpcode(CMSG_QUESTLOG_SWAP_QUEST,          "CMSG_QUESTLOG_SWAP_QUEST",         STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleQuestLogSwapQuest);
    /*[-ZERO] Need check */ /*0x194*/  StoreOpcode(CMSG_QUESTLOG_REMOVE_QUEST,        "CMSG_QUESTLOG_REMOVE_QUEST",       STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleQuestLogRemoveQuest);
    /*0x195*/  StoreOpcode(SMSG_QUESTLOG_FULL,                "SMSG_QUESTLOG_FULL",               STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x196*/  StoreOpcode(SMSG_QUESTUPDATE_FAILED,           "SMSG_QUESTUPDATE_FAILED",          STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x197*/  StoreOpcode(SMSG_QUESTUPDATE_FAILEDTIMER,      "SMSG_QUESTUPDATE_FAILEDTIMER",     STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x198*/  StoreOpcode(SMSG_QUESTUPDATE_COMPLETE,         "SMSG_QUESTUPDATE_COMPLETE",        STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x199*/  StoreOpcode(SMSG_QUESTUPDATE_ADD_KILL,         "SMSG_QUESTUPDATE_ADD_KILL",        STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x19A*/  StoreOpcode(SMSG_QUESTUPDATE_ADD_ITEM,         "SMSG_QUESTUPDATE_ADD_ITEM",        STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x19B*/  StoreOpcode(CMSG_QUEST_CONFIRM_ACCEPT,         "CMSG_QUEST_CONFIRM_ACCEPT",        STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleQuestConfirmAccept);
    /*0x19C*/  StoreOpcode(SMSG_QUEST_CONFIRM_ACCEPT,         "SMSG_QUEST_CONFIRM_ACCEPT",        STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x19D*/  StoreOpcode(CMSG_PUSHQUESTTOPARTY,             "CMSG_PUSHQUESTTOPARTY",            STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandlePushQuestToParty);
    /*[-ZERO] Need check */ /*0x19E*/  StoreOpcode(CMSG_LIST_INVENTORY,               "CMSG_LIST_INVENTORY",              STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleListInventoryOpcode);
    /*0x19F*/  StoreOpcode(SMSG_LIST_INVENTORY,               "SMSG_LIST_INVENTORY",              STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x1A0*/  StoreOpcode(CMSG_SELL_ITEM,                    "CMSG_SELL_ITEM",                   STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleSellItemOpcode);
    /*0x1A1*/  StoreOpcode(SMSG_SELL_ITEM,                    "SMSG_SELL_ITEM",                   STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x1A2*/  StoreOpcode(CMSG_BUY_ITEM,                     "CMSG_BUY_ITEM",                    STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleBuyItemOpcode);
    /*[-ZERO] Need check */ /*0x1A3*/  StoreOpcode(CMSG_BUY_ITEM_IN_SLOT,             "CMSG_BUY_ITEM_IN_SLOT",            STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleBuyItemInSlotOpcode);
    /*0x1A4*/  StoreOpcode(SMSG_BUY_ITEM,                     "SMSG_BUY_ITEM",                    STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x1A5*/  StoreOpcode(SMSG_BUY_FAILED,                   "SMSG_BUY_FAILED",                  STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x1A6*/  StoreOpcode(CMSG_TAXICLEARALLNODES,            "CMSG_TAXICLEARALLNODES",           STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x1A7*/  StoreOpcode(CMSG_TAXIENABLEALLNODES,           "CMSG_TAXIENABLEALLNODES",          STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x1A8*/  StoreOpcode(CMSG_TAXISHOWNODES,                "CMSG_TAXISHOWNODES",               STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x1A9*/  StoreOpcode(SMSG_SHOWTAXINODES,                "SMSG_SHOWTAXINODES",               STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x1AA*/  StoreOpcode(CMSG_TAXINODE_STATUS_QUERY,        "CMSG_TAXINODE_STATUS_QUERY",       STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleTaxiNodeStatusQueryOpcode);
    /*0x1AB*/  StoreOpcode(SMSG_TAXINODE_STATUS,              "SMSG_TAXINODE_STATUS",             STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x1AC*/  StoreOpcode(CMSG_TAXIQUERYAVAILABLENODES,      "CMSG_TAXIQUERYAVAILABLENODES",     STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleTaxiQueryAvailableNodes);
    /*[-ZERO] Need check */ /*0x1AD*/  StoreOpcode(CMSG_ACTIVATETAXI,                 "CMSG_ACTIVATETAXI",                STATUS_LOGGEDIN,  PROCESS_INPLACE,      &WorldSession::HandleActivateTaxiOpcode);
    /*0x1AE*/  StoreOpcode(SMSG_ACTIVATETAXIREPLY,            "SMSG_ACTIVATETAXIREPLY",           STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x1AF*/  StoreOpcode(SMSG_NEW_TAXI_PATH,                "SMSG_NEW_TAXI_PATH",               STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x1B0*/  StoreOpcode(CMSG_TRAINER_LIST,                 "CMSG_TRAINER_LIST",                STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleTrainerListOpcode);
    /*0x1B1*/  StoreOpcode(SMSG_TRAINER_LIST,                 "SMSG_TRAINER_LIST",                STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x1B2*/  StoreOpcode(CMSG_TRAINER_BUY_SPELL,            "CMSG_TRAINER_BUY_SPELL",           STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleTrainerBuySpellOpcode);
    /*[-ZERO] No effect */ /*0x1B3*/  StoreOpcode(SMSG_TRAINER_BUY_SUCCEEDED,        "SMSG_TRAINER_BUY_SUCCEEDED",       STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x1B4*/  StoreOpcode(SMSG_TRAINER_BUY_FAILED,           "SMSG_TRAINER_BUY_FAILED",          STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x1B5*/  StoreOpcode(CMSG_BINDER_ACTIVATE,              "CMSG_BINDER_ACTIVATE",             STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleBinderActivateOpcode);
    /*0x1B6*/  StoreOpcode(SMSG_PLAYERBINDERROR,              "SMSG_PLAYERBINDERROR",             STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x1B7*/  StoreOpcode(CMSG_BANKER_ACTIVATE,              "CMSG_BANKER_ACTIVATE",             STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleBankerActivateOpcode);
    /*0x1B8*/  StoreOpcode(SMSG_SHOW_BANK,                    "SMSG_SHOW_BANK",                   STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x1B9*/  StoreOpcode(CMSG_BUY_BANK_SLOT,                "CMSG_BUY_BANK_SLOT",               STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleBuyBankSlotOpcode);
    /*0x1BA*/  StoreOpcode(SMSG_BUY_BANK_SLOT_RESULT,         "SMSG_BUY_BANK_SLOT_RESULT",        STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x1BB*/  StoreOpcode(CMSG_PETITION_SHOWLIST,            "CMSG_PETITION_SHOWLIST",           STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandlePetitionShowListOpcode);
    /*0x1BC*/  StoreOpcode(SMSG_PETITION_SHOWLIST,            "SMSG_PETITION_SHOWLIST",           STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x1BD*/  StoreOpcode(CMSG_PETITION_BUY,                 "CMSG_PETITION_BUY",                STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandlePetitionBuyOpcode);
    /*[-ZERO] Need check */ /*0x1BE*/  StoreOpcode(CMSG_PETITION_SHOW_SIGNATURES,     "CMSG_PETITION_SHOW_SIGNATURES",    STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandlePetitionShowSignOpcode);
    /*0x1BF*/  StoreOpcode(SMSG_PETITION_SHOW_SIGNATURES,     "SMSG_PETITION_SHOW_SIGNATURES",    STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x1C0*/  StoreOpcode(CMSG_PETITION_SIGN,                "CMSG_PETITION_SIGN",               STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandlePetitionSignOpcode);
    /*0x1C1*/  StoreOpcode(SMSG_PETITION_SIGN_RESULTS,        "SMSG_PETITION_SIGN_RESULTS",       STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x1C2*/  StoreOpcode(MSG_PETITION_DECLINE,              "MSG_PETITION_DECLINE",             STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandlePetitionDeclineOpcode);
    /*[-ZERO] Need check */ /*0x1C3*/  StoreOpcode(CMSG_OFFER_PETITION,               "CMSG_OFFER_PETITION",              STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleOfferPetitionOpcode);
    /*[-ZERO] Need check */ /*0x1C4*/  StoreOpcode(CMSG_TURN_IN_PETITION,             "CMSG_TURN_IN_PETITION",            STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleTurnInPetitionOpcode);
    /*0x1C5*/  StoreOpcode(SMSG_TURN_IN_PETITION_RESULTS,     "SMSG_TURN_IN_PETITION_RESULTS",    STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x1C6*/  StoreOpcode(CMSG_PETITION_QUERY,               "CMSG_PETITION_QUERY",              STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandlePetitionQueryOpcode);
    /*0x1C7*/  StoreOpcode(SMSG_PETITION_QUERY_RESPONSE,      "SMSG_PETITION_QUERY_RESPONSE",     STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x1C8*/  StoreOpcode(SMSG_FISH_NOT_HOOKED,              "SMSG_FISH_NOT_HOOKED",             STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x1C9*/  StoreOpcode(SMSG_FISH_ESCAPED,                 "SMSG_FISH_ESCAPED",                STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x1CA*/  StoreOpcode(CMSG_BUG,                          "CMSG_BUG",                         STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleBugOpcode);
    /*0x1CB*/  StoreOpcode(SMSG_NOTIFICATION,                 "SMSG_NOTIFICATION",                STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x1CC*/  StoreOpcode(CMSG_PLAYED_TIME,                  "CMSG_PLAYED_TIME",                 STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandlePlayedTime);
    /*0x1CD*/  StoreOpcode(SMSG_PLAYED_TIME,                  "SMSG_PLAYED_TIME",                 STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x1CE*/  StoreOpcode(CMSG_QUERY_TIME,                   "CMSG_QUERY_TIME",                  STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleQueryTimeOpcode);
    /*0x1CF*/  StoreOpcode(SMSG_QUERY_TIME_RESPONSE,          "SMSG_QUERY_TIME_RESPONSE",         STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x1D0*/  StoreOpcode(SMSG_LOG_XPGAIN,                   "SMSG_LOG_XPGAIN",                  STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x1D1*/  StoreOpcode(SMSG_AURACASTLOG,                  "SMSG_AURACASTLOG",                 STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x1D2*/  StoreOpcode(CMSG_RECLAIM_CORPSE,               "CMSG_RECLAIM_CORPSE",              STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleReclaimCorpseOpcode);
    /*[-ZERO] Need check */ /*0x1D3*/  StoreOpcode(CMSG_WRAP_ITEM,                    "CMSG_WRAP_ITEM",                   STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleWrapItemOpcode);
    /*0x1D4*/  StoreOpcode(SMSG_LEVELUP_INFO,                 "SMSG_LEVELUP_INFO",                STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x1D5*/  StoreOpcode(MSG_MINIMAP_PING,                  "MSG_MINIMAP_PING",                 STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleMinimapPingOpcode);
    /*0x1D6*/  StoreOpcode(SMSG_RESISTLOG,                    "SMSG_RESISTLOG",                   STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x1D7*/  StoreOpcode(SMSG_ENCHANTMENTLOG,               "SMSG_ENCHANTMENTLOG",              STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x1D8*/  StoreOpcode(CMSG_SET_SKILL_CHEAT,              "CMSG_SET_SKILL_CHEAT",             STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x1D9*/  StoreOpcode(SMSG_START_MIRROR_TIMER,           "SMSG_START_MIRROR_TIMER",          STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x1DA*/  StoreOpcode(SMSG_PAUSE_MIRROR_TIMER,           "SMSG_PAUSE_MIRROR_TIMER",          STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x1DB*/  StoreOpcode(SMSG_STOP_MIRROR_TIMER,            "SMSG_STOP_MIRROR_TIMER",           STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x1DC*/  StoreOpcode(CMSG_PING,                         "CMSG_PING",                        STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_EarlyProccess);
    /*[ZERO] To check */ /*0x1DD*/  StoreOpcode(SMSG_PONG,                         "SMSG_PONG",                        STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x1DE*/  StoreOpcode(SMSG_CLEAR_COOLDOWN,               "SMSG_CLEAR_COOLDOWN",              STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x1DF*/  StoreOpcode(SMSG_GAMEOBJECT_PAGETEXT,          "SMSG_GAMEOBJECT_PAGETEXT",         STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x1E0*/  StoreOpcode(CMSG_SETSHEATHED,                  "CMSG_SETSHEATHED",                 STATUS_LOGGEDIN,  PROCESS_INPLACE,      &WorldSession::HandleSetSheathedOpcode);
    /*0x1E1*/  StoreOpcode(SMSG_COOLDOWN_CHEAT,               "SMSG_COOLDOWN_CHEAT",              STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x1E2*/  StoreOpcode(SMSG_SPELL_DELAYED,                "SMSG_SPELL_DELAYED",               STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x1E3*/  StoreOpcode(CMSG_PLAYER_MACRO_OBSOLETE,        "CMSG_PLAYER_MACRO_OBSOLETE",       STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*[-ZERO] Need check */ /*0x1E4*/  StoreOpcode(SMSG_PLAYER_MACRO_OBSOLETE,        "SMSG_PLAYER_MACRO_OBSOLETE",       STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x1E5*/  StoreOpcode(CMSG_GHOST,                        "CMSG_GHOST",                       STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x1E6*/  StoreOpcode(CMSG_GM_INVIS,                     "CMSG_GM_INVIS",                    STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*[-ZERO] Need check */ /*0x1E7*/  StoreOpcode(SMSG_INVALID_PROMOTION_CODE,       "SMSG_INVALID_PROMOTION_CODE",      STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x1E8*/  StoreOpcode(MSG_GM_BIND_OTHER,                 "MSG_GM_BIND_OTHER",                STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*[-ZERO] Need check */ /*0x1E9*/  StoreOpcode(MSG_GM_SUMMON,                     "MSG_GM_SUMMON",                    STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x1EA*/  StoreOpcode(SMSG_ITEM_TIME_UPDATE,             "SMSG_ITEM_TIME_UPDATE",            STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x1EB*/  StoreOpcode(SMSG_ITEM_ENCHANT_TIME_UPDATE,     "SMSG_ITEM_ENCHANT_TIME_UPDATE",    STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x1EC*/  StoreOpcode(SMSG_AUTH_CHALLENGE,               "SMSG_AUTH_CHALLENGE",              STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x1ED*/  StoreOpcode(CMSG_AUTH_SESSION,                 "CMSG_AUTH_SESSION",                STATUS_NEVER,     PROCESS_THREADSAFE,   &WorldSession::Handle_EarlyProccess);
    StoreOpcode(SMSG_AUTH_RESPONSE,                "SMSG_AUTH_RESPONSE",               STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x1EF*/  StoreOpcode(MSG_GM_SHOWLABEL,                  "MSG_GM_SHOWLABEL",                 STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*[-ZERO] Need check */ /*0x1F0*/  StoreOpcode(CMSG_PET_CAST_SPELL,               "CMSG_PET_CAST_SPELL",              STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandlePetCastSpellOpcode);
    /*0x1F1*/  StoreOpcode(MSG_SAVE_GUILD_EMBLEM,             "MSG_SAVE_GUILD_EMBLEM",            STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleSaveGuildEmblemOpcode);
    /*0x1F2*/  StoreOpcode(MSG_TABARDVENDOR_ACTIVATE,         "MSG_TABARDVENDOR_ACTIVATE",        STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleTabardVendorActivateOpcode);
    /*0x1F3*/  StoreOpcode(SMSG_PLAY_SPELL_VISUAL,            "SMSG_PLAY_SPELL_VISUAL",           STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x1F4*/  StoreOpcode(CMSG_ZONEUPDATE,                   "CMSG_ZONEUPDATE",                  STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleZoneUpdateOpcode);
    /*0x1F5*/  StoreOpcode(SMSG_PARTYKILLLOG,                 "SMSG_PARTYKILLLOG",                STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x1F6*/  StoreOpcode(SMSG_COMPRESSED_UPDATE_OBJECT,     "SMSG_COMPRESSED_UPDATE_OBJECT",    STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x1F7*/  StoreOpcode(SMSG_PLAY_SPELL_IMPACT,            "SMSG_PLAY_SPELL_IMPACT",           STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x1F8*/  StoreOpcode(SMSG_EXPLORATION_EXPERIENCE,       "SMSG_EXPLORATION_EXPERIENCE",      STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x1F9*/  StoreOpcode(CMSG_GM_SET_SECURITY_GROUP,        "CMSG_GM_SET_SECURITY_GROUP",       STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x1FA*/  StoreOpcode(CMSG_GM_NUKE,                      "CMSG_GM_NUKE",                     STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x1FB*/  StoreOpcode(MSG_RANDOM_ROLL,                   "MSG_RANDOM_ROLL",                  STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleRandomRollOpcode);
    /*0x1FC*/  StoreOpcode(SMSG_ENVIRONMENTALDAMAGELOG,       "SMSG_ENVIRONMENTALDAMAGELOG",      STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x1FD*/  StoreOpcode(CMSG_RWHOIS_OBSOLETE,              "CMSG_RWHOIS_OBSOLETE",             STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x1FE*/  StoreOpcode(SMSG_RWHOIS,                       "SMSG_RWHOIS",                      STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x201*/  StoreOpcode(CMSG_UNLEARN_SPELL,                "CMSG_UNLEARN_SPELL",               STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*[-ZERO] Need check */ /*0x202*/  StoreOpcode(CMSG_UNLEARN_SKILL,                "CMSG_UNLEARN_SKILL",               STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleUnlearnSkillOpcode);
    /*0x203*/  StoreOpcode(SMSG_REMOVED_SPELL,                "SMSG_REMOVED_SPELL",               STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x204*/  StoreOpcode(CMSG_DECHARGE,                     "CMSG_DECHARGE",                    STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*[-ZERO] Need check */ /*0x205*/  StoreOpcode(CMSG_GMTICKET_CREATE,              "CMSG_GMTICKET_CREATE",             STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleGMTicketCreateOpcode);
    /*0x206*/  StoreOpcode(SMSG_GMTICKET_CREATE,              "SMSG_GMTICKET_CREATE",             STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x207*/  StoreOpcode(CMSG_GMTICKET_UPDATETEXT,          "CMSG_GMTICKET_UPDATETEXT",         STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleGMTicketUpdateTextOpcode);
    /*0x208*/  StoreOpcode(SMSG_GMTICKET_UPDATETEXT,          "SMSG_GMTICKET_UPDATETEXT",         STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x209*/  StoreOpcode(SMSG_ACCOUNT_DATA_TIMES,           "SMSG_ACCOUNT_DATA_TIMES",          STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x20A*/  StoreOpcode(CMSG_REQUEST_ACCOUNT_DATA,         "CMSG_REQUEST_ACCOUNT_DATA",        STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleRequestAccountData);
    /*[-ZERO] Need check */ /*0x20B*/  StoreOpcode(CMSG_UPDATE_ACCOUNT_DATA,          "CMSG_UPDATE_ACCOUNT_DATA",         STATUS_LOGGEDIN_OR_RECENTLY_LOGGEDOUT, PROCESS_THREADUNSAFE, &WorldSession::HandleUpdateAccountData);
    /*0x20C*/  StoreOpcode(SMSG_UPDATE_ACCOUNT_DATA,          "SMSG_UPDATE_ACCOUNT_DATA",         STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x20D*/  StoreOpcode(SMSG_CLEAR_FAR_SIGHT_IMMEDIATE,    "SMSG_CLEAR_FAR_SIGHT_IMMEDIATE",   STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x20E*/  StoreOpcode(SMSG_POWERGAINLOG_OBSOLETE,        "SMSG_POWERGAINLOG_OBSOLETE",       STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x20F*/  StoreOpcode(CMSG_GM_TEACH,                     "CMSG_GM_TEACH",                    STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x210*/  StoreOpcode(CMSG_GM_CREATE_ITEM_TARGET,        "CMSG_GM_CREATE_ITEM_TARGET",       STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x211*/  StoreOpcode(CMSG_GMTICKET_GETTICKET,           "CMSG_GMTICKET_GETTICKET",          STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleGMTicketGetTicketOpcode);
    /*0x212*/  StoreOpcode(SMSG_GMTICKET_GETTICKET,           "SMSG_GMTICKET_GETTICKET",          STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x213*/  StoreOpcode(CMSG_UNLEARN_TALENTS,              "CMSG_UNLEARN_TALENTS",             STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*[-ZERO] Need check */ /*0x214*/  StoreOpcode(SMSG_GAMEOBJECT_SPAWN_ANIM_OBSOLETE,    "SMSG_GAMEOBJECT_SPAWN_ANIM_OBSOLETE",     STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x215*/  StoreOpcode(SMSG_GAMEOBJECT_DESPAWN_ANIM,      "SMSG_GAMEOBJECT_DESPAWN_ANIM",     STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x216*/  StoreOpcode(MSG_CORPSE_QUERY,                  "MSG_CORPSE_QUERY",                 STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleCorpseQueryOpcode);
    /*[-ZERO] Need check */ /*0x217*/  StoreOpcode(CMSG_GMTICKET_DELETETICKET,        "CMSG_GMTICKET_DELETETICKET",       STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleGMTicketDeleteTicketOpcode);
    /*0x218*/  StoreOpcode(SMSG_GMTICKET_DELETETICKET,        "SMSG_GMTICKET_DELETETICKET",       STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x219*/  StoreOpcode(SMSG_CHAT_WRONG_FACTION,           "SMSG_CHAT_WRONG_FACTION",          STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x21A*/  StoreOpcode(CMSG_GMTICKET_SYSTEMSTATUS,        "CMSG_GMTICKET_SYSTEMSTATUS",       STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleGMTicketSystemStatusOpcode);
    /*0x21B*/  StoreOpcode(SMSG_GMTICKET_SYSTEMSTATUS,        "SMSG_GMTICKET_SYSTEMSTATUS",       STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x21C*/  StoreOpcode(CMSG_SPIRIT_HEALER_ACTIVATE,       "CMSG_SPIRIT_HEALER_ACTIVATE",      STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleSpiritHealerActivateOpcode);
    /*0x21D*/  StoreOpcode(CMSG_SET_STAT_CHEAT,               "CMSG_SET_STAT_CHEAT",              STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x21E*/  StoreOpcode(SMSG_SET_REST_START,               "SMSG_SET_REST_START",              STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x21F*/  StoreOpcode(CMSG_SKILL_BUY_STEP,               "CMSG_SKILL_BUY_STEP",              STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x220*/  StoreOpcode(CMSG_SKILL_BUY_RANK,               "CMSG_SKILL_BUY_RANK",              STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x221*/  StoreOpcode(CMSG_XP_CHEAT,                     "CMSG_XP_CHEAT",                    STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x222*/  StoreOpcode(SMSG_SPIRIT_HEALER_CONFIRM,        "SMSG_SPIRIT_HEALER_CONFIRM",       STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x223*/  StoreOpcode(CMSG_CHARACTER_POINT_CHEAT,        "CMSG_CHARACTER_POINT_CHEAT",       STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x224*/  StoreOpcode(SMSG_GOSSIP_POI,                   "SMSG_GOSSIP_POI",                  STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x225*/  StoreOpcode(CMSG_CHAT_IGNORED,                 "CMSG_CHAT_IGNORED",                STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleChatIgnoredOpcode);
    /*0x226*/  StoreOpcode(CMSG_GM_VISION,                    "CMSG_GM_VISION",                   STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x227*/  StoreOpcode(CMSG_SERVER_COMMAND,               "CMSG_SERVER_COMMAND",              STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x228*/  StoreOpcode(CMSG_GM_SILENCE,                   "CMSG_GM_SILENCE",                  STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x229*/  StoreOpcode(CMSG_GM_REVEALTO,                  "CMSG_GM_REVEALTO",                 STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x22A*/  StoreOpcode(CMSG_GM_RESURRECT,                 "CMSG_GM_RESURRECT",                STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x22B*/  StoreOpcode(CMSG_GM_SUMMONMOB,                 "CMSG_GM_SUMMONMOB",                STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x22C*/  StoreOpcode(CMSG_GM_MOVECORPSE,                "CMSG_GM_MOVECORPSE",               STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x22D*/  StoreOpcode(CMSG_GM_FREEZE,                    "CMSG_GM_FREEZE",                   STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x22E*/  StoreOpcode(CMSG_GM_UBERINVIS,                 "CMSG_GM_UBERINVIS",                STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x22F*/  StoreOpcode(CMSG_GM_REQUEST_PLAYER_INFO,       "CMSG_GM_REQUEST_PLAYER_INFO",      STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*[-ZERO] Need check */ /*0x230*/  StoreOpcode(SMSG_GM_PLAYER_INFO,               "SMSG_GM_PLAYER_INFO",              STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*[-ZERO] Need check */ /*0x231*/  StoreOpcode(CMSG_GUILD_RANK,                   "CMSG_GUILD_RANK",                  STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleGuildRankOpcode);
    /*[-ZERO] Need check */ /*0x232*/  StoreOpcode(CMSG_GUILD_ADD_RANK,               "CMSG_GUILD_ADD_RANK",              STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleGuildAddRankOpcode);
    /*[-ZERO] Need check */ /*0x233*/  StoreOpcode(CMSG_GUILD_DEL_RANK,               "CMSG_GUILD_DEL_RANK",              STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleGuildDelRankOpcode);
    /*[-ZERO] Need check */ /*0x234*/  StoreOpcode(CMSG_GUILD_SET_PUBLIC_NOTE,        "CMSG_GUILD_SET_PUBLIC_NOTE",       STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleGuildSetPublicNoteOpcode);
    /*[-ZERO] Need check */ /*0x235*/  StoreOpcode(CMSG_GUILD_SET_OFFICER_NOTE,       "CMSG_GUILD_SET_OFFICER_NOTE",      STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleGuildSetOfficerNoteOpcode);
    /*0x236*/  StoreOpcode(SMSG_LOGIN_VERIFY_WORLD,           "SMSG_LOGIN_VERIFY_WORLD",          STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x237*/  StoreOpcode(CMSG_CLEAR_EXPLORATION,            "CMSG_CLEAR_EXPLORATION",           STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*[-ZERO] Need check */ /*0x238*/  StoreOpcode(CMSG_SEND_MAIL,                    "CMSG_SEND_MAIL",                   STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleSendMail);
    /*0x239*/  StoreOpcode(SMSG_SEND_MAIL_RESULT,             "SMSG_SEND_MAIL_RESULT",            STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x23A*/  StoreOpcode(CMSG_GET_MAIL_LIST,                "CMSG_GET_MAIL_LIST",               STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleGetMailList);
    /*0x23B*/  StoreOpcode(SMSG_MAIL_LIST_RESULT,             "SMSG_MAIL_LIST_RESULT",            STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x23C*/  StoreOpcode(CMSG_BATTLEFIELD_LIST,             "CMSG_BATTLEFIELD_LIST",            STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleBattlefieldListOpcode);
    /*0x23D*/  StoreOpcode(SMSG_BATTLEFIELD_LIST,             "SMSG_BATTLEFIELD_LIST",            STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x23E*/  StoreOpcode(CMSG_BATTLEFIELD_JOIN,             "CMSG_BATTLEFIELD_JOIN",            STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x23F*/  StoreOpcode(SMSG_BATTLEFIELD_WIN_OBSOLETE,     "SMSG_BATTLEFIELD_WIN_OBSOLETE",    STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x240*/  StoreOpcode(SMSG_BATTLEFIELD_LOSE_OBSOLETE,    "SMSG_BATTLEFIELD_LOSE_OBSOLETE",   STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x241*/  StoreOpcode(CMSG_TAXICLEARNODE,                "CMSG_TAXICLEARNODE",               STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x242*/  StoreOpcode(CMSG_TAXIENABLENODE,               "CMSG_TAXIENABLENODE",              STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*[-ZERO] Need check */ /*0x243*/  StoreOpcode(CMSG_ITEM_TEXT_QUERY,              "CMSG_ITEM_TEXT_QUERY",             STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleItemTextQuery);
    /*0x244*/  StoreOpcode(SMSG_ITEM_TEXT_QUERY_RESPONSE,     "SMSG_ITEM_TEXT_QUERY_RESPONSE",    STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x245*/  StoreOpcode(CMSG_MAIL_TAKE_MONEY,              "CMSG_MAIL_TAKE_MONEY",             STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleMailTakeMoney);
    /*0x246*/  StoreOpcode(CMSG_MAIL_TAKE_ITEM,               "CMSG_MAIL_TAKE_ITEM",              STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleMailTakeItem);
    /*0x247*/  StoreOpcode(CMSG_MAIL_MARK_AS_READ,            "CMSG_MAIL_MARK_AS_READ",           STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleMailMarkAsRead);
    /*0x248*/  StoreOpcode(CMSG_MAIL_RETURN_TO_SENDER,        "CMSG_MAIL_RETURN_TO_SENDER",       STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleMailReturnToSender);
    /*0x249*/  StoreOpcode(CMSG_MAIL_DELETE,                  "CMSG_MAIL_DELETE",                 STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleMailDelete);
    /*0x24A*/  StoreOpcode(CMSG_MAIL_CREATE_TEXT_ITEM,        "CMSG_MAIL_CREATE_TEXT_ITEM",       STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleMailCreateTextItem);
    /*0x24B*/  StoreOpcode(SMSG_SPELLLOGMISS,                 "SMSG_SPELLLOGMISS",                STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x24C*/  StoreOpcode(SMSG_SPELLLOGEXECUTE,              "SMSG_SPELLLOGEXECUTE",             STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] No effect */ /*0x24D*/  StoreOpcode(SMSG_DEBUGAURAPROC,                "SMSG_DEBUGAURAPROC",               STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x24E*/  StoreOpcode(SMSG_PERIODICAURALOG,              "SMSG_PERIODICAURALOG",             STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x24F*/  StoreOpcode(SMSG_SPELLDAMAGESHIELD,            "SMSG_SPELLDAMAGESHIELD",           STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x250*/  StoreOpcode(SMSG_SPELLNONMELEEDAMAGELOG,       "SMSG_SPELLNONMELEEDAMAGELOG",      STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x251*/  StoreOpcode(CMSG_LEARN_TALENT,                 "CMSG_LEARN_TALENT",                STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleLearnTalentOpcode);
    /*0x252*/  StoreOpcode(SMSG_RESURRECT_FAILED,             "SMSG_RESURRECT_FAILED",            STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x253*/  StoreOpcode(CMSG_TOGGLE_PVP,                   "CMSG_TOGGLE_PVP",                  STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleTogglePvP);
    /*0x254*/  StoreOpcode(SMSG_ZONE_UNDER_ATTACK,            "SMSG_ZONE_UNDER_ATTACK",           STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x255*/  StoreOpcode(MSG_AUCTION_HELLO,                 "MSG_AUCTION_HELLO",                STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleAuctionHelloOpcode);
    /*0x256*/  StoreOpcode(CMSG_AUCTION_SELL_ITEM,            "CMSG_AUCTION_SELL_ITEM",           STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleAuctionSellItem);
    /*0x257*/  StoreOpcode(CMSG_AUCTION_REMOVE_ITEM,          "CMSG_AUCTION_REMOVE_ITEM",         STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleAuctionRemoveItem);
    /*0x258*/  StoreOpcode(CMSG_AUCTION_LIST_ITEMS,           "CMSG_AUCTION_LIST_ITEMS",          STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleAuctionListItems);
    /*[-ZERO] Need check */ /*0x259*/  StoreOpcode(CMSG_AUCTION_LIST_OWNER_ITEMS,     "CMSG_AUCTION_LIST_OWNER_ITEMS",    STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleAuctionListOwnerItems);
    /*0x25A*/  StoreOpcode(CMSG_AUCTION_PLACE_BID,            "CMSG_AUCTION_PLACE_BID",           STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleAuctionPlaceBid);
    /*0x25B*/  StoreOpcode(SMSG_AUCTION_COMMAND_RESULT,       "SMSG_AUCTION_COMMAND_RESULT",      STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x25C*/  StoreOpcode(SMSG_AUCTION_LIST_RESULT,          "SMSG_AUCTION_LIST_RESULT",         STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x25D*/  StoreOpcode(SMSG_AUCTION_OWNER_LIST_RESULT,    "SMSG_AUCTION_OWNER_LIST_RESULT",   STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x25E*/  StoreOpcode(SMSG_AUCTION_BIDDER_NOTIFICATION,  "SMSG_AUCTION_BIDDER_NOTIFICATION", STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x25F*/  StoreOpcode(SMSG_AUCTION_OWNER_NOTIFICATION,   "SMSG_AUCTION_OWNER_NOTIFICATION",  STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x260*/  StoreOpcode(SMSG_PROCRESIST,                   "SMSG_PROCRESIST",                  STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x261*/  StoreOpcode(SMSG_STANDSTATE_CHANGE_FAILURE_OBSOLETE, "SMSG_STANDSTATE_CHANGE_FAILURE_OBSOLETE", STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x262*/  StoreOpcode(SMSG_DISPEL_FAILED,                "SMSG_DISPEL_FAILED",               STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x263*/  StoreOpcode(SMSG_SPELLORDAMAGE_IMMUNE,         "SMSG_SPELLORDAMAGE_IMMUNE",        STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x264*/  StoreOpcode(CMSG_AUCTION_LIST_BIDDER_ITEMS,    "CMSG_AUCTION_LIST_BIDDER_ITEMS",   STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleAuctionListBidderItems);
    /*0x265*/  StoreOpcode(SMSG_AUCTION_BIDDER_LIST_RESULT,   "SMSG_AUCTION_BIDDER_LIST_RESULT",  STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x266*/  StoreOpcode(SMSG_SET_FLAT_SPELL_MODIFIER,      "SMSG_SET_FLAT_SPELL_MODIFIER",     STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x267*/  StoreOpcode(SMSG_SET_PCT_SPELL_MODIFIER,       "SMSG_SET_PCT_SPELL_MODIFIER",      STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x268*/  StoreOpcode(CMSG_SET_AMMO,                     "CMSG_SET_AMMO",                    STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleSetAmmoOpcode);
    /*0x269*/  StoreOpcode(SMSG_CORPSE_RECLAIM_DELAY,         "SMSG_CORPSE_RECLAIM_DELAY",        STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x26A*/  StoreOpcode(CMSG_SET_ACTIVE_MOVER,             "CMSG_SET_ACTIVE_MOVER",            STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleSetActiveMoverOpcode);
    /*[-ZERO] Need check */ /*0x26B*/  StoreOpcode(CMSG_PET_CANCEL_AURA,              "CMSG_PET_CANCEL_AURA",             STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandlePetCancelAuraOpcode);
    /*0x26C*/  StoreOpcode(CMSG_PLAYER_AI_CHEAT,              "CMSG_PLAYER_AI_CHEAT",             STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*[-ZERO] Need check */ /*0x26D*/  StoreOpcode(CMSG_CANCEL_AUTO_REPEAT_SPELL,     "CMSG_CANCEL_AUTO_REPEAT_SPELL",    STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleCancelAutoRepeatSpellOpcode);
    /*0x26E*/  StoreOpcode(MSG_GM_ACCOUNT_ONLINE,             "MSG_GM_ACCOUNT_ONLINE",            STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x26F*/  StoreOpcode(MSG_LIST_STABLED_PETS,             "MSG_LIST_STABLED_PETS",            STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleListStabledPetsOpcode);
    /*0x270*/  StoreOpcode(CMSG_STABLE_PET,                   "CMSG_STABLE_PET",                  STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleStablePet);
    /*[-ZERO] Need check */ /*0x271*/  StoreOpcode(CMSG_UNSTABLE_PET,                 "CMSG_UNSTABLE_PET",                STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleUnstablePet);
    /*[-ZERO] Need check */ /*0x272*/  StoreOpcode(CMSG_BUY_STABLE_SLOT,              "CMSG_BUY_STABLE_SLOT",             STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleBuyStableSlot);
    /*0x273*/  StoreOpcode(SMSG_STABLE_RESULT,                "SMSG_STABLE_RESULT",               STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x274*/  StoreOpcode(CMSG_STABLE_REVIVE_PET,            "CMSG_STABLE_REVIVE_PET",           STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleStableRevivePet);
    /*0x275*/  StoreOpcode(CMSG_STABLE_SWAP_PET,              "CMSG_STABLE_SWAP_PET",             STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleStableSwapPet);
    /*0x276*/  StoreOpcode(MSG_QUEST_PUSH_RESULT,             "MSG_QUEST_PUSH_RESULT",            STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleQuestPushResult);
    /*0x277*/  StoreOpcode(SMSG_PLAY_MUSIC,                   "SMSG_PLAY_MUSIC",                  STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x278*/  StoreOpcode(SMSG_PLAY_OBJECT_SOUND,            "SMSG_PLAY_OBJECT_SOUND",           STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x279*/  StoreOpcode(CMSG_REQUEST_PET_INFO,             "CMSG_REQUEST_PET_INFO",            STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleRequestPetInfoOpcode);
    /*[-ZERO] Need check */ /*0x27A*/  StoreOpcode(CMSG_FAR_SIGHT,                    "CMSG_FAR_SIGHT",                   STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleFarSightOpcode);
    /*0x27B*/  StoreOpcode(SMSG_SPELLDISPELLOG,               "SMSG_SPELLDISPELLOG",              STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x27C*/  StoreOpcode(SMSG_DAMAGE_CALC_LOG,              "SMSG_DAMAGE_CALC_LOG",             STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x27D*/  StoreOpcode(CMSG_ENABLE_DAMAGE_LOG,            "CMSG_ENABLE_DAMAGE_LOG",           STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*[-ZERO] Need check */ /*0x27E*/  StoreOpcode(CMSG_GROUP_CHANGE_SUB_GROUP,       "CMSG_GROUP_CHANGE_SUB_GROUP",      STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleGroupChangeSubGroupOpcode);
    /*0x27F*/  StoreOpcode(CMSG_REQUEST_PARTY_MEMBER_STATS,   "CMSG_REQUEST_PARTY_MEMBER_STATS",  STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleRequestPartyMemberStatsOpcode);
    /*0x280*/  StoreOpcode(CMSG_GROUP_SWAP_SUB_GROUP,         "CMSG_GROUP_SWAP_SUB_GROUP",        STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x281*/  StoreOpcode(CMSG_RESET_FACTION_CHEAT,          "CMSG_RESET_FACTION_CHEAT",         STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*[-ZERO] Need check */ /*0x282*/  StoreOpcode(CMSG_AUTOSTORE_BANK_ITEM,          "CMSG_AUTOSTORE_BANK_ITEM",         STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleAutoStoreBankItemOpcode);
    /*[-ZERO] Need check */ /*0x283*/  StoreOpcode(CMSG_AUTOBANK_ITEM,                "CMSG_AUTOBANK_ITEM",               STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleAutoBankItemOpcode);
    /*0x284*/  StoreOpcode(MSG_QUERY_NEXT_MAIL_TIME,          "MSG_QUERY_NEXT_MAIL_TIME",         STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleQueryNextMailTime);
    /*0x285*/  StoreOpcode(SMSG_RECEIVED_MAIL,                "SMSG_RECEIVED_MAIL",               STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x286*/  StoreOpcode(SMSG_RAID_GROUP_ONLY,              "SMSG_RAID_GROUP_ONLY",             STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x287*/  StoreOpcode(CMSG_SET_DURABILITY_CHEAT,         "CMSG_SET_DURABILITY_CHEAT",        STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x288*/  StoreOpcode(CMSG_SET_PVP_RANK_CHEAT,           "CMSG_SET_PVP_RANK_CHEAT",          STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x289*/  StoreOpcode(CMSG_ADD_PVP_MEDAL_CHEAT,          "CMSG_ADD_PVP_MEDAL_CHEAT",         STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x28A*/  StoreOpcode(CMSG_DEL_PVP_MEDAL_CHEAT,          "CMSG_DEL_PVP_MEDAL_CHEAT",         STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x28B*/  StoreOpcode(CMSG_SET_PVP_TITLE,                "CMSG_SET_PVP_TITLE",               STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x28C*/  StoreOpcode(SMSG_PVP_CREDIT,                   "SMSG_PVP_CREDIT",                  STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x28D*/  StoreOpcode(SMSG_AUCTION_REMOVED_NOTIFICATION, "SMSG_AUCTION_REMOVED_NOTIFICATION", STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x28E*/  StoreOpcode(CMSG_GROUP_RAID_CONVERT,           "CMSG_GROUP_RAID_CONVERT",          STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleGroupRaidConvertOpcode);
    /*[-ZERO] Need check */ /*0x28F*/  StoreOpcode(CMSG_GROUP_ASSISTANT_LEADER,       "CMSG_GROUP_ASSISTANT_LEADER",      STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleGroupAssistantLeaderOpcode);
    /*0x290*/  StoreOpcode(CMSG_BUYBACK_ITEM,                 "CMSG_BUYBACK_ITEM",                STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleBuybackItem);
    /*0x291*/  StoreOpcode(SMSG_SERVER_MESSAGE,               "SMSG_SERVER_MESSAGE",              STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x292*/  StoreOpcode(CMSG_MEETINGSTONE_JOIN,            "CMSG_MEETINGSTONE_JOIN",           STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleMeetingStoneJoinOpcode);
    /*0x293*/  StoreOpcode(CMSG_MEETINGSTONE_LEAVE,           "CMSG_MEETINGSTONE_LEAVE",          STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleMeetingStoneLeaveOpcode);
    /*0x294*/  StoreOpcode(CMSG_MEETINGSTONE_CHEAT,           "CMSG_MEETINGSTONE_CHEAT",          STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x295*/  StoreOpcode(SMSG_MEETINGSTONE_SETQUEUE,        "SMSG_MEETINGSTONE_SETQUEUE",       STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x296*/  StoreOpcode(CMSG_MEETINGSTONE_INFO,            "CMSG_MEETINGSTONE_INFO",           STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleMeetingStoneInfoOpcode);
    /*0x297*/  StoreOpcode(SMSG_MEETINGSTONE_COMPLETE,        "SMSG_MEETINGSTONE_COMPLETE",       STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x298*/  StoreOpcode(SMSG_MEETINGSTONE_IN_PROGRESS,     "SMSG_MEETINGSTONE_IN_PROGRESS",    STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x299*/  StoreOpcode(SMSG_MEETINGSTONE_MEMBER_ADDED,    "SMSG_MEETINGSTONE_MEMBER_ADDED",   STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x29A*/  StoreOpcode(CMSG_GMTICKETSYSTEM_TOGGLE,        "CMSG_GMTICKETSYSTEM_TOGGLE",       STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*[-ZERO] Need check */ /*0x29B*/  StoreOpcode(CMSG_CANCEL_GROWTH_AURA,           "CMSG_CANCEL_GROWTH_AURA",          STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleCancelGrowthAuraOpcode);
    /*0x29C*/  StoreOpcode(SMSG_CANCEL_AUTO_REPEAT,           "SMSG_CANCEL_AUTO_REPEAT",          STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x29D*/  StoreOpcode(SMSG_STANDSTATE_UPDATE,            "SMSG_STANDSTATE_UPDATE",           STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x29E*/  StoreOpcode(SMSG_LOOT_ALL_PASSED,              "SMSG_LOOT_ALL_PASSED",             STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x29F*/  StoreOpcode(SMSG_LOOT_ROLL_WON,                "SMSG_LOOT_ROLL_WON",               STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x2A0*/  StoreOpcode(CMSG_LOOT_ROLL,                    "CMSG_LOOT_ROLL",                   STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleLootRoll);
    /*0x2A1*/  StoreOpcode(SMSG_LOOT_START_ROLL,              "SMSG_LOOT_START_ROLL",             STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x2A2*/  StoreOpcode(SMSG_LOOT_ROLL,                    "SMSG_LOOT_ROLL",                   STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x2A3*/  StoreOpcode(CMSG_LOOT_MASTER_GIVE,             "CMSG_LOOT_MASTER_GIVE",            STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleLootMasterGiveOpcode);
    /*0x2A4*/  StoreOpcode(SMSG_LOOT_MASTER_LIST,             "SMSG_LOOT_MASTER_LIST",            STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x2A5*/  StoreOpcode(SMSG_SET_FORCED_REACTIONS,         "SMSG_SET_FORCED_REACTIONS",        STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x2A6*/  StoreOpcode(SMSG_SPELL_FAILED_OTHER,           "SMSG_SPELL_FAILED_OTHER",          STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x2A7*/  StoreOpcode(SMSG_GAMEOBJECT_RESET_STATE,       "SMSG_GAMEOBJECT_RESET_STATE",      STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x2A8*/  StoreOpcode(CMSG_REPAIR_ITEM,                  "CMSG_REPAIR_ITEM",                 STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleRepairItemOpcode);
    /*0x2A9*/  StoreOpcode(SMSG_CHAT_PLAYER_NOT_FOUND,        "SMSG_CHAT_PLAYER_NOT_FOUND",       STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x2AA*/  StoreOpcode(MSG_TALENT_WIPE_CONFIRM,           "MSG_TALENT_WIPE_CONFIRM",          STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleTalentWipeConfirmOpcode);
    /*0x2AB*/  StoreOpcode(SMSG_SUMMON_REQUEST,               "SMSG_SUMMON_REQUEST",              STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x2AC*/  StoreOpcode(CMSG_SUMMON_RESPONSE,              "CMSG_SUMMON_RESPONSE",             STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleSummonResponseOpcode);
    /*0x2AD*/  StoreOpcode(MSG_MOVE_TOGGLE_GRAVITY_CHEAT,     "MSG_MOVE_TOGGLE_GRAVITY_CHEAT",    STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x2AE*/  StoreOpcode(SMSG_MONSTER_MOVE_TRANSPORT,       "SMSG_MONSTER_MOVE_TRANSPORT",      STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x2AF*/  StoreOpcode(SMSG_PET_BROKEN,                   "SMSG_PET_BROKEN",                  STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x2B0*/  StoreOpcode(MSG_MOVE_FEATHER_FALL,             "MSG_MOVE_FEATHER_FALL",            STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x2B1*/  StoreOpcode(MSG_MOVE_WATER_WALK,               "MSG_MOVE_WATER_WALK",              STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x2B2*/  StoreOpcode(CMSG_SERVER_BROADCAST,             "CMSG_SERVER_BROADCAST",            STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*[-ZERO] Need check */ /*0x2B3*/  StoreOpcode(CMSG_SELF_RES,                     "CMSG_SELF_RES",                    STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleSelfResOpcode);
    /*0x2B4*/  StoreOpcode(SMSG_FEIGN_DEATH_RESISTED,         "SMSG_FEIGN_DEATH_RESISTED",        STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x2B5*/  StoreOpcode(CMSG_RUN_SCRIPT,                   "CMSG_RUN_SCRIPT",                  STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*[-ZERO] Need check */ /*0x2B6*/  StoreOpcode(SMSG_SCRIPT_MESSAGE,               "SMSG_SCRIPT_MESSAGE",              STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x2B7*/  StoreOpcode(SMSG_DUEL_COUNTDOWN,               "SMSG_DUEL_COUNTDOWN",              STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x2B8*/  StoreOpcode(SMSG_AREA_TRIGGER_MESSAGE,         "SMSG_AREA_TRIGGER_MESSAGE",        STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x2B9*/  StoreOpcode(CMSG_TOGGLE_HELM,                  "CMSG_TOGGLE_HELM",                 STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleShowingHelmOpcode);
    /*0x2BA*/  StoreOpcode(CMSG_TOGGLE_CLOAK,                 "CMSG_TOGGLE_CLOAK",                STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleShowingCloakOpcode);
    /*0x2BB*/  StoreOpcode(SMSG_MEETINGSTONE_JOINFAILED,      "SMSG_MEETINGSTONE_JOINFAILED",     STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x2BC*/  StoreOpcode(SMSG_PLAYER_SKINNED,               "SMSG_PLAYER_SKINNED",              STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x2BD*/  StoreOpcode(SMSG_DURABILITY_DAMAGE_DEATH,      "SMSG_DURABILITY_DAMAGE_DEATH",     STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x2BE*/  StoreOpcode(CMSG_SET_EXPLORATION,              "CMSG_SET_EXPLORATION",             STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x2BF*/  StoreOpcode(CMSG_SET_ACTIONBAR_TOGGLES,        "CMSG_SET_ACTIONBAR_TOGGLES",       STATUS_AUTHED,    PROCESS_THREADUNSAFE, &WorldSession::HandleSetActionBarTogglesOpcode);
    /*0x2C0*/  StoreOpcode(UMSG_DELETE_GUILD_CHARTER,         "UMSG_DELETE_GUILD_CHARTER",        STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x2C1*/  StoreOpcode(MSG_PETITION_RENAME,               "MSG_PETITION_RENAME",              STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandlePetitionRenameOpcode);
    /*0x2C2*/  StoreOpcode(SMSG_INIT_WORLD_STATES,            "SMSG_INIT_WORLD_STATES",           STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x2C3*/  StoreOpcode(SMSG_UPDATE_WORLD_STATE,           "SMSG_UPDATE_WORLD_STATE",          STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x2C4*/  StoreOpcode(CMSG_ITEM_NAME_QUERY,              "CMSG_ITEM_NAME_QUERY",             STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleItemNameQueryOpcode);
    /*0x2C5*/  StoreOpcode(SMSG_ITEM_NAME_QUERY_RESPONSE,     "SMSG_ITEM_NAME_QUERY_RESPONSE",    STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x2C6*/  StoreOpcode(SMSG_PET_ACTION_FEEDBACK,          "SMSG_PET_ACTION_FEEDBACK",         STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x2C7*/  StoreOpcode(CMSG_CHAR_RENAME,                  "CMSG_CHAR_RENAME",                 STATUS_AUTHED,    PROCESS_THREADUNSAFE, &WorldSession::HandleCharRenameOpcode);
    /*0x2C8*/  StoreOpcode(SMSG_CHAR_RENAME,                  "SMSG_CHAR_RENAME",                 STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x2C9*/  StoreOpcode(CMSG_MOVE_SPLINE_DONE,             "CMSG_MOVE_SPLINE_DONE",            STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleMoveSplineDoneOpcode);
    /*0x2CA*/  StoreOpcode(CMSG_MOVE_FALL_RESET,              "CMSG_MOVE_FALL_RESET",             STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleMovementOpcodes);
    /*0x2CB*/  StoreOpcode(SMSG_INSTANCE_SAVE_CREATED,        "SMSG_INSTANCE_SAVE_CREATED",       STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x2CC*/  StoreOpcode(SMSG_RAID_INSTANCE_INFO,           "SMSG_RAID_INSTANCE_INFO",          STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x2CD*/  StoreOpcode(CMSG_REQUEST_RAID_INFO,            "CMSG_REQUEST_RAID_INFO",           STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleRequestRaidInfoOpcode);
    /*[-ZERO] Need check */ /*0x2CE*/  StoreOpcode(CMSG_MOVE_TIME_SKIPPED,            "CMSG_MOVE_TIME_SKIPPED",           STATUS_LOGGEDIN,  PROCESS_INPLACE,      &WorldSession::HandleMoveTimeSkippedOpcode);
    /*0x2CF*/  StoreOpcode(CMSG_MOVE_FEATHER_FALL_ACK,        "CMSG_MOVE_FEATHER_FALL_ACK",       STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleFeatherFallAck);
    /*0x2D0*/  StoreOpcode(CMSG_MOVE_WATER_WALK_ACK,          "CMSG_MOVE_WATER_WALK_ACK",         STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleMoveWaterWalkAck);
    /*0x2D1*/  StoreOpcode(CMSG_MOVE_NOT_ACTIVE_MOVER,        "CMSG_MOVE_NOT_ACTIVE_MOVER",       STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleMoveNotActiveMoverOpcode);
    /*0x2D2*/  StoreOpcode(SMSG_PLAY_SOUND,                   "SMSG_PLAY_SOUND",                  STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x2D3*/  StoreOpcode(CMSG_BATTLEFIELD_STATUS,           "CMSG_BATTLEFIELD_STATUS",          STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleBattlefieldStatusOpcode);
    /*0x2D4*/  StoreOpcode(SMSG_BATTLEFIELD_STATUS,           "SMSG_BATTLEFIELD_STATUS",          STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x2D5*/  StoreOpcode(CMSG_BATTLEFIELD_PORT,             "CMSG_BATTLEFIELD_PORT",            STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleBattleFieldPortOpcode);
    /*0x2D6*/  StoreOpcode(MSG_INSPECT_HONOR_STATS,           "MSG_INSPECT_HONOR_STATS",          STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleInspectHonorStatsOpcode);
    /*0x2D7*/  StoreOpcode(CMSG_BATTLEMASTER_HELLO,           "CMSG_BATTLEMASTER_HELLO",          STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleBattlemasterHelloOpcode);
    /*0x2D8*/  StoreOpcode(CMSG_MOVE_START_SWIM_CHEAT,        "CMSG_MOVE_START_SWIM_CHEAT",       STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x2D9*/  StoreOpcode(CMSG_MOVE_STOP_SWIM_CHEAT,         "CMSG_MOVE_STOP_SWIM_CHEAT",        STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x2DA*/  StoreOpcode(SMSG_FORCE_WALK_SPEED_CHANGE,      "SMSG_FORCE_WALK_SPEED_CHANGE",     STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x2DB*/  StoreOpcode(CMSG_FORCE_WALK_SPEED_CHANGE_ACK,  "CMSG_FORCE_WALK_SPEED_CHANGE_ACK", STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleForceSpeedChangeAckOpcodes);
    /*0x2DC*/  StoreOpcode(SMSG_FORCE_SWIM_BACK_SPEED_CHANGE, "SMSG_FORCE_SWIM_BACK_SPEED_CHANGE", STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x2DD*/  StoreOpcode(CMSG_FORCE_SWIM_BACK_SPEED_CHANGE_ACK,  "CMSG_FORCE_SWIM_BACK_SPEED_CHANGE_ACK",   STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleForceSpeedChangeAckOpcodes);
    /*0x2DE*/  StoreOpcode(SMSG_FORCE_TURN_RATE_CHANGE,       "SMSG_FORCE_TURN_RATE_CHANGE",      STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x2DF*/  StoreOpcode(CMSG_FORCE_TURN_RATE_CHANGE_ACK,   "CMSG_FORCE_TURN_RATE_CHANGE_ACK",  STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleForceSpeedChangeAckOpcodes);
    /*0x2E0*/  StoreOpcode(MSG_PVP_LOG_DATA,                  "MSG_PVP_LOG_DATA",                 STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandlePVPLogDataOpcode);
    /*0x2E1*/  StoreOpcode(CMSG_LEAVE_BATTLEFIELD,            "CMSG_LEAVE_BATTLEFIELD",           STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleLeaveBattlefieldOpcode);
    /*[-ZERO] Need check */ /*0x2E2*/  StoreOpcode(CMSG_AREA_SPIRIT_HEALER_QUERY,     "CMSG_AREA_SPIRIT_HEALER_QUERY",    STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleAreaSpiritHealerQueryOpcode);
    /*[-ZERO] Need check */ /*0x2E3*/  StoreOpcode(CMSG_AREA_SPIRIT_HEALER_QUEUE,     "CMSG_AREA_SPIRIT_HEALER_QUEUE",    STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleAreaSpiritHealerQueueOpcode);
    /*0x2E4*/  StoreOpcode(SMSG_AREA_SPIRIT_HEALER_TIME,      "SMSG_AREA_SPIRIT_HEALER_TIME",     STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x2E5*/  StoreOpcode(CMSG_GM_UNTEACH,                   "CMSG_GM_UNTEACH",                  STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*[-ZERO] Need check */ /*0x2E6*/  StoreOpcode(SMSG_WARDEN_DATA,                  "SMSG_WARDEN_DATA",                 STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x2E7*/  StoreOpcode(CMSG_WARDEN_DATA,                  "CMSG_WARDEN_DATA",                 STATUS_AUTHED,    PROCESS_THREADUNSAFE, &WorldSession::HandleWardenDataOpcode);
    /*0x2E8*/  StoreOpcode(SMSG_GROUP_JOINED_BATTLEGROUND,    "SMSG_GROUP_JOINED_BATTLEGROUND",   STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x2E9*/  StoreOpcode(MSG_BATTLEGROUND_PLAYER_POSITIONS, "MSG_BATTLEGROUND_PLAYER_POSITIONS", STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleBattleGroundPlayerPositionsOpcode);
    /*0x2EA*/  StoreOpcode(CMSG_PET_STOP_ATTACK,              "CMSG_PET_STOP_ATTACK",             STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandlePetStopAttack);
    /*0x2EB*/  StoreOpcode(SMSG_BINDER_CONFIRM,               "SMSG_BINDER_CONFIRM",              STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x2EC*/  StoreOpcode(SMSG_BATTLEGROUND_PLAYER_JOINED,   "SMSG_BATTLEGROUND_PLAYER_JOINED",  STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x2ED*/  StoreOpcode(SMSG_BATTLEGROUND_PLAYER_LEFT,     "SMSG_BATTLEGROUND_PLAYER_LEFT",    STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x2EE*/  StoreOpcode(CMSG_BATTLEMASTER_JOIN,            "CMSG_BATTLEMASTER_JOIN",           STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleBattlemasterJoinOpcode);
    /*0x2EF*/  StoreOpcode(SMSG_ADDON_INFO,                   "SMSG_ADDON_INFO",                  STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x2F0*/  StoreOpcode(CMSG_PET_UNLEARN,                  "CMSG_PET_UNLEARN",                 STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandlePetUnlearnOpcode);
    /*0x2F1*/  StoreOpcode(SMSG_PET_UNLEARN_CONFIRM,          "SMSG_PET_UNLEARN_CONFIRM",         STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x2F2*/  StoreOpcode(SMSG_PARTY_MEMBER_STATS_FULL,      "SMSG_PARTY_MEMBER_STATS_FULL",     STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x2F3*/  StoreOpcode(CMSG_PET_SPELL_AUTOCAST,           "CMSG_PET_SPELL_AUTOCAST",          STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandlePetSpellAutocastOpcode);
    /*0x2F4*/  StoreOpcode(SMSG_WEATHER,                      "SMSG_WEATHER",                     STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x2F5*/  StoreOpcode(SMSG_PLAY_TIME_WARNING,            "SMSG_PLAY_TIME_WARNING",           STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x2F6*/  StoreOpcode(SMSG_MINIGAME_SETUP,               "SMSG_MINIGAME_SETUP",              STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x2F7*/  StoreOpcode(SMSG_MINIGAME_STATE,               "SMSG_MINIGAME_STATE",              STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x2F8*/  StoreOpcode(CMSG_MINIGAME_MOVE,                "CMSG_MINIGAME_MOVE",               STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*[-ZERO] Need check */ /*0x2F9*/  StoreOpcode(SMSG_MINIGAME_MOVE_FAILED,         "SMSG_MINIGAME_MOVE_FAILED",        STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x2FA*/  StoreOpcode(SMSG_RAID_INSTANCE_MESSAGE,        "SMSG_RAID_INSTANCE_MESSAGE",       STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x2FB*/  StoreOpcode(SMSG_COMPRESSED_MOVES,             "SMSG_COMPRESSED_MOVES",            STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x2FC*/  StoreOpcode(CMSG_GUILD_INFO_TEXT,              "CMSG_GUILD_INFO_TEXT",             STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleGuildChangeInfoTextOpcode);
    /*0x2FD*/  StoreOpcode(SMSG_CHAT_RESTRICTED,              "SMSG_CHAT_RESTRICTED",             STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x2FE*/  StoreOpcode(SMSG_SPLINE_SET_RUN_SPEED,         "SMSG_SPLINE_SET_RUN_SPEED",        STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x2FF*/  StoreOpcode(SMSG_SPLINE_SET_RUN_BACK_SPEED,    "SMSG_SPLINE_SET_RUN_BACK_SPEED",   STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x300*/  StoreOpcode(SMSG_SPLINE_SET_SWIM_SPEED,        "SMSG_SPLINE_SET_SWIM_SPEED",       STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x301*/  StoreOpcode(SMSG_SPLINE_SET_WALK_SPEED,        "SMSG_SPLINE_SET_WALK_SPEED",       STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x302*/  StoreOpcode(SMSG_SPLINE_SET_SWIM_BACK_SPEED,   "SMSG_SPLINE_SET_SWIM_BACK_SPEED",  STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x303*/  StoreOpcode(SMSG_SPLINE_SET_TURN_RATE,         "SMSG_SPLINE_SET_TURN_RATE",        STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x304*/  StoreOpcode(SMSG_SPLINE_MOVE_UNROOT,           "SMSG_SPLINE_MOVE_UNROOT",          STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x305*/  StoreOpcode(SMSG_SPLINE_MOVE_FEATHER_FALL,     "SMSG_SPLINE_MOVE_FEATHER_FALL",    STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x306*/  StoreOpcode(SMSG_SPLINE_MOVE_NORMAL_FALL,      "SMSG_SPLINE_MOVE_NORMAL_FALL",     STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x307*/  StoreOpcode(SMSG_SPLINE_MOVE_SET_HOVER,        "SMSG_SPLINE_MOVE_SET_HOVER",       STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x308*/  StoreOpcode(SMSG_SPLINE_MOVE_UNSET_HOVER,      "SMSG_SPLINE_MOVE_UNSET_HOVER",     STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x309*/  StoreOpcode(SMSG_SPLINE_MOVE_WATER_WALK,       "SMSG_SPLINE_MOVE_WATER_WALK",      STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x30A*/  StoreOpcode(SMSG_SPLINE_MOVE_LAND_WALK,        "SMSG_SPLINE_MOVE_LAND_WALK",       STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x30B*/  StoreOpcode(SMSG_SPLINE_MOVE_START_SWIM,       "SMSG_SPLINE_MOVE_START_SWIM",      STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x30C*/  StoreOpcode(SMSG_SPLINE_MOVE_STOP_SWIM,        "SMSG_SPLINE_MOVE_STOP_SWIM",       STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x30D*/  StoreOpcode(SMSG_SPLINE_MOVE_SET_RUN_MODE,     "SMSG_SPLINE_MOVE_SET_RUN_MODE",    STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x30E*/  StoreOpcode(SMSG_SPLINE_MOVE_SET_WALK_MODE,    "SMSG_SPLINE_MOVE_SET_WALK_MODE",   STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x30F*/  StoreOpcode(CMSG_GM_NUKE_ACCOUNT,              "CMSG_GM_NUKE_ACCOUNT",             STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*[-ZERO] Need check */ /*0x310*/  StoreOpcode(MSG_GM_DESTROY_CORPSE,             "MSG_GM_DESTROY_CORPSE",            STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x311*/  StoreOpcode(CMSG_GM_DESTROY_ONLINE_CORPSE,     "CMSG_GM_DESTROY_ONLINE_CORPSE",    STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*[-ZERO] Need check */ /*0x312*/  StoreOpcode(CMSG_ACTIVATETAXIEXPRESS,          "CMSG_ACTIVATETAXIEXPRESS",         STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleActivateTaxiExpressOpcode);
    /*0x313*/  StoreOpcode(SMSG_SET_FACTION_ATWAR,            "SMSG_SET_FACTION_ATWAR",           STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x314*/  StoreOpcode(SMSG_GAMETIMEBIAS_SET,             "SMSG_GAMETIMEBIAS_SET",            STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x315*/  StoreOpcode(CMSG_DEBUG_ACTIONS_START,          "CMSG_DEBUG_ACTIONS_START",         STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x316*/  StoreOpcode(CMSG_DEBUG_ACTIONS_STOP,           "CMSG_DEBUG_ACTIONS_STOP",          STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*[-ZERO] Need check */ /*0x317*/  StoreOpcode(CMSG_SET_FACTION_INACTIVE,         "CMSG_SET_FACTION_INACTIVE",        STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleSetFactionInactiveOpcode);
    /*[-ZERO] Need check */ /*0x318*/  StoreOpcode(CMSG_SET_WATCHED_FACTION,          "CMSG_SET_WATCHED_FACTION",         STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleSetWatchedFactionOpcode);
    /*0x319*/  StoreOpcode(MSG_MOVE_TIME_SKIPPED,             "MSG_MOVE_TIME_SKIPPED",            STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x31A*/  StoreOpcode(SMSG_SPLINE_MOVE_ROOT,             "SMSG_SPLINE_MOVE_ROOT",            STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x31B*/  StoreOpcode(CMSG_SET_EXPLORATION_ALL,          "CMSG_SET_EXPLORATION_ALL",         STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x31C*/  StoreOpcode(SMSG_INVALIDATE_PLAYER,            "SMSG_INVALIDATE_PLAYER",           STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x31D*/  StoreOpcode(CMSG_RESET_INSTANCES,              "CMSG_RESET_INSTANCES",             STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleResetInstancesOpcode);
    /*0x31E*/  StoreOpcode(SMSG_INSTANCE_RESET,               "SMSG_INSTANCE_RESET",              STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x31F*/  StoreOpcode(SMSG_INSTANCE_RESET_FAILED,        "SMSG_INSTANCE_RESET_FAILED",       STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x320*/  StoreOpcode(SMSG_UPDATE_LAST_INSTANCE,         "SMSG_UPDATE_LAST_INSTANCE",        STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x321*/  StoreOpcode(MSG_RAID_TARGET_UPDATE,            "MSG_RAID_TARGET_UPDATE",           STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleRaidTargetUpdateOpcode);
    /*0x322*/  StoreOpcode(MSG_RAID_READY_CHECK,              "MSG_RAID_READY_CHECK",             STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleRaidReadyCheckOpcode);
    /*0x323*/  StoreOpcode(CMSG_LUA_USAGE,                    "CMSG_LUA_USAGE",                   STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x324*/  StoreOpcode(SMSG_PET_ACTION_SOUND,             "SMSG_PET_ACTION_SOUND",            STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x325*/  StoreOpcode(SMSG_PET_DISMISS_SOUND,            "SMSG_PET_DISMISS_SOUND",           STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x326*/  StoreOpcode(SMSG_GHOSTEE_GONE,                 "SMSG_GHOSTEE_GONE",                STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x327*/  StoreOpcode(CMSG_GM_UPDATE_TICKET_STATUS,      "CMSG_GM_UPDATE_TICKET_STATUS",     STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x328*/  StoreOpcode(SMSG_GM_TICKET_STATUS_UPDATE,      "SMSG_GM_TICKET_STATUS_UPDATE",     STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x32A*/  StoreOpcode(CMSG_GMSURVEY_SUBMIT,              "CMSG_GMSURVEY_SUBMIT",             STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleGMTicketSurveySubmitOpcode);
    /*0x32B*/  StoreOpcode(SMSG_UPDATE_INSTANCE_OWNERSHIP,    "SMSG_UPDATE_INSTANCE_OWNERSHIP",   STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x32C*/  StoreOpcode(CMSG_IGNORE_KNOCKBACK_CHEAT,       "CMSG_IGNORE_KNOCKBACK_CHEAT",      STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x32D*/  StoreOpcode(SMSG_CHAT_PLAYER_AMBIGUOUS,        "SMSG_CHAT_PLAYER_AMBIGUOUS",       STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x32E*/  StoreOpcode(MSG_DELAY_GHOST_TELEPORT,          "MSG_DELAY_GHOST_TELEPORT",         STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x32F*/  StoreOpcode(SMSG_SPELLINSTAKILLLOG,            "SMSG_SPELLINSTAKILLLOG",           STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x330*/  StoreOpcode(SMSG_SPELL_UPDATE_CHAIN_TARGETS,   "SMSG_SPELL_UPDATE_CHAIN_TARGETS",  STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x331*/  StoreOpcode(CMSG_CHAT_FILTERED,                "CMSG_CHAT_FILTERED",               STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x332*/  StoreOpcode(SMSG_EXPECTED_SPAM_RECORDS,        "SMSG_EXPECTED_SPAM_RECORDS",       STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x333*/  StoreOpcode(SMSG_SPELLSTEALLOG,                "SMSG_SPELLSTEALLOG",               STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x334*/  StoreOpcode(CMSG_LOTTERY_QUERY_OBSOLETE,       "CMSG_LOTTERY_QUERY_OBSOLETE",      STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*0x335*/  StoreOpcode(SMSG_LOTTERY_QUERY_RESULT_OBSOLETE, "SMSG_LOTTERY_QUERY_RESULT_OBSOLETE",      STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*0x336*/  StoreOpcode(CMSG_BUY_LOTTERY_TICKET_OBSOLETE,  "CMSG_BUY_LOTTERY_TICKET_OBSOLETE", STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*[-ZERO] No effect */ /*0x337*/  StoreOpcode(SMSG_LOTTERY_RESULT_OBSOLETE,      "SMSG_LOTTERY_RESULT_OBSOLETE",     STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x338*/  StoreOpcode(SMSG_CHARACTER_PROFILE,            "SMSG_CHARACTER_PROFILE",           STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x339*/  StoreOpcode(SMSG_CHARACTER_PROFILE_REALM_CONNECTED, "SMSG_CHARACTER_PROFILE_REALM_CONNECTED",  STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x33A*/  StoreOpcode(SMSG_DEFENSE_MESSAGE,              "SMSG_DEFENSE_MESSAGE",             STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x33C*/  StoreOpcode(MSG_GM_RESETINSTANCELIMIT,         "MSG_GM_RESETINSTANCELIMIT",        STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*[-ZERO] Need check */ /*0x33E*/  StoreOpcode(SMSG_MOVE_SET_FLIGHT,              "SMSG_MOVE_SET_FLIGHT_OBSOLETE",    STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x33F*/  StoreOpcode(SMSG_MOVE_UNSET_FLIGHT,            "SMSG_MOVE_UNSET_FLIGHT_OBSOLETE",  STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);
    /*[-ZERO] Need check */ /*0x340*/  StoreOpcode(CMSG_MOVE_FLIGHT_ACK,              "CMSG_MOVE_FLIGHT_ACK_OBSOLETE",    STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*[-ZERO] Need check */ /*0x341*/  StoreOpcode(MSG_MOVE_START_SWIM_CHEAT,         "MSG_MOVE_START_SWIM_CHEAT",        STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);
    /*[-ZERO] Need check */ /*0x342*/  StoreOpcode(MSG_MOVE_STOP_SWIM_CHEAT,          "MSG_MOVE_STOP_SWIM_CHEAT",         STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);

    // [-ZERO] Last existed in 1.12.1 opcode, maybe some renumbering from other side
    StoreOpcode(CMSG_CANCEL_MOUNT_AURA,            "CMSG_CANCEL_MOUNT_AURA",           STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleCancelMountAuraOpcode);     /// 0x375: @TODO need to check usage in vanilla WoW
    StoreOpcode(CMSG_CANCEL_TEMP_ENCHANTMENT,      "CMSG_CANCEL_TEMP_ENCHANTMENT",     STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleCancelTempEnchantmentOpcode);       /// 0x379: @TODO need to check usage in vanilla WoW
    StoreOpcode(CMSG_SET_TAXI_BENCHMARK_MODE,      "CMSG_SET_TAXI_BENCHMARK_MODE",     STATUS_AUTHED,    PROCESS_THREADUNSAFE, &WorldSession::HandleSetTaxiBenchmarkOpcode);        /// 0x389: @TODO need to check usage in vanilla WoW
    StoreOpcode(CMSG_MOVE_CHNG_TRANSPORT,          "CMSG_MOVE_CHNG_TRANSPORT",         STATUS_LOGGEDIN,  PROCESS_THREADSAFE,   &WorldSession::HandleMovementOpcodes);       /// 0x38D: @TODO need to check usage in vanilla WoW
    StoreOpcode(MSG_PARTY_ASSIGNMENT,              "MSG_PARTY_ASSIGNMENT",             STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandlePartyAssignmentOpcode);     /// 0x38E: @TODO need to check usage in vanilla WoW
    StoreOpcode(SMSG_OFFER_PETITION_ERROR,         "SMSG_OFFER_PETITION_ERROR",        STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);       /// 0x38F: @TODO need to check usage in vanilla WoW
    StoreOpcode(SMSG_RESET_FAILED_NOTIFY,          "SMSG_RESET_FAILED_NOTIFY",         STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);       /// 0x396: @TODO need to check usage in vanilla WoW
    StoreOpcode(SMSG_REAL_GROUP_UPDATE,            "SMSG_REAL_GROUP_UPDATE",           STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);       /// 0x397: @TODO need to check usage in vanilla WoW
    StoreOpcode(SMSG_INIT_EXTRA_AURA_INFO,         "SMSG_INIT_EXTRA_AURA_INFO",        STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);       /// 0x3A3: @TODO need to check usage in vanilla WoW
    StoreOpcode(SMSG_SET_EXTRA_AURA_INFO,          "SMSG_SET_EXTRA_AURA_INFO",         STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);       /// 0x3A4: @TODO need to check usage in vanilla WoW
    StoreOpcode(SMSG_SET_EXTRA_AURA_INFO_NEED_UPDATE,   "SMSG_SET_EXTRA_AURA_INFO_NEED_UPDATE",    STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);       /// 0x3A5: @TODO need to check usage in vanilla WoW
    StoreOpcode(SMSG_SPELL_CHANCE_PROC_LOG,        "SMSG_SPELL_CHANCE_PROC_LOG",       STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);       /// 0x3AA: @TODO need to check usage in vanilla WoW
    StoreOpcode(CMSG_MOVE_SET_RUN_SPEED,           "CMSG_MOVE_SET_RUN_SPEED",          STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);     /// 0x3AB: @TODO need to check usage in vanilla WoW
    StoreOpcode(SMSG_DISMOUNT,                     "SMSG_DISMOUNT",                    STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);       /// 0x3AC: @TODO need to check usage in vanilla WoW
    StoreOpcode(MSG_RAID_READY_CHECK_CONFIRM,      "MSG_RAID_READY_CHECK_CONFIRM",     STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);     /// 0x3AE: @TODO need to check usage in vanilla WoW
    StoreOpcode(SMSG_CLEAR_TARGET,                 "SMSG_CLEAR_TARGET",                STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);       /// 0x3BE: @TODO need to check usage in vanilla WoW
    StoreOpcode(CMSG_BOT_DETECTED,                 "CMSG_BOT_DETECTED",                STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);     /// 0x3BF: @TODO need to check usage in vanilla WoW
    StoreOpcode(SMSG_KICK_REASON,                  "SMSG_KICK_REASON",                 STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);       /// 0x3C4: @TODO need to check usage in vanilla WoW
    StoreOpcode(MSG_RAID_READY_CHECK_FINISHED,     "MSG_RAID_READY_CHECK_FINISHED",    STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleRaidReadyCheckFinishedOpcode);      /// 0x3C5: @TODO need to check usage in vanilla WoW
    StoreOpcode(CMSG_TARGET_CAST,                  "CMSG_TARGET_CAST",                 STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);     /// 0x3CF: @TODO need to check usage in vanilla WoW
    StoreOpcode(CMSG_TARGET_SCRIPT_CAST,           "CMSG_TARGET_SCRIPT_CAST",          STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);     /// 0x3D0: @TODO need to check usage in vanilla WoW
    StoreOpcode(CMSG_CHANNEL_DISPLAY_LIST,         "CMSG_CHANNEL_DISPLAY_LIST",        STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleChannelDisplayListQueryOpcode);     /// 0x3D1: @TODO need to check usage in vanilla WoW
    StoreOpcode(CMSG_GET_CHANNEL_MEMBER_COUNT,     "CMSG_GET_CHANNEL_MEMBER_COUNT",    STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleGetChannelMemberCountOpcode);       /// 0x3D3: @TODO need to check usage in vanilla WoW
    StoreOpcode(SMSG_CHANNEL_MEMBER_COUNT,         "SMSG_CHANNEL_MEMBER_COUNT",        STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);       /// 0x3D4: @TODO need to check usage in vanilla WoW
    StoreOpcode(CMSG_DEBUG_LIST_TARGETS,           "CMSG_DEBUG_LIST_TARGETS",          STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);     /// 0x3D7: @TODO need to check usage in vanilla WoW
    StoreOpcode(SMSG_DEBUG_LIST_TARGETS,           "SMSG_DEBUG_LIST_TARGETS",          STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);       /// 0x3D8: @TODO need to check usage in vanilla WoW
    StoreOpcode(CMSG_PARTY_SILENCE,                "CMSG_PARTY_SILENCE",               STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);     /// 0x3DC: @TODO need to check usage in vanilla WoW
    StoreOpcode(CMSG_PARTY_UNSILENCE,              "CMSG_PARTY_UNSILENCE",             STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);     /// 0x3DD: @TODO need to check usage in vanilla WoW
    StoreOpcode(MSG_NOTIFY_PARTY_SQUELCH,          "MSG_NOTIFY_PARTY_SQUELCH",         STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);     /// 0x3DE: @TODO need to check usage in vanilla WoW
    StoreOpcode(SMSG_COMSAT_RECONNECT_TRY,         "SMSG_COMSAT_RECONNECT_TRY",        STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);       /// 0x3DF: @TODO need to check usage in vanilla WoW
    StoreOpcode(SMSG_COMSAT_DISCONNECT,            "SMSG_COMSAT_DISCONNECT",           STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);       /// 0x3E0: @TODO need to check usage in vanilla WoW
    StoreOpcode(SMSG_COMSAT_CONNECT_FAIL,          "SMSG_COMSAT_CONNECT_FAIL",         STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);       /// 0x3E1: @TODO need to check usage in vanilla WoW
    StoreOpcode(CMSG_SET_CHANNEL_WATCH,            "CMSG_SET_CHANNEL_WATCH",           STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleSetChannelWatchOpcode);     /// 0x3EE: @TODO need to check usage in vanilla WoW
    StoreOpcode(SMSG_USERLIST_ADD,                 "SMSG_USERLIST_ADD",                STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);       /// 0x3EF: @TODO need to check usage in vanilla WoW
    StoreOpcode(SMSG_USERLIST_REMOVE,              "SMSG_USERLIST_REMOVE",             STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);       /// 0x3F0: @TODO need to check usage in vanilla WoW
    StoreOpcode(SMSG_USERLIST_UPDATE,              "SMSG_USERLIST_UPDATE",             STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);       /// 0x3F1: @TODO need to check usage in vanilla WoW
    StoreOpcode(CMSG_CLEAR_CHANNEL_WATCH,          "CMSG_CLEAR_CHANNEL_WATCH",         STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);     /// 0x3F2: @TODO need to check usage in vanilla WoW
    StoreOpcode(SMSG_GOGOGO_OBSOLETE,              "SMSG_GOGOGO_OBSOLETE",             STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);       /// 0x3F4: @TODO need to check usage in vanilla WoW
    StoreOpcode(SMSG_ECHO_PARTY_SQUELCH,           "SMSG_ECHO_PARTY_SQUELCH",          STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);       /// 0x3F5: @TODO need to check usage in vanilla WoW
    StoreOpcode(CMSG_SPELLCLICK,                   "CMSG_SPELLCLICK",                  STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);     /// 0x3F7: @TODO need to check usage in vanilla WoW
    StoreOpcode(SMSG_LOOT_LIST,                    "SMSG_LOOT_LIST",                   STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);       /// 0x3F8: @TODO need to check usage in vanilla WoW
    StoreOpcode(MSG_GUILD_EVENT_LOG_QUERY,         "MSG_GUILD_EVENT_LOG_QUERY",        STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleGuildEventLogQueryOpcode);      /// 0x3FE: @TODO need to check usage in vanilla WoW
    StoreOpcode(CMSG_MAELSTROM_RENAME_GUILD,       "CMSG_MAELSTROM_RENAME_GUILD",      STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);     /// 0x3FF: @TODO need to check usage in vanilla WoW
    StoreOpcode(CMSG_GET_MIRRORIMAGE_DATA,         "CMSG_GET_MIRRORIMAGE_DATA",        STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);     /// 0x400: @TODO need to check usage in vanilla WoW
    StoreOpcode(SMSG_MIRRORIMAGE_DATA,             "SMSG_MIRRORIMAGE_DATA",            STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);       /// 0x401: @TODO need to check usage in vanilla WoW
    StoreOpcode(SMSG_FORCE_DISPLAY_UPDATE,         "SMSG_FORCE_DISPLAY_UPDATE",        STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);       /// 0x402: @TODO need to check usage in vanilla WoW
    StoreOpcode(SMSG_SPELL_CHANCE_RESIST_PUSHBACK, "SMSG_SPELL_CHANCE_RESIST_PUSHBACK", STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);      /// 0x403: @TODO need to check usage in vanilla WoW
    StoreOpcode(CMSG_IGNORE_DIMINISHING_RETURNS_CHEAT,  "CMSG_IGNORE_DIMINISHING_RETURNS_CHEAT",   STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);     /// 0x404: @TODO need to check usage in vanilla WoW
    StoreOpcode(SMSG_IGNORE_DIMINISHING_RETURNS_CHEAT,  "SMSG_IGNORE_DIMINISHING_RETURNS_CHEAT",   STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);       /// 0x405: @TODO need to check usage in vanilla WoW
    StoreOpcode(CMSG_KEEP_ALIVE,                   "CMSG_KEEP_ALIVE",                  STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_EarlyProccess);        /// 0x406: @TODO need to check usage in vanilla WoW
    StoreOpcode(SMSG_RAID_READY_CHECK_ERROR,       "SMSG_RAID_READY_CHECK_ERROR",      STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);       /// 0x407: @TODO need to check usage in vanilla WoW
    StoreOpcode(CMSG_OPT_OUT_OF_LOOT,              "CMSG_OPT_OUT_OF_LOOT",             STATUS_AUTHED,    PROCESS_THREADUNSAFE, &WorldSession::HandleOptOutOfLootOpcode);        /// 0x408: @TODO need to check usage in vanilla WoW
    StoreOpcode(CMSG_SET_GRANTABLE_LEVELS,         "CMSG_SET_GRANTABLE_LEVELS",        STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);     /// 0x40B: @TODO need to check usage in vanilla WoW
    StoreOpcode(CMSG_GRANT_LEVEL,                  "CMSG_GRANT_LEVEL",                 STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);     /// 0x40C: @TODO need to check usage in vanilla WoW
    StoreOpcode(CMSG_DECLINE_CHANNEL_INVITE,       "CMSG_DECLINE_CHANNEL_INVITE",      STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);     /// 0x40F: @TODO need to check usage in vanilla WoW
    StoreOpcode(CMSG_GROUPACTION_THROTTLED,        "CMSG_GROUPACTION_THROTTLED",       STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);     /// 0x410: @TODO need to check usage in vanilla WoW
    StoreOpcode(SMSG_OVERRIDE_LIGHT,               "SMSG_OVERRIDE_LIGHT",              STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);       /// 0x411: @TODO need to check usage in vanilla WoW
    StoreOpcode(SMSG_TOTEM_CREATED,                "SMSG_TOTEM_CREATED",               STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);       /// 0x412: @TODO need to check usage in vanilla WoW
    StoreOpcode(CMSG_TOTEM_DESTROYED,              "CMSG_TOTEM_DESTROYED",             STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleTotemDestroyed);        /// 0x413: @TODO need to check usage in vanilla WoW
    StoreOpcode(CMSG_EXPIRE_RAID_INSTANCE,         "CMSG_EXPIRE_RAID_INSTANCE",        STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);     /// 0x414: @TODO need to check usage in vanilla WoW
    StoreOpcode(CMSG_NO_SPELL_VARIANCE,            "CMSG_NO_SPELL_VARIANCE",           STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);     /// 0x415: @TODO need to check usage in vanilla WoW
    StoreOpcode(CMSG_QUESTGIVER_STATUS_MULTIPLE_QUERY,  "CMSG_QUESTGIVER_STATUS_MULTIPLE_QUERY",   STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleQuestgiverStatusMultipleQuery);     /// 0x416: @TODO need to check usage in vanilla WoW
    StoreOpcode(SMSG_QUESTGIVER_STATUS_MULTIPLE,   "SMSG_QUESTGIVER_STATUS_MULTIPLE",  STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);       /// 0x417: @TODO need to check usage in vanilla WoW
    StoreOpcode(CMSG_QUERY_SERVER_BUCK_DATA,       "CMSG_QUERY_SERVER_BUCK_DATA",      STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);     /// 0x41A: @TODO need to check usage in vanilla WoW
    StoreOpcode(CMSG_CLEAR_SERVER_BUCK_DATA,       "CMSG_CLEAR_SERVER_BUCK_DATA",      STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);     /// 0x41B: @TODO need to check usage in vanilla WoW
    StoreOpcode(SMSG_SERVER_BUCK_DATA,             "SMSG_SERVER_BUCK_DATA",            STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);       /// 0x41C: @TODO need to check usage in vanilla WoW
    StoreOpcode(SMSG_SEND_UNLEARN_SPELLS,          "SMSG_SEND_UNLEARN_SPELLS",         STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);       /// 0x41D: @TODO need to check usage in vanilla WoW
    StoreOpcode(SMSG_PROPOSE_LEVEL_GRANT,          "SMSG_PROPOSE_LEVEL_GRANT",         STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);       /// 0x41E: @TODO need to check usage in vanilla WoW
    StoreOpcode(CMSG_ACCEPT_LEVEL_GRANT,           "CMSG_ACCEPT_LEVEL_GRANT",          STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_NULL);     /// 0x41F: @TODO need to check usage in vanilla WoW
    StoreOpcode(SMSG_REFER_A_FRIEND_FAILURE,       "SMSG_REFER_A_FRIEND_FAILURE",      STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);       /// 0x420: @TODO need to check usage in vanilla WoW
    StoreOpcode(SMSG_SUMMON_CANCEL,                "SMSG_SUMMON_CANCEL",               STATUS_NEVER,     PROCESS_INPLACE,      &WorldSession::Handle_ServerSide);       /// 0x423: @TODO need to check usage in vanilla WoW

    return;
}
