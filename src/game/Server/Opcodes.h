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

/// \addtogroup u2w
/// @{
/// \file

#ifndef MANGOS_H_OPCODES
#define MANGOS_H_OPCODES

#include "Common.h"
#include "Policies/Singleton.h"

// Note: this include need for be sure have full definition of class WorldSession
//       if this class definition not complite then VS for x64 release use different size for
//       struct OpcodeHandler in this header and Opcode.cpp and get totally wrong data from
//       table opcodeTable in source when Opcode.h included but WorldSession.h not included
#include "WorldSession.h"

/**
 * This is a list of Opcodes that are known for the client/server communication, it is used
 * to tell the server to do something or the client to do something. Every opcode is handled
 * in some way, and you can find what functions handle what opcode in the implementation of
 * \ref Opcodes::BuildOpcodeList
 *
 * To send messages the following functions can be used: \ref WorldObject::SendMessageToSet,
 * \ref WorldObject::SendMessageToSetExcept, \ref WorldObject::SendMessageToSetInRange
 *
 * \see WorldPacket
 * \todo Replace the Pack GUID part with a packed GUID, ie: it's shorter than usual?
 */
enum OpcodesList
{
    MSG_NULL_ACTION                                 = 0x000,
    CMSG_BOOTME                                     = 0x001,
    CMSG_DBLOOKUP                                   = 0x002,
    SMSG_DBLOOKUP                                   = 0x003,
    CMSG_QUERY_OBJECT_POSITION                      = 0x004,
    SMSG_QUERY_OBJECT_POSITION                      = 0x005,
    CMSG_QUERY_OBJECT_ROTATION                      = 0x006,
    SMSG_QUERY_OBJECT_ROTATION                      = 0x007,
    CMSG_WORLD_TELEPORT                             = 0x008,
    CMSG_TELEPORT_TO_UNIT                           = 0x009,
    CMSG_ZONE_MAP                                   = 0x00A,
    SMSG_ZONE_MAP                                   = 0x00B,
    CMSG_DEBUG_CHANGECELLZONE                       = 0x00C,
    CMSG_EMBLAZON_TABARD_OBSOLETE                   = 0x00D,
    CMSG_UNEMBLAZON_TABARD_OBSOLETE                 = 0x00E,
    CMSG_RECHARGE                                   = 0x00F,
    CMSG_LEARN_SPELL                                = 0x010,
    CMSG_CREATEMONSTER                              = 0x011,
    CMSG_DESTROYMONSTER                             = 0x012,
    CMSG_CREATEITEM                                 = 0x013,
    CMSG_CREATEGAMEOBJECT                           = 0x014,
    SMSG_CHECK_FOR_BOTS                             = 0x015,
    CMSG_MAKEMONSTERATTACKGUID                      = 0x016,
    CMSG_BOT_DETECTED2                              = 0x017,
    CMSG_FORCEACTION                                = 0x018,
    CMSG_FORCEACTIONONOTHER                         = 0x019,
    CMSG_FORCEACTIONSHOW                            = 0x01A,
    SMSG_FORCEACTIONSHOW                            = 0x01B,
    CMSG_PETGODMODE                                 = 0x01C,
    SMSG_PETGODMODE                                 = 0x01D,
    SMSG_DEBUGINFOSPELLMISS_OBSOLETE                = 0x01E,
    CMSG_WEATHER_SPEED_CHEAT                        = 0x01F,
    CMSG_UNDRESSPLAYER                              = 0x020,
    CMSG_BEASTMASTER                                = 0x021,
    CMSG_GODMODE                                    = 0x022,
    SMSG_GODMODE                                    = 0x023,
    CMSG_CHEAT_SETMONEY                             = 0x024,
    CMSG_LEVEL_CHEAT                                = 0x025,
    CMSG_PET_LEVEL_CHEAT                            = 0x026,
    CMSG_SET_WORLDSTATE                             = 0x027,
    CMSG_COOLDOWN_CHEAT                             = 0x028,
    CMSG_USE_SKILL_CHEAT                            = 0x029,
    CMSG_FLAG_QUEST                                 = 0x02A,
    CMSG_FLAG_QUEST_FINISH                          = 0x02B,
    CMSG_CLEAR_QUEST                                = 0x02C,
    CMSG_SEND_EVENT                                 = 0x02D,
    CMSG_DEBUG_AISTATE                              = 0x02E,
    SMSG_DEBUG_AISTATE                              = 0x02F,
    CMSG_DISABLE_PVP_CHEAT                          = 0x030,
    CMSG_ADVANCE_SPAWN_TIME                         = 0x031,
    CMSG_PVP_PORT_OBSOLETE                          = 0x032,
    CMSG_AUTH_SRP6_BEGIN                            = 0x033,
    CMSG_AUTH_SRP6_PROOF                            = 0x034,
    CMSG_AUTH_SRP6_RECODE                           = 0x035,
    CMSG_CHAR_CREATE                                = 0x036,
    CMSG_CHAR_ENUM                                  = 0x037,
    CMSG_CHAR_DELETE                                = 0x038,
    SMSG_AUTH_SRP6_RESPONSE                         = 0x039,
    SMSG_CHAR_CREATE                                = 0x03A,
    SMSG_CHAR_ENUM                                  = 0x03B,
    SMSG_CHAR_DELETE                                = 0x03C,
    CMSG_PLAYER_LOGIN                               = 0x03D,
    SMSG_NEW_WORLD                                  = 0x03E,
    SMSG_TRANSFER_PENDING                           = 0x03F,
    SMSG_TRANSFER_ABORTED                           = 0x040,
    SMSG_CHARACTER_LOGIN_FAILED                     = 0x041,
    SMSG_LOGIN_SETTIMESPEED                         = 0x042,
    SMSG_GAMETIME_UPDATE                            = 0x043,
    CMSG_GAMETIME_SET                               = 0x044,
    SMSG_GAMETIME_SET                               = 0x045,
    CMSG_GAMESPEED_SET                              = 0x046,
    SMSG_GAMESPEED_SET                              = 0x047,
    CMSG_SERVERTIME                                 = 0x048,
    SMSG_SERVERTIME                                 = 0x049,
    CMSG_PLAYER_LOGOUT                              = 0x04A,
    CMSG_LOGOUT_REQUEST                             = 0x04B,
    SMSG_LOGOUT_RESPONSE                            = 0x04C,
    SMSG_LOGOUT_COMPLETE                            = 0x04D,
    CMSG_LOGOUT_CANCEL                              = 0x04E,
    SMSG_LOGOUT_CANCEL_ACK                          = 0x04F,
    CMSG_NAME_QUERY                                 = 0x050,
    SMSG_NAME_QUERY_RESPONSE                        = 0x051,
    CMSG_PET_NAME_QUERY                             = 0x052,
    SMSG_PET_NAME_QUERY_RESPONSE                    = 0x053,
    CMSG_GUILD_QUERY                                = 0x054,
    SMSG_GUILD_QUERY_RESPONSE                       = 0x055,
    CMSG_ITEM_QUERY_SINGLE                          = 0x056,
    CMSG_ITEM_QUERY_MULTIPLE                        = 0x057,
    SMSG_ITEM_QUERY_SINGLE_RESPONSE                 = 0x058,
    SMSG_ITEM_QUERY_MULTIPLE_RESPONSE               = 0x059,
    CMSG_PAGE_TEXT_QUERY                            = 0x05A,
    SMSG_PAGE_TEXT_QUERY_RESPONSE                   = 0x05B,
    CMSG_QUEST_QUERY                                = 0x05C,
    SMSG_QUEST_QUERY_RESPONSE                       = 0x05D,
    CMSG_GAMEOBJECT_QUERY                           = 0x05E,
    SMSG_GAMEOBJECT_QUERY_RESPONSE                  = 0x05F,
    CMSG_CREATURE_QUERY                             = 0x060,
    SMSG_CREATURE_QUERY_RESPONSE                    = 0x061,
    CMSG_WHO                                        = 0x062,
    SMSG_WHO                                        = 0x063,
    CMSG_WHOIS                                      = 0x064,
    SMSG_WHOIS                                      = 0x065,
    CMSG_FRIEND_LIST                                = 0x066,
    SMSG_FRIEND_LIST                                = 0x067,
    SMSG_FRIEND_STATUS                              = 0x068,
    CMSG_ADD_FRIEND                                 = 0x069,
    CMSG_DEL_FRIEND                                 = 0x06A,
    SMSG_IGNORE_LIST                                = 0x06B,
    CMSG_ADD_IGNORE                                 = 0x06C,
    CMSG_DEL_IGNORE                                 = 0x06D,
    CMSG_GROUP_INVITE                               = 0x06E,
    SMSG_GROUP_INVITE                               = 0x06F,
    CMSG_GROUP_CANCEL                               = 0x070,
    SMSG_GROUP_CANCEL                               = 0x071,
    CMSG_GROUP_ACCEPT                               = 0x072,
    CMSG_GROUP_DECLINE                              = 0x073,
    SMSG_GROUP_DECLINE                              = 0x074,
    CMSG_GROUP_UNINVITE                             = 0x075,
    CMSG_GROUP_UNINVITE_GUID                        = 0x076,
    SMSG_GROUP_UNINVITE                             = 0x077,
    CMSG_GROUP_SET_LEADER                           = 0x078,
    SMSG_GROUP_SET_LEADER                           = 0x079,
    CMSG_LOOT_METHOD                                = 0x07A,
    CMSG_GROUP_DISBAND                              = 0x07B,
    SMSG_GROUP_DESTROYED                            = 0x07C,
    SMSG_GROUP_LIST                                 = 0x07D,
    SMSG_PARTY_MEMBER_STATS                         = 0x07E,
    SMSG_PARTY_COMMAND_RESULT                       = 0x07F,
    UMSG_UPDATE_GROUP_MEMBERS                       = 0x080,
    CMSG_GUILD_CREATE                               = 0x081,
    CMSG_GUILD_INVITE                               = 0x082,
    SMSG_GUILD_INVITE                               = 0x083,
    CMSG_GUILD_ACCEPT                               = 0x084,
    CMSG_GUILD_DECLINE                              = 0x085,
    SMSG_GUILD_DECLINE                              = 0x086,
    CMSG_GUILD_INFO                                 = 0x087,
    SMSG_GUILD_INFO                                 = 0x088,
    CMSG_GUILD_ROSTER                               = 0x089,
    SMSG_GUILD_ROSTER                               = 0x08A,
    CMSG_GUILD_PROMOTE                              = 0x08B,
    CMSG_GUILD_DEMOTE                               = 0x08C,
    CMSG_GUILD_LEAVE                                = 0x08D,
    CMSG_GUILD_REMOVE                               = 0x08E,
    CMSG_GUILD_DISBAND                              = 0x08F,
    CMSG_GUILD_LEADER                               = 0x090,
    CMSG_GUILD_MOTD                                 = 0x091,
    SMSG_GUILD_EVENT                                = 0x092,
    SMSG_GUILD_COMMAND_RESULT                       = 0x093,
    UMSG_UPDATE_GUILD                               = 0x094,
    CMSG_MESSAGECHAT                                = 0x095,
    SMSG_MESSAGECHAT                                = 0x096,
    CMSG_JOIN_CHANNEL                               = 0x097,
    CMSG_LEAVE_CHANNEL                              = 0x098,
    SMSG_CHANNEL_NOTIFY                             = 0x099,
    CMSG_CHANNEL_LIST                               = 0x09A,
    SMSG_CHANNEL_LIST                               = 0x09B,
    CMSG_CHANNEL_PASSWORD                           = 0x09C,
    CMSG_CHANNEL_SET_OWNER                          = 0x09D,
    CMSG_CHANNEL_OWNER                              = 0x09E,
    CMSG_CHANNEL_MODERATOR                          = 0x09F,
    CMSG_CHANNEL_UNMODERATOR                        = 0x0A0,
    CMSG_CHANNEL_MUTE                               = 0x0A1,
    CMSG_CHANNEL_UNMUTE                             = 0x0A2,
    CMSG_CHANNEL_INVITE                             = 0x0A3,
    CMSG_CHANNEL_KICK                               = 0x0A4,
    CMSG_CHANNEL_BAN                                = 0x0A5,
    CMSG_CHANNEL_UNBAN                              = 0x0A6,
    CMSG_CHANNEL_ANNOUNCEMENTS                      = 0x0A7,
    CMSG_CHANNEL_MODERATE                           = 0x0A8,
    SMSG_UPDATE_OBJECT                              = 0x0A9,
    SMSG_DESTROY_OBJECT                             = 0x0AA,
    CMSG_USE_ITEM                                   = 0x0AB,
    CMSG_OPEN_ITEM                                  = 0x0AC,
    CMSG_READ_ITEM                                  = 0x0AD,
    SMSG_READ_ITEM_OK                               = 0x0AE,
    SMSG_READ_ITEM_FAILED                           = 0x0AF,
    SMSG_ITEM_COOLDOWN                              = 0x0B0,
    CMSG_GAMEOBJ_USE                                = 0x0B1,
    CMSG_GAMEOBJ_CHAIR_USE_OBSOLETE                 = 0x0B2,
    SMSG_GAMEOBJECT_CUSTOM_ANIM                     = 0x0B3,
    CMSG_AREATRIGGER                                = 0x0B4,
    MSG_MOVE_START_FORWARD                          = 0x0B5,
    MSG_MOVE_START_BACKWARD                         = 0x0B6,
    MSG_MOVE_STOP                                   = 0x0B7,
    MSG_MOVE_START_STRAFE_LEFT                      = 0x0B8,
    MSG_MOVE_START_STRAFE_RIGHT                     = 0x0B9,
    MSG_MOVE_STOP_STRAFE                            = 0x0BA,
    MSG_MOVE_JUMP                                   = 0x0BB,
    MSG_MOVE_START_TURN_LEFT                        = 0x0BC,
    MSG_MOVE_START_TURN_RIGHT                       = 0x0BD,
    MSG_MOVE_STOP_TURN                              = 0x0BE,
    MSG_MOVE_START_PITCH_UP                         = 0x0BF,
    MSG_MOVE_START_PITCH_DOWN                       = 0x0C0,
    MSG_MOVE_STOP_PITCH                             = 0x0C1,
    MSG_MOVE_SET_RUN_MODE                           = 0x0C2,
    MSG_MOVE_SET_WALK_MODE                          = 0x0C3,
    MSG_MOVE_TOGGLE_LOGGING                         = 0x0C4,
    MSG_MOVE_TELEPORT                               = 0x0C5,
    MSG_MOVE_TELEPORT_CHEAT                         = 0x0C6,
    MSG_MOVE_TELEPORT_ACK                           = 0x0C7,
    MSG_MOVE_TOGGLE_FALL_LOGGING                    = 0x0C8,
    MSG_MOVE_FALL_LAND                              = 0x0C9,
    MSG_MOVE_START_SWIM                             = 0x0CA,
    MSG_MOVE_STOP_SWIM                              = 0x0CB,
    MSG_MOVE_SET_RUN_SPEED_CHEAT                    = 0x0CC,
    MSG_MOVE_SET_RUN_SPEED                          = 0x0CD,
    MSG_MOVE_SET_RUN_BACK_SPEED_CHEAT               = 0x0CE,
    MSG_MOVE_SET_RUN_BACK_SPEED                     = 0x0CF,
    MSG_MOVE_SET_WALK_SPEED_CHEAT                   = 0x0D0,
    MSG_MOVE_SET_WALK_SPEED                         = 0x0D1,
    MSG_MOVE_SET_SWIM_SPEED_CHEAT                   = 0x0D2,
    MSG_MOVE_SET_SWIM_SPEED                         = 0x0D3,
    MSG_MOVE_SET_SWIM_BACK_SPEED_CHEAT              = 0x0D4,
    MSG_MOVE_SET_SWIM_BACK_SPEED                    = 0x0D5,
    MSG_MOVE_SET_ALL_SPEED_CHEAT                    = 0x0D6,
    MSG_MOVE_SET_TURN_RATE_CHEAT                    = 0x0D7,
    MSG_MOVE_SET_TURN_RATE                          = 0x0D8,
    MSG_MOVE_TOGGLE_COLLISION_CHEAT                 = 0x0D9,
    MSG_MOVE_SET_FACING                             = 0x0DA,
    MSG_MOVE_SET_PITCH                              = 0x0DB,
    MSG_MOVE_WORLDPORT_ACK                          = 0x0DC,
    SMSG_MONSTER_MOVE                               = 0x0DD,
    SMSG_MOVE_WATER_WALK                            = 0x0DE,
    SMSG_MOVE_LAND_WALK                             = 0x0DF,
    MSG_MOVE_SET_RAW_POSITION_ACK                   = 0x0E0,
    CMSG_MOVE_SET_RAW_POSITION                      = 0x0E1,
    SMSG_FORCE_RUN_SPEED_CHANGE                     = 0x0E2,
    CMSG_FORCE_RUN_SPEED_CHANGE_ACK                 = 0x0E3,
    SMSG_FORCE_RUN_BACK_SPEED_CHANGE                = 0x0E4,
    CMSG_FORCE_RUN_BACK_SPEED_CHANGE_ACK            = 0x0E5,
    SMSG_FORCE_SWIM_SPEED_CHANGE                    = 0x0E6,
    CMSG_FORCE_SWIM_SPEED_CHANGE_ACK                = 0x0E7,
    SMSG_FORCE_MOVE_ROOT                            = 0x0E8,
    CMSG_FORCE_MOVE_ROOT_ACK                        = 0x0E9,
    SMSG_FORCE_MOVE_UNROOT                          = 0x0EA,
    CMSG_FORCE_MOVE_UNROOT_ACK                      = 0x0EB,
    MSG_MOVE_ROOT                                   = 0x0EC,
    MSG_MOVE_UNROOT                                 = 0x0ED,
    MSG_MOVE_HEARTBEAT                              = 0x0EE,
    SMSG_MOVE_KNOCK_BACK                            = 0x0EF,
    CMSG_MOVE_KNOCK_BACK_ACK                        = 0x0F0,
    MSG_MOVE_KNOCK_BACK                             = 0x0F1,
    SMSG_MOVE_FEATHER_FALL                          = 0x0F2,
    SMSG_MOVE_NORMAL_FALL                           = 0x0F3,
    SMSG_MOVE_SET_HOVER                             = 0x0F4,
    SMSG_MOVE_UNSET_HOVER                           = 0x0F5,
    CMSG_MOVE_HOVER_ACK                             = 0x0F6,
    MSG_MOVE_HOVER                                  = 0x0F7,
    CMSG_TRIGGER_CINEMATIC_CHEAT                    = 0x0F8,
    CMSG_OPENING_CINEMATIC                          = 0x0F9,
    SMSG_TRIGGER_CINEMATIC                          = 0x0FA,
    CMSG_NEXT_CINEMATIC_CAMERA                      = 0x0FB,
    CMSG_COMPLETE_CINEMATIC                         = 0x0FC,
    SMSG_TUTORIAL_FLAGS                             = 0x0FD,
    CMSG_TUTORIAL_FLAG                              = 0x0FE,
    CMSG_TUTORIAL_CLEAR                             = 0x0FF,
    CMSG_TUTORIAL_RESET                             = 0x100,
    CMSG_STANDSTATECHANGE                           = 0x101,
    CMSG_EMOTE                                      = 0x102,
    SMSG_EMOTE                                      = 0x103,
    CMSG_TEXT_EMOTE                                 = 0x104,
    SMSG_TEXT_EMOTE                                 = 0x105,
    CMSG_AUTOEQUIP_GROUND_ITEM                      = 0x106,
    CMSG_AUTOSTORE_GROUND_ITEM                      = 0x107,
    CMSG_AUTOSTORE_LOOT_ITEM                        = 0x108,
    CMSG_STORE_LOOT_IN_SLOT                         = 0x109,
    CMSG_AUTOEQUIP_ITEM                             = 0x10A,
    CMSG_AUTOSTORE_BAG_ITEM                         = 0x10B,
    CMSG_SWAP_ITEM                                  = 0x10C,
    CMSG_SWAP_INV_ITEM                              = 0x10D,
    CMSG_SPLIT_ITEM                                 = 0x10E,
    CMSG_AUTOEQUIP_ITEM_SLOT                        = 0x10F,
    OBSOLETE_DROP_ITEM                              = 0x110,
    CMSG_DESTROYITEM                                = 0x111,
    SMSG_INVENTORY_CHANGE_FAILURE                   = 0x112,
    SMSG_OPEN_CONTAINER                             = 0x113,
    CMSG_INSPECT                                    = 0x114,
    SMSG_INSPECT                                    = 0x115,
    CMSG_INITIATE_TRADE                             = 0x116,
    CMSG_BEGIN_TRADE                                = 0x117,
    CMSG_BUSY_TRADE                                 = 0x118,
    CMSG_IGNORE_TRADE                               = 0x119,
    CMSG_ACCEPT_TRADE                               = 0x11A,
    CMSG_UNACCEPT_TRADE                             = 0x11B,
    CMSG_CANCEL_TRADE                               = 0x11C,
    CMSG_SET_TRADE_ITEM                             = 0x11D,
    CMSG_CLEAR_TRADE_ITEM                           = 0x11E,
    CMSG_SET_TRADE_GOLD                             = 0x11F,
    SMSG_TRADE_STATUS                               = 0x120,
    SMSG_TRADE_STATUS_EXTENDED                      = 0x121,
    SMSG_INITIALIZE_FACTIONS                        = 0x122,
    SMSG_SET_FACTION_VISIBLE                        = 0x123,
    SMSG_SET_FACTION_STANDING                       = 0x124,
    CMSG_SET_FACTION_ATWAR                          = 0x125,
    CMSG_SET_FACTION_CHEAT                          = 0x126,
    SMSG_SET_PROFICIENCY                            = 0x127,
    CMSG_SET_ACTION_BUTTON                          = 0x128,
    SMSG_ACTION_BUTTONS                             = 0x129,
    SMSG_INITIAL_SPELLS                             = 0x12A,
    SMSG_LEARNED_SPELL                              = 0x12B,
    SMSG_SUPERCEDED_SPELL                           = 0x12C,
    CMSG_NEW_SPELL_SLOT                             = 0x12D,
    CMSG_CAST_SPELL                                 = 0x12E,
    CMSG_CANCEL_CAST                                = 0x12F,
    SMSG_CAST_FAILED                                = 0x130,
    SMSG_SPELL_START                                = 0x131,
    SMSG_SPELL_GO                                   = 0x132,
    SMSG_SPELL_FAILURE                              = 0x133,
    SMSG_SPELL_COOLDOWN                             = 0x134,
    SMSG_COOLDOWN_EVENT                             = 0x135,
    CMSG_CANCEL_AURA                                = 0x136,
    SMSG_UPDATE_AURA_DURATION                       = 0x137,
    SMSG_PET_CAST_FAILED                            = 0x138,
    MSG_CHANNEL_START                               = 0x139,
    MSG_CHANNEL_UPDATE                              = 0x13A,
    CMSG_CANCEL_CHANNELLING                         = 0x13B,
    SMSG_AI_REACTION                                = 0x13C,
    CMSG_SET_SELECTION                              = 0x13D,
    CMSG_SET_TARGET_OBSOLETE                        = 0x13E,
    CMSG_UNUSED                                     = 0x13F,
    CMSG_UNUSED2                                    = 0x140,
    CMSG_ATTACKSWING                                = 0x141,
    CMSG_ATTACKSTOP                                 = 0x142,
    SMSG_ATTACKSTART                                = 0x143,
    SMSG_ATTACKSTOP                                 = 0x144,
    SMSG_ATTACKSWING_NOTINRANGE                     = 0x145,
    SMSG_ATTACKSWING_BADFACING                      = 0x146,
    SMSG_ATTACKSWING_NOTSTANDING                    = 0x147,
    SMSG_ATTACKSWING_DEADTARGET                     = 0x148,
    SMSG_ATTACKSWING_CANT_ATTACK                    = 0x149,
    SMSG_ATTACKERSTATEUPDATE                        = 0x14A,
    SMSG_VICTIMSTATEUPDATE_OBSOLETE                 = 0x14B,
    SMSG_DAMAGE_DONE_OBSOLETE                       = 0x14C,
    SMSG_DAMAGE_TAKEN_OBSOLETE                      = 0x14D,
    SMSG_CANCEL_COMBAT                              = 0x14E,
    SMSG_PLAYER_COMBAT_XP_GAIN_OBSOLETE             = 0x14F,
    SMSG_SPELLHEALLOG                               = 0x150,
    SMSG_SPELLENERGIZELOG                           = 0x151,
    CMSG_SHEATHE_OBSOLETE                           = 0x152,
    CMSG_SAVE_PLAYER                                = 0x153,
    CMSG_SETDEATHBINDPOINT                          = 0x154,
    SMSG_BINDPOINTUPDATE                            = 0x155,
    CMSG_GETDEATHBINDZONE                           = 0x156,
    SMSG_BINDZONEREPLY                              = 0x157,
    SMSG_PLAYERBOUND                                = 0x158,
    SMSG_CLIENT_CONTROL_UPDATE                      = 0x159,
    CMSG_REPOP_REQUEST                              = 0x15A,
    SMSG_RESURRECT_REQUEST                          = 0x15B,
    CMSG_RESURRECT_RESPONSE                         = 0x15C,
    CMSG_LOOT                                       = 0x15D,
    CMSG_LOOT_MONEY                                 = 0x15E,
    CMSG_LOOT_RELEASE                               = 0x15F,
    SMSG_LOOT_RESPONSE                              = 0x160,
    SMSG_LOOT_RELEASE_RESPONSE                      = 0x161,
    SMSG_LOOT_REMOVED                               = 0x162,
    SMSG_LOOT_MONEY_NOTIFY                          = 0x163,
    SMSG_LOOT_ITEM_NOTIFY                           = 0x164,
    SMSG_LOOT_CLEAR_MONEY                           = 0x165,
    SMSG_ITEM_PUSH_RESULT                           = 0x166,
    SMSG_DUEL_REQUESTED                             = 0x167,
    SMSG_DUEL_OUTOFBOUNDS                           = 0x168,
    SMSG_DUEL_INBOUNDS                              = 0x169,
    SMSG_DUEL_COMPLETE                              = 0x16A,
    SMSG_DUEL_WINNER                                = 0x16B,
    CMSG_DUEL_ACCEPTED                              = 0x16C,
    CMSG_DUEL_CANCELLED                             = 0x16D,
    SMSG_MOUNTRESULT                                = 0x16E,
    SMSG_DISMOUNTRESULT                             = 0x16F,
    SMSG_PUREMOUNT_CANCELLED_OBSOLETE               = 0x170,
    CMSG_MOUNTSPECIAL_ANIM                          = 0x171,
    SMSG_MOUNTSPECIAL_ANIM                          = 0x172,
    SMSG_PET_TAME_FAILURE                           = 0x173,
    CMSG_PET_SET_ACTION                             = 0x174,
    CMSG_PET_ACTION                                 = 0x175,
    CMSG_PET_ABANDON                                = 0x176,
    CMSG_PET_RENAME                                 = 0x177,
    SMSG_PET_NAME_INVALID                           = 0x178,
    SMSG_PET_SPELLS                                 = 0x179,
    SMSG_PET_MODE                                   = 0x17A,
    CMSG_GOSSIP_HELLO                               = 0x17B,
    CMSG_GOSSIP_SELECT_OPTION                       = 0x17C,
    SMSG_GOSSIP_MESSAGE                             = 0x17D,
    SMSG_GOSSIP_COMPLETE                            = 0x17E,
    CMSG_NPC_TEXT_QUERY                             = 0x17F,
    SMSG_NPC_TEXT_UPDATE                            = 0x180,
    SMSG_NPC_WONT_TALK                              = 0x181,
    CMSG_QUESTGIVER_STATUS_QUERY                    = 0x182,
    SMSG_QUESTGIVER_STATUS                          = 0x183,
    CMSG_QUESTGIVER_HELLO                           = 0x184,
    SMSG_QUESTGIVER_QUEST_LIST                      = 0x185,
    CMSG_QUESTGIVER_QUERY_QUEST                     = 0x186,
    CMSG_QUESTGIVER_QUEST_AUTOLAUNCH                = 0x187,
    SMSG_QUESTGIVER_QUEST_DETAILS                   = 0x188,
    CMSG_QUESTGIVER_ACCEPT_QUEST                    = 0x189,
    CMSG_QUESTGIVER_COMPLETE_QUEST                  = 0x18A,
    SMSG_QUESTGIVER_REQUEST_ITEMS                   = 0x18B,
    CMSG_QUESTGIVER_REQUEST_REWARD                  = 0x18C,
    SMSG_QUESTGIVER_OFFER_REWARD                    = 0x18D,
    CMSG_QUESTGIVER_CHOOSE_REWARD                   = 0x18E,
    SMSG_QUESTGIVER_QUEST_INVALID                   = 0x18F,
    CMSG_QUESTGIVER_CANCEL                          = 0x190,
    SMSG_QUESTGIVER_QUEST_COMPLETE                  = 0x191,
    SMSG_QUESTGIVER_QUEST_FAILED                    = 0x192,
    CMSG_QUESTLOG_SWAP_QUEST                        = 0x193,
    CMSG_QUESTLOG_REMOVE_QUEST                      = 0x194,
    SMSG_QUESTLOG_FULL                              = 0x195,
    SMSG_QUESTUPDATE_FAILED                         = 0x196,
    SMSG_QUESTUPDATE_FAILEDTIMER                    = 0x197,
    SMSG_QUESTUPDATE_COMPLETE                       = 0x198,
    SMSG_QUESTUPDATE_ADD_KILL                       = 0x199,
    SMSG_QUESTUPDATE_ADD_ITEM                       = 0x19A,
    CMSG_QUEST_CONFIRM_ACCEPT                       = 0x19B,
    SMSG_QUEST_CONFIRM_ACCEPT                       = 0x19C,
    CMSG_PUSHQUESTTOPARTY                           = 0x19D,
    CMSG_LIST_INVENTORY                             = 0x19E,
    SMSG_LIST_INVENTORY                             = 0x19F,
    CMSG_SELL_ITEM                                  = 0x1A0,
    SMSG_SELL_ITEM                                  = 0x1A1,
    CMSG_BUY_ITEM                                   = 0x1A2,
    CMSG_BUY_ITEM_IN_SLOT                           = 0x1A3,
    SMSG_BUY_ITEM                                   = 0x1A4,
    SMSG_BUY_FAILED                                 = 0x1A5,
    CMSG_TAXICLEARALLNODES                          = 0x1A6,
    CMSG_TAXIENABLEALLNODES                         = 0x1A7,
    CMSG_TAXISHOWNODES                              = 0x1A8,
    SMSG_SHOWTAXINODES                              = 0x1A9,
    CMSG_TAXINODE_STATUS_QUERY                      = 0x1AA,
    SMSG_TAXINODE_STATUS                            = 0x1AB,
    CMSG_TAXIQUERYAVAILABLENODES                    = 0x1AC,
    CMSG_ACTIVATETAXI                               = 0x1AD,
    SMSG_ACTIVATETAXIREPLY                          = 0x1AE,
    SMSG_NEW_TAXI_PATH                              = 0x1AF,
    CMSG_TRAINER_LIST                               = 0x1B0,
    SMSG_TRAINER_LIST                               = 0x1B1,
    CMSG_TRAINER_BUY_SPELL                          = 0x1B2,
    SMSG_TRAINER_BUY_SUCCEEDED                      = 0x1B3,
    SMSG_TRAINER_BUY_FAILED                         = 0x1B4,// uint64, uint32, uint32 (0...2)
    CMSG_BINDER_ACTIVATE                            = 0x1B5,
    SMSG_PLAYERBINDERROR                            = 0x1B6,
    CMSG_BANKER_ACTIVATE                            = 0x1B7,
    SMSG_SHOW_BANK                                  = 0x1B8,
    CMSG_BUY_BANK_SLOT                              = 0x1B9,
    SMSG_BUY_BANK_SLOT_RESULT                       = 0x1BA,
    CMSG_PETITION_SHOWLIST                          = 0x1BB,
    SMSG_PETITION_SHOWLIST                          = 0x1BC,
    CMSG_PETITION_BUY                               = 0x1BD,
    CMSG_PETITION_SHOW_SIGNATURES                   = 0x1BE,
    SMSG_PETITION_SHOW_SIGNATURES                   = 0x1BF,
    CMSG_PETITION_SIGN                              = 0x1C0,
    SMSG_PETITION_SIGN_RESULTS                      = 0x1C1,
    MSG_PETITION_DECLINE                            = 0x1C2,
    CMSG_OFFER_PETITION                             = 0x1C3,
    CMSG_TURN_IN_PETITION                           = 0x1C4,
    SMSG_TURN_IN_PETITION_RESULTS                   = 0x1C5,
    CMSG_PETITION_QUERY                             = 0x1C6,
    SMSG_PETITION_QUERY_RESPONSE                    = 0x1C7,
    SMSG_FISH_NOT_HOOKED                            = 0x1C8,
    SMSG_FISH_ESCAPED                               = 0x1C9,
    CMSG_BUG                                        = 0x1CA,
    SMSG_NOTIFICATION                               = 0x1CB,
    CMSG_PLAYED_TIME                                = 0x1CC,
    SMSG_PLAYED_TIME                                = 0x1CD,
    CMSG_QUERY_TIME                                 = 0x1CE,
    SMSG_QUERY_TIME_RESPONSE                        = 0x1CF,
    SMSG_LOG_XPGAIN                                 = 0x1D0,
    SMSG_AURACASTLOG                                = 0x1D1,
    CMSG_RECLAIM_CORPSE                             = 0x1D2,
    CMSG_WRAP_ITEM                                  = 0x1D3,
    SMSG_LEVELUP_INFO                               = 0x1D4,
    MSG_MINIMAP_PING                                = 0x1D5,
    SMSG_RESISTLOG                                  = 0x1D6,// GUID, GUID, int32, float, float, int32, int32
    SMSG_ENCHANTMENTLOG                             = 0x1D7,
    CMSG_SET_SKILL_CHEAT                            = 0x1D8,
    SMSG_START_MIRROR_TIMER                         = 0x1D9,
    SMSG_PAUSE_MIRROR_TIMER                         = 0x1DA,
    SMSG_STOP_MIRROR_TIMER                          = 0x1DB,
    CMSG_PING                                       = 0x1DC,
    SMSG_PONG                                       = 0x1DD,
    SMSG_CLEAR_COOLDOWN                             = 0x1DE,
    SMSG_GAMEOBJECT_PAGETEXT                        = 0x1DF,
    CMSG_SETSHEATHED                                = 0x1E0,
    SMSG_COOLDOWN_CHEAT                             = 0x1E1,
    SMSG_SPELL_DELAYED                              = 0x1E2,
    CMSG_PLAYER_MACRO_OBSOLETE                      = 0x1E3,
    SMSG_PLAYER_MACRO_OBSOLETE                      = 0x1E4,
    CMSG_GHOST                                      = 0x1E5,
    CMSG_GM_INVIS                                   = 0x1E6,
    SMSG_INVALID_PROMOTION_CODE                     = 0x1E7,
    MSG_GM_BIND_OTHER                               = 0x1E8,
    MSG_GM_SUMMON                                   = 0x1E9,
    SMSG_ITEM_TIME_UPDATE                           = 0x1EA,
    SMSG_ITEM_ENCHANT_TIME_UPDATE                   = 0x1EB,
    SMSG_AUTH_CHALLENGE                             = 0x1EC,
    CMSG_AUTH_SESSION                               = 0x1ED,
    SMSG_AUTH_RESPONSE                              = 0x1EE,
    MSG_GM_SHOWLABEL                                = 0x1EF,
    CMSG_PET_CAST_SPELL                             = 0x1F0,
    MSG_SAVE_GUILD_EMBLEM                           = 0x1F1,
    MSG_TABARDVENDOR_ACTIVATE                       = 0x1F2,
    SMSG_PLAY_SPELL_VISUAL                          = 0x1F3,
    CMSG_ZONEUPDATE                                 = 0x1F4,
    SMSG_PARTYKILLLOG                               = 0x1F5,
    SMSG_COMPRESSED_UPDATE_OBJECT                   = 0x1F6,
    SMSG_PLAY_SPELL_IMPACT                          = 0x1F7,
    SMSG_EXPLORATION_EXPERIENCE                     = 0x1F8,
    CMSG_GM_SET_SECURITY_GROUP                      = 0x1F9,
    CMSG_GM_NUKE                                    = 0x1FA,
    MSG_RANDOM_ROLL                                 = 0x1FB,
    SMSG_ENVIRONMENTALDAMAGELOG                     = 0x1FC,
    CMSG_RWHOIS_OBSOLETE                            = 0x1FD,
    SMSG_RWHOIS                                     = 0x1FE,
    MSG_LOOKING_FOR_GROUP                           = 0x1FF,
    CMSG_SET_LOOKING_FOR_GROUP                      = 0x200,
    CMSG_UNLEARN_SPELL                              = 0x201,
    CMSG_UNLEARN_SKILL                              = 0x202,
    SMSG_REMOVED_SPELL                              = 0x203,
    CMSG_DECHARGE                                   = 0x204,
    CMSG_GMTICKET_CREATE                            = 0x205,
    SMSG_GMTICKET_CREATE                            = 0x206,
    CMSG_GMTICKET_UPDATETEXT                        = 0x207,
    SMSG_GMTICKET_UPDATETEXT                        = 0x208,
    SMSG_ACCOUNT_DATA_TIMES                         = 0x209,
    CMSG_REQUEST_ACCOUNT_DATA                       = 0x20A,
    CMSG_UPDATE_ACCOUNT_DATA                        = 0x20B,
    SMSG_UPDATE_ACCOUNT_DATA                        = 0x20C,
    SMSG_CLEAR_FAR_SIGHT_IMMEDIATE                  = 0x20D,
    SMSG_POWERGAINLOG_OBSOLETE                      = 0x20E,
    CMSG_GM_TEACH                                   = 0x20F,
    CMSG_GM_CREATE_ITEM_TARGET                      = 0x210,
    CMSG_GMTICKET_GETTICKET                         = 0x211,
    SMSG_GMTICKET_GETTICKET                         = 0x212,
    CMSG_UNLEARN_TALENTS                            = 0x213,
    SMSG_GAMEOBJECT_SPAWN_ANIM_OBSOLETE             = 0x214,
    SMSG_GAMEOBJECT_DESPAWN_ANIM                    = 0x215,
    MSG_CORPSE_QUERY                                = 0x216,
    CMSG_GMTICKET_DELETETICKET                      = 0x217,
    SMSG_GMTICKET_DELETETICKET                      = 0x218,
    SMSG_CHAT_WRONG_FACTION                         = 0x219,
    CMSG_GMTICKET_SYSTEMSTATUS                      = 0x21A,
    SMSG_GMTICKET_SYSTEMSTATUS                      = 0x21B,
    CMSG_SPIRIT_HEALER_ACTIVATE                     = 0x21C,
    CMSG_SET_STAT_CHEAT                             = 0x21D,
    SMSG_SET_REST_START                             = 0x21E,
    CMSG_SKILL_BUY_STEP                             = 0x21F,
    CMSG_SKILL_BUY_RANK                             = 0x220,
    CMSG_XP_CHEAT                                   = 0x221,
    SMSG_SPIRIT_HEALER_CONFIRM                      = 0x222,
    CMSG_CHARACTER_POINT_CHEAT                      = 0x223,
    SMSG_GOSSIP_POI                                 = 0x224,
    CMSG_CHAT_IGNORED                               = 0x225,
    CMSG_GM_VISION                                  = 0x226,
    CMSG_SERVER_COMMAND                             = 0x227,
    CMSG_GM_SILENCE                                 = 0x228,
    CMSG_GM_REVEALTO                                = 0x229,
    CMSG_GM_RESURRECT                               = 0x22A,
    CMSG_GM_SUMMONMOB                               = 0x22B,
    CMSG_GM_MOVECORPSE                              = 0x22C,
    CMSG_GM_FREEZE                                  = 0x22D,
    CMSG_GM_UBERINVIS                               = 0x22E,
    CMSG_GM_REQUEST_PLAYER_INFO                     = 0x22F,
    SMSG_GM_PLAYER_INFO                             = 0x230,
    CMSG_GUILD_RANK                                 = 0x231,
    CMSG_GUILD_ADD_RANK                             = 0x232,
    CMSG_GUILD_DEL_RANK                             = 0x233,
    CMSG_GUILD_SET_PUBLIC_NOTE                      = 0x234,
    CMSG_GUILD_SET_OFFICER_NOTE                     = 0x235,
    SMSG_LOGIN_VERIFY_WORLD                         = 0x236,
    CMSG_CLEAR_EXPLORATION                          = 0x237,
    CMSG_SEND_MAIL                                  = 0x238,
    SMSG_SEND_MAIL_RESULT                           = 0x239,
    CMSG_GET_MAIL_LIST                              = 0x23A,
    SMSG_MAIL_LIST_RESULT                           = 0x23B,
    CMSG_BATTLEFIELD_LIST                           = 0x23C,
    SMSG_BATTLEFIELD_LIST                           = 0x23D,
    CMSG_BATTLEFIELD_JOIN                           = 0x23E,
    SMSG_BATTLEFIELD_WIN_OBSOLETE                   = 0x23F,
    SMSG_BATTLEFIELD_LOSE_OBSOLETE                  = 0x240,
    CMSG_TAXICLEARNODE                              = 0x241,
    CMSG_TAXIENABLENODE                             = 0x242,
    CMSG_ITEM_TEXT_QUERY                            = 0x243,
    SMSG_ITEM_TEXT_QUERY_RESPONSE                   = 0x244,
    CMSG_MAIL_TAKE_MONEY                            = 0x245,
    CMSG_MAIL_TAKE_ITEM                             = 0x246,
    CMSG_MAIL_MARK_AS_READ                          = 0x247,
    CMSG_MAIL_RETURN_TO_SENDER                      = 0x248,
    CMSG_MAIL_DELETE                                = 0x249,
    CMSG_MAIL_CREATE_TEXT_ITEM                      = 0x24A,
    SMSG_SPELLLOGMISS                               = 0x24B,
    SMSG_SPELLLOGEXECUTE                            = 0x24C,
    SMSG_DEBUGAURAPROC                              = 0x24D,
    SMSG_PERIODICAURALOG                            = 0x24E,
    SMSG_SPELLDAMAGESHIELD                          = 0x24F,
    SMSG_SPELLNONMELEEDAMAGELOG                     = 0x250,
    CMSG_LEARN_TALENT                               = 0x251,
    SMSG_RESURRECT_FAILED                           = 0x252,
    CMSG_TOGGLE_PVP                                 = 0x253,
    SMSG_ZONE_UNDER_ATTACK                          = 0x254,
    MSG_AUCTION_HELLO                               = 0x255,
    CMSG_AUCTION_SELL_ITEM                          = 0x256,
    CMSG_AUCTION_REMOVE_ITEM                        = 0x257,
    CMSG_AUCTION_LIST_ITEMS                         = 0x258,
    CMSG_AUCTION_LIST_OWNER_ITEMS                   = 0x259,
    CMSG_AUCTION_PLACE_BID                          = 0x25A,
    SMSG_AUCTION_COMMAND_RESULT                     = 0x25B,
    SMSG_AUCTION_LIST_RESULT                        = 0x25C,
    SMSG_AUCTION_OWNER_LIST_RESULT                  = 0x25D,
    SMSG_AUCTION_BIDDER_NOTIFICATION                = 0x25E,
    SMSG_AUCTION_OWNER_NOTIFICATION                 = 0x25F,
    SMSG_PROCRESIST                                 = 0x260,
    SMSG_STANDSTATE_CHANGE_FAILURE_OBSOLETE         = 0x261,
    SMSG_DISPEL_FAILED                              = 0x262,
    SMSG_SPELLORDAMAGE_IMMUNE                       = 0x263,
    CMSG_AUCTION_LIST_BIDDER_ITEMS                  = 0x264,
    SMSG_AUCTION_BIDDER_LIST_RESULT                 = 0x265,
    SMSG_SET_FLAT_SPELL_MODIFIER                    = 0x266,
    SMSG_SET_PCT_SPELL_MODIFIER                     = 0x267,
    CMSG_SET_AMMO                                   = 0x268,
    SMSG_CORPSE_RECLAIM_DELAY                       = 0x269,
    CMSG_SET_ACTIVE_MOVER                           = 0x26A,
    CMSG_PET_CANCEL_AURA                            = 0x26B,
    CMSG_PLAYER_AI_CHEAT                            = 0x26C,
    CMSG_CANCEL_AUTO_REPEAT_SPELL                   = 0x26D,
    MSG_GM_ACCOUNT_ONLINE                           = 0x26E,
    MSG_LIST_STABLED_PETS                           = 0x26F,
    CMSG_STABLE_PET                                 = 0x270,
    CMSG_UNSTABLE_PET                               = 0x271,
    CMSG_BUY_STABLE_SLOT                            = 0x272,
    SMSG_STABLE_RESULT                              = 0x273,
    CMSG_STABLE_REVIVE_PET                          = 0x274,
    CMSG_STABLE_SWAP_PET                            = 0x275,
    MSG_QUEST_PUSH_RESULT                           = 0x276,
    SMSG_PLAY_MUSIC                                 = 0x277,
    SMSG_PLAY_OBJECT_SOUND                          = 0x278,
    CMSG_REQUEST_PET_INFO                           = 0x279,
    CMSG_FAR_SIGHT                                  = 0x27A,
    SMSG_SPELLDISPELLOG                             = 0x27B,
    SMSG_DAMAGE_CALC_LOG                            = 0x27C,
    CMSG_ENABLE_DAMAGE_LOG                          = 0x27D,
    CMSG_GROUP_CHANGE_SUB_GROUP                     = 0x27E,
    CMSG_REQUEST_PARTY_MEMBER_STATS                 = 0x27F,
    CMSG_GROUP_SWAP_SUB_GROUP                       = 0x280,
    CMSG_RESET_FACTION_CHEAT                        = 0x281,
    CMSG_AUTOSTORE_BANK_ITEM                        = 0x282,
    CMSG_AUTOBANK_ITEM                              = 0x283,
    MSG_QUERY_NEXT_MAIL_TIME                        = 0x284,
    SMSG_RECEIVED_MAIL                              = 0x285,
    SMSG_RAID_GROUP_ONLY                            = 0x286,
    CMSG_SET_DURABILITY_CHEAT                       = 0x287,
    CMSG_SET_PVP_RANK_CHEAT                         = 0x288,
    CMSG_ADD_PVP_MEDAL_CHEAT                        = 0x289,
    CMSG_DEL_PVP_MEDAL_CHEAT                        = 0x28A,
    CMSG_SET_PVP_TITLE                              = 0x28B,
    SMSG_PVP_CREDIT                                 = 0x28C,
    SMSG_AUCTION_REMOVED_NOTIFICATION               = 0x28D,
    CMSG_GROUP_RAID_CONVERT                         = 0x28E,
    CMSG_GROUP_ASSISTANT_LEADER                     = 0x28F,
    CMSG_BUYBACK_ITEM                               = 0x290,
    SMSG_SERVER_MESSAGE                             = 0x291,
    CMSG_MEETINGSTONE_JOIN                          = 0x292,// lua: SetSavedInstanceExtend
    CMSG_MEETINGSTONE_LEAVE                         = 0x293,
    CMSG_MEETINGSTONE_CHEAT                         = 0x294,
    SMSG_MEETINGSTONE_SETQUEUE                      = 0x295,
    CMSG_MEETINGSTONE_INFO                          = 0x296,
    SMSG_MEETINGSTONE_COMPLETE                      = 0x297,
    SMSG_MEETINGSTONE_IN_PROGRESS                   = 0x298,
    SMSG_MEETINGSTONE_MEMBER_ADDED                  = 0x299,
    CMSG_GMTICKETSYSTEM_TOGGLE                      = 0x29A,
    CMSG_CANCEL_GROWTH_AURA                         = 0x29B,
    SMSG_CANCEL_AUTO_REPEAT                         = 0x29C,
    SMSG_STANDSTATE_UPDATE                          = 0x29D,
    SMSG_LOOT_ALL_PASSED                            = 0x29E,
    SMSG_LOOT_ROLL_WON                              = 0x29F,
    CMSG_LOOT_ROLL                                  = 0x2A0,
    SMSG_LOOT_START_ROLL                            = 0x2A1,
    SMSG_LOOT_ROLL                                  = 0x2A2,
    CMSG_LOOT_MASTER_GIVE                           = 0x2A3,
    SMSG_LOOT_MASTER_LIST                           = 0x2A4,
    SMSG_SET_FORCED_REACTIONS                       = 0x2A5,
    SMSG_SPELL_FAILED_OTHER                         = 0x2A6,
    SMSG_GAMEOBJECT_RESET_STATE                     = 0x2A7,
    CMSG_REPAIR_ITEM                                = 0x2A8,
    SMSG_CHAT_PLAYER_NOT_FOUND                      = 0x2A9,
    MSG_TALENT_WIPE_CONFIRM                         = 0x2AA,
    SMSG_SUMMON_REQUEST                             = 0x2AB,
    CMSG_SUMMON_RESPONSE                            = 0x2AC,
    MSG_MOVE_TOGGLE_GRAVITY_CHEAT                   = 0x2AD,
    SMSG_MONSTER_MOVE_TRANSPORT                     = 0x2AE,
    SMSG_PET_BROKEN                                 = 0x2AF,
    MSG_MOVE_FEATHER_FALL                           = 0x2B0,
    MSG_MOVE_WATER_WALK                             = 0x2B1,
    CMSG_SERVER_BROADCAST                           = 0x2B2,
    CMSG_SELF_RES                                   = 0x2B3,
    SMSG_FEIGN_DEATH_RESISTED                       = 0x2B4,
    CMSG_RUN_SCRIPT                                 = 0x2B5,
    SMSG_SCRIPT_MESSAGE                             = 0x2B6,
    SMSG_DUEL_COUNTDOWN                             = 0x2B7,
    SMSG_AREA_TRIGGER_MESSAGE                       = 0x2B8,
    CMSG_TOGGLE_HELM                                = 0x2B9,
    CMSG_TOGGLE_CLOAK                               = 0x2BA,
    SMSG_MEETINGSTONE_JOINFAILED                    = 0x2BB,
    SMSG_PLAYER_SKINNED                             = 0x2BC,
    SMSG_DURABILITY_DAMAGE_DEATH                    = 0x2BD,
    CMSG_SET_EXPLORATION                            = 0x2BE,
    CMSG_SET_ACTIONBAR_TOGGLES                      = 0x2BF,
    UMSG_DELETE_GUILD_CHARTER                       = 0x2C0,
    MSG_PETITION_RENAME                             = 0x2C1,
    SMSG_INIT_WORLD_STATES                          = 0x2C2,
    SMSG_UPDATE_WORLD_STATE                         = 0x2C3,
    CMSG_ITEM_NAME_QUERY                            = 0x2C4,
    SMSG_ITEM_NAME_QUERY_RESPONSE                   = 0x2C5,
    SMSG_PET_ACTION_FEEDBACK                        = 0x2C6,
    CMSG_CHAR_RENAME                                = 0x2C7,
    SMSG_CHAR_RENAME                                = 0x2C8,
    CMSG_MOVE_SPLINE_DONE                           = 0x2C9,
    CMSG_MOVE_FALL_RESET                            = 0x2CA,
    SMSG_INSTANCE_SAVE_CREATED                      = 0x2CB,
    SMSG_RAID_INSTANCE_INFO                         = 0x2CC,
    CMSG_REQUEST_RAID_INFO                          = 0x2CD,
    CMSG_MOVE_TIME_SKIPPED                          = 0x2CE,
    CMSG_MOVE_FEATHER_FALL_ACK                      = 0x2CF,
    CMSG_MOVE_WATER_WALK_ACK                        = 0x2D0,
    CMSG_MOVE_NOT_ACTIVE_MOVER                      = 0x2D1,
    SMSG_PLAY_SOUND                                 = 0x2D2,
    CMSG_BATTLEFIELD_STATUS                         = 0x2D3,
    SMSG_BATTLEFIELD_STATUS                         = 0x2D4,
    CMSG_BATTLEFIELD_PORT                           = 0x2D5,
    MSG_INSPECT_HONOR_STATS                         = 0x2D6,
    CMSG_BATTLEMASTER_HELLO                         = 0x2D7,
    CMSG_MOVE_START_SWIM_CHEAT                      = 0x2D8,
    CMSG_MOVE_STOP_SWIM_CHEAT                       = 0x2D9,
    SMSG_FORCE_WALK_SPEED_CHANGE                    = 0x2DA,
    CMSG_FORCE_WALK_SPEED_CHANGE_ACK                = 0x2DB,
    SMSG_FORCE_SWIM_BACK_SPEED_CHANGE               = 0x2DC,
    CMSG_FORCE_SWIM_BACK_SPEED_CHANGE_ACK           = 0x2DD,
    SMSG_FORCE_TURN_RATE_CHANGE                     = 0x2DE,
    CMSG_FORCE_TURN_RATE_CHANGE_ACK                 = 0x2DF,
    MSG_PVP_LOG_DATA                                = 0x2E0,
    CMSG_LEAVE_BATTLEFIELD                          = 0x2E1,
    CMSG_AREA_SPIRIT_HEALER_QUERY                   = 0x2E2,
    CMSG_AREA_SPIRIT_HEALER_QUEUE                   = 0x2E3,
    SMSG_AREA_SPIRIT_HEALER_TIME                    = 0x2E4,
    CMSG_GM_UNTEACH                                 = 0x2E5,
    SMSG_WARDEN_DATA                                = 0x2E6,
    CMSG_WARDEN_DATA                                = 0x2E7,
    SMSG_GROUP_JOINED_BATTLEGROUND                  = 0x2E8,
    MSG_BATTLEGROUND_PLAYER_POSITIONS               = 0x2E9,
    CMSG_PET_STOP_ATTACK                            = 0x2EA,
    SMSG_BINDER_CONFIRM                             = 0x2EB,
    SMSG_BATTLEGROUND_PLAYER_JOINED                 = 0x2EC,
    SMSG_BATTLEGROUND_PLAYER_LEFT                   = 0x2ED,
    CMSG_BATTLEMASTER_JOIN                          = 0x2EE,
    SMSG_ADDON_INFO                                 = 0x2EF,
    CMSG_PET_UNLEARN                                = 0x2F0,
    SMSG_PET_UNLEARN_CONFIRM                        = 0x2F1,
    SMSG_PARTY_MEMBER_STATS_FULL                    = 0x2F2,
    CMSG_PET_SPELL_AUTOCAST                         = 0x2F3,
    SMSG_WEATHER                                    = 0x2F4,
    SMSG_PLAY_TIME_WARNING                          = 0x2F5,
    SMSG_MINIGAME_SETUP                             = 0x2F6,
    SMSG_MINIGAME_STATE                             = 0x2F7,
    CMSG_MINIGAME_MOVE                              = 0x2F8,
    SMSG_MINIGAME_MOVE_FAILED                       = 0x2F9,
    SMSG_RAID_INSTANCE_MESSAGE                      = 0x2FA,
    SMSG_COMPRESSED_MOVES                           = 0x2FB,
    CMSG_GUILD_INFO_TEXT                            = 0x2FC,
    SMSG_CHAT_RESTRICTED                            = 0x2FD,
    SMSG_SPLINE_SET_RUN_SPEED                       = 0x2FE,
    SMSG_SPLINE_SET_RUN_BACK_SPEED                  = 0x2FF,
    SMSG_SPLINE_SET_SWIM_SPEED                      = 0x300,
    SMSG_SPLINE_SET_WALK_SPEED                      = 0x301,
    SMSG_SPLINE_SET_SWIM_BACK_SPEED                 = 0x302,
    SMSG_SPLINE_SET_TURN_RATE                       = 0x303,
    SMSG_SPLINE_MOVE_UNROOT                         = 0x304,
    SMSG_SPLINE_MOVE_FEATHER_FALL                   = 0x305,
    SMSG_SPLINE_MOVE_NORMAL_FALL                    = 0x306,
    SMSG_SPLINE_MOVE_SET_HOVER                      = 0x307,
    SMSG_SPLINE_MOVE_UNSET_HOVER                    = 0x308,
    SMSG_SPLINE_MOVE_WATER_WALK                     = 0x309,
    SMSG_SPLINE_MOVE_LAND_WALK                      = 0x30A,
    SMSG_SPLINE_MOVE_START_SWIM                     = 0x30B,
    SMSG_SPLINE_MOVE_STOP_SWIM                      = 0x30C,
    SMSG_SPLINE_MOVE_SET_RUN_MODE                   = 0x30D,
    SMSG_SPLINE_MOVE_SET_WALK_MODE                  = 0x30E,
    CMSG_GM_NUKE_ACCOUNT                            = 0x30F,
    MSG_GM_DESTROY_CORPSE                           = 0x310,
    CMSG_GM_DESTROY_ONLINE_CORPSE                   = 0x311,
    CMSG_ACTIVATETAXIEXPRESS                        = 0x312,
    SMSG_SET_FACTION_ATWAR                          = 0x313,
    SMSG_GAMETIMEBIAS_SET                           = 0x314,
    CMSG_DEBUG_ACTIONS_START                        = 0x315,
    CMSG_DEBUG_ACTIONS_STOP                         = 0x316,
    CMSG_SET_FACTION_INACTIVE                       = 0x317,
    CMSG_SET_WATCHED_FACTION                        = 0x318,
    MSG_MOVE_TIME_SKIPPED                           = 0x319,
    SMSG_SPLINE_MOVE_ROOT                           = 0x31A,
    CMSG_SET_EXPLORATION_ALL                        = 0x31B,
    SMSG_INVALIDATE_PLAYER                          = 0x31C,
    CMSG_RESET_INSTANCES                            = 0x31D,
    SMSG_INSTANCE_RESET                             = 0x31E,
    SMSG_INSTANCE_RESET_FAILED                      = 0x31F,
    SMSG_UPDATE_LAST_INSTANCE                       = 0x320,
    MSG_RAID_TARGET_UPDATE                          = 0x321,
    MSG_RAID_READY_CHECK                            = 0x322,
    CMSG_LUA_USAGE                                  = 0x323,
    SMSG_PET_ACTION_SOUND                           = 0x324,
    SMSG_PET_DISMISS_SOUND                          = 0x325,
    SMSG_GHOSTEE_GONE                               = 0x326,
    CMSG_GM_UPDATE_TICKET_STATUS                    = 0x327,
    SMSG_GM_TICKET_STATUS_UPDATE                    = 0x328,
    CMSG_GMSURVEY_SUBMIT                            = 0x32A,
    SMSG_UPDATE_INSTANCE_OWNERSHIP                  = 0x32B,
    CMSG_IGNORE_KNOCKBACK_CHEAT                     = 0x32C,
    SMSG_CHAT_PLAYER_AMBIGUOUS                      = 0x32D,
    MSG_DELAY_GHOST_TELEPORT                        = 0x32E,
    SMSG_SPELLINSTAKILLLOG                          = 0x32F,
    SMSG_SPELL_UPDATE_CHAIN_TARGETS                 = 0x330,
    CMSG_CHAT_FILTERED                              = 0x331,
    SMSG_EXPECTED_SPAM_RECORDS                      = 0x332,
    SMSG_SPELLSTEALLOG                              = 0x333,
    CMSG_LOTTERY_QUERY_OBSOLETE                     = 0x334,
    SMSG_LOTTERY_QUERY_RESULT_OBSOLETE              = 0x335,
    CMSG_BUY_LOTTERY_TICKET_OBSOLETE                = 0x336,
    SMSG_LOTTERY_RESULT_OBSOLETE                    = 0x337,
    SMSG_CHARACTER_PROFILE                          = 0x338,
    SMSG_CHARACTER_PROFILE_REALM_CONNECTED          = 0x339,
    SMSG_DEFENSE_MESSAGE                            = 0x33A,
    MSG_GM_RESETINSTANCELIMIT                       = 0x33C,
    // SMSG_MOTD                                       = 0x33D,
    SMSG_MOVE_SET_FLIGHT                            = 0x33E,
    SMSG_MOVE_UNSET_FLIGHT                          = 0x33F,
    CMSG_MOVE_FLIGHT_ACK                            = 0x340,
    MSG_MOVE_START_SWIM_CHEAT                       = 0x341,
    MSG_MOVE_STOP_SWIM_CHEAT                        = 0x342,
    // [-ZERO] Last existed in 1.12.1 opcode, maybe some renumbering from other side
    CMSG_CANCEL_MOUNT_AURA                          = 0x375,
    CMSG_CANCEL_TEMP_ENCHANTMENT                    = 0x379,
    CMSG_MAELSTROM_INVALIDATE_CACHE                 = 0x387,
    CMSG_SET_TAXI_BENCHMARK_MODE                    = 0x389,
    CMSG_MOVE_CHNG_TRANSPORT                        = 0x38D,
    MSG_PARTY_ASSIGNMENT                            = 0x38E,
    SMSG_OFFER_PETITION_ERROR                       = 0x38F,
    SMSG_RESET_FAILED_NOTIFY                        = 0x396,
    SMSG_REAL_GROUP_UPDATE                          = 0x397,
    SMSG_INIT_EXTRA_AURA_INFO                       = 0x3A3,
    SMSG_SET_EXTRA_AURA_INFO                        = 0x3A4,
    SMSG_SET_EXTRA_AURA_INFO_NEED_UPDATE            = 0x3A5,
    SMSG_SPELL_CHANCE_PROC_LOG                      = 0x3AA,
    CMSG_MOVE_SET_RUN_SPEED                         = 0x3AB,
    SMSG_DISMOUNT                                   = 0x3AC,
    MSG_RAID_READY_CHECK_CONFIRM                    = 0x3AE,
    SMSG_CLEAR_TARGET                               = 0x3BE,
    CMSG_BOT_DETECTED                               = 0x3BF,
    SMSG_KICK_REASON                                = 0x3C4,
    MSG_RAID_READY_CHECK_FINISHED                   = 0x3C5,
    CMSG_TARGET_CAST                                = 0x3CF,
    CMSG_TARGET_SCRIPT_CAST                         = 0x3D0,
    CMSG_CHANNEL_DISPLAY_LIST                       = 0x3D1,
    CMSG_GET_CHANNEL_MEMBER_COUNT                   = 0x3D3,
    SMSG_CHANNEL_MEMBER_COUNT                       = 0x3D4,
    CMSG_DEBUG_LIST_TARGETS                         = 0x3D7,
    SMSG_DEBUG_LIST_TARGETS                         = 0x3D8,
    CMSG_PARTY_SILENCE                              = 0x3DC,
    CMSG_PARTY_UNSILENCE                            = 0x3DD,
    MSG_NOTIFY_PARTY_SQUELCH                        = 0x3DE,
    SMSG_COMSAT_RECONNECT_TRY                       = 0x3DF,
    SMSG_COMSAT_DISCONNECT                          = 0x3E0,
    SMSG_COMSAT_CONNECT_FAIL                        = 0x3E1,
    CMSG_SET_CHANNEL_WATCH                          = 0x3EE,
    SMSG_USERLIST_ADD                               = 0x3EF,
    SMSG_USERLIST_REMOVE                            = 0x3F0,
    SMSG_USERLIST_UPDATE                            = 0x3F1,
    CMSG_CLEAR_CHANNEL_WATCH                        = 0x3F2,
    SMSG_GOGOGO_OBSOLETE                            = 0x3F4,
    SMSG_ECHO_PARTY_SQUELCH                         = 0x3F5,
    CMSG_SPELLCLICK                                 = 0x3F7,
    SMSG_LOOT_LIST                                  = 0x3F8,
    MSG_GUILD_PERMISSIONS                           = 0x3FC,
    MSG_GUILD_EVENT_LOG_QUERY                       = 0x3FE,
    CMSG_MAELSTROM_RENAME_GUILD                     = 0x3FF,
    CMSG_GET_MIRRORIMAGE_DATA                       = 0x400,
    SMSG_MIRRORIMAGE_DATA                           = 0x401,
    SMSG_FORCE_DISPLAY_UPDATE                       = 0x402,
    SMSG_SPELL_CHANCE_RESIST_PUSHBACK               = 0x403,
    CMSG_IGNORE_DIMINISHING_RETURNS_CHEAT           = 0x404,
    SMSG_IGNORE_DIMINISHING_RETURNS_CHEAT           = 0x405,
    CMSG_KEEP_ALIVE                                 = 0x406,
    SMSG_RAID_READY_CHECK_ERROR                     = 0x407,
    CMSG_OPT_OUT_OF_LOOT                            = 0x408,
    CMSG_SET_GRANTABLE_LEVELS                       = 0x40B,
    CMSG_GRANT_LEVEL                                = 0x40C,
    CMSG_DECLINE_CHANNEL_INVITE                     = 0x40F,
    CMSG_GROUPACTION_THROTTLED                      = 0x410,
    SMSG_OVERRIDE_LIGHT                             = 0x411,
    SMSG_TOTEM_CREATED                              = 0x412,
    CMSG_TOTEM_DESTROYED                            = 0x413,
    CMSG_EXPIRE_RAID_INSTANCE                       = 0x414,
    CMSG_NO_SPELL_VARIANCE                          = 0x415,
    CMSG_QUESTGIVER_STATUS_MULTIPLE_QUERY           = 0x416,
    SMSG_QUESTGIVER_STATUS_MULTIPLE                 = 0x417,
    CMSG_QUERY_SERVER_BUCK_DATA                     = 0x41A,
    CMSG_CLEAR_SERVER_BUCK_DATA                     = 0x41B,
    SMSG_SERVER_BUCK_DATA                           = 0x41C,
    SMSG_SEND_UNLEARN_SPELLS                        = 0x41D,
    SMSG_PROPOSE_LEVEL_GRANT                        = 0x41E,
    CMSG_ACCEPT_LEVEL_GRANT                         = 0x41F,
    SMSG_REFER_A_FRIEND_FAILURE                     = 0x420,
    SMSG_SUMMON_CANCEL                              = 0x423
};

// Don't forget to change this value and add opcode name to Opcodes.cpp when you add new opcode!
#define NUM_MSG_TYPES 0x424

/// Player state
enum SessionStatus
{
    STATUS_AUTHED = 0,                     ///< Player authenticated (_player==NULL, m_playerRecentlyLogout = false or will be reset before handler call)
    STATUS_LOGGEDIN,                       ///< Player in game (_player!=NULL, inWorld())
    STATUS_TRANSFER,                       ///< Player transferring to another map (_player!=NULL, !inWorld())
    STATUS_LOGGEDIN_OR_RECENTLY_LOGGEDOUT, ///< _player!= NULL or _player==NULL && m_playerRecentlyLogout)
    STATUS_NEVER,                          ///< Opcode not accepted from client (deprecated or server side only)
    STATUS_UNHANDLED                       ///< We don' handle this opcode yet
};

/**
 * This determines how a \ref WorldPacket is handled by MaNGOS. This can be either in the
 * same function as we received it in, this is unusual, or it can be in:
 * - \ref World::UpdateSessions if it's not thread safe
 * - \ref Map::Update if it is thread safe
 */
enum PacketProcessing
{
    PROCESS_INPLACE = 0,   ///< process packet whenever we receive it - mostly for non-handled or non-implemented packets
    PROCESS_THREADUNSAFE,  ///< packet is not thread-safe - process it in \ref World::UpdateSessions
    PROCESS_THREADSAFE     ///< packet is thread-safe - process it in \ref Map::Update
};

class WorldPacket;

/**
 * A structure containing some of the necessary info to handle a \ref WorldPacket when it comes in.
 * The most interesting thing in here is the \ref OpcodeHandler::handler that actually does
 * something with one of the opcodes (see \ref Opcodes) that came in.
 */
struct OpcodeHandler
{
    ///A string representation of the name of this opcode (see \ref Opcodes)
    char const* name;
    ///The status for this handler, it tells whether or not we will handle the packet at all and
    ///when we will handle it.
    SessionStatus status;
    ///This tells where the packet should be processed, ie: is it thread un/safe, which in turn
    ///determines where it will be processed
    PacketProcessing packetProcessing;
    ///The callback called for this opcode which will work some magic
    void (WorldSession::*handler)(WorldPacket& recvPacket);
};

typedef std::map< uint16, OpcodeHandler> OpcodeMap;

class Opcodes
{
    public:
        Opcodes();
        ~Opcodes();
    public:
        void BuildOpcodeList();
        void StoreOpcode(uint16 Opcode, char const* name, SessionStatus status, PacketProcessing process, void (WorldSession::*handler)(WorldPacket& recvPacket))
        {
            OpcodeHandler& ref = mOpcodeMap[Opcode];
            ref.name = name;
            ref.status = status;
            ref.packetProcessing = process;
            ref.handler = handler;
        }

        /// Lookup opcode
        inline OpcodeHandler const* LookupOpcode(uint16 id) const
        {
            OpcodeMap::const_iterator itr = mOpcodeMap.find(id);
            if (itr != mOpcodeMap.end())
                { return &itr->second; }
            return NULL;
        }

        /// compatible with other mangos branches access

        inline OpcodeHandler const& operator[](uint16 id) const
        {
            OpcodeMap::const_iterator itr = mOpcodeMap.find(id);
            if (itr != mOpcodeMap.end())
                { return itr->second; }
            return emptyHandler;
        }

        static OpcodeHandler const emptyHandler;

        OpcodeMap mOpcodeMap;
};

#define opcodeTable MaNGOS::Singleton<Opcodes>::Instance()

/// Lookup opcode name for human understandable logging
inline const char* LookupOpcodeName(uint16 id)
{
    if (OpcodeHandler const* op = opcodeTable.LookupOpcode(id))
        { return op->name; }
    return "Received unknown opcode, it's more than max!";
}
#endif

/**
 * \var OpcodesList::SMSG_PERIODICAURALOG
 * This opcode is used to send data for the combat log when you receive either periodic damage or
 * buffs from a \ref Aura in some way, ie  you gain 10 life every second, you increase your regen
 * of power or something along those lines. The data that needs to be sent is a little different
 * depending on the \ref Modifier for the \ref Aura, what should always be included though is:
 * - The victims Pack GUID (see \ref Object::GetPackGUID)
 * - The casting \ref Player s Pack GUID (see \ref Object::GetPackGUID)
 * - The spellid for the \ref Aura (see \ref Aura::GetId) as a \ref uint32
 * - A 1 as a \ref uint32 this is the count of something (what)
 * - The id of the aura see \ref Modifier::m_auraname as a \ref uint32
 *
 * Now comes different parts depending on what value the \ref Modifier::m_auraname has, if it
 * is \ref AuraType::SPELL_AURA_PERIODIC_DAMAGE or
 * \ref AuraType::SPELL_AURA_PERIODIC_DAMAGE_PERCENT then this is sent:
 * - Damage done as a \ref uint32 from \ref SpellPeriodicAuraLogInfo::damage
 * - The \ref SpellSchools of the \ref SpellEntry for the \ref Aura as a \ref uint32 (see
 * \ref SpellEntry::School)
 * - How much that was absorbed as a \ref uint32
 * - How mcuh that was resisted as a \ref uint32
 *
 * If the \ref Modifier::m_auraname has one of the values of:
 * \ref AuraType::SPELL_AURA_PERIODIC_HEAL or \ref AuraType::SPELL_AURA_OBS_MOD_HEALTH then
 * this should be sent:
 * - Damage/healing (in this case) done as a \ref uint32
 *
 * If the \ref Modifier::m_auraname has one of the values of:
 * \ref AuraType::SPELL_AURA_OBS_MOD_MANA or \ref AuraType::SPELL_AURA_PERIODIC_ENERGIZE then
 * this should be sent:
 * - The \ref Modifier::m_miscvalue as a \ref uint32, in this case it's a power type from the
 * \ref Powers
 * - The damage/mana earned (in this case) as a \ref uint32
 *
 * If the \ref Modifier::m_auraname has one of the values of:
 * \ref AuraType::SPELL_AURA_PERIODIC_MANA_LEECH then this should be sent:
 * - The \ref Modifier::m_miscvalue as a \ref uint32, in this case it's a power type from the
 * \ref Powers
 * - The damage/amount of mana drained (in this case) as a \ref uint32
 * - The gain multiplier as a \ref float from the which probably increases how much power was
 * drained
 *
 * To not create this packet and send it all the time you need it you can use
 * \ref Unit::SendPeriodicAuraLog
 *
 * Also, this should be sent with \ref Object::SendMessageToSet so that all nearby (in
 * the same \ref Cell) \ref Player s get the information. To do this with an \ref Aura
 * one could use \ref Aura::GetTarget and then use the \ref Unit::SendMessageToSet
 * \todo Is it actually for the combat log?
 * \todo Is it in the same \ref Cell?
 * \todo What is the count that is sent as a uint32?
 * \todo Document the multiplier in some way?
 */

/**
 * \var OpcodesList::SMSG_SPELLNONMELEEDAMAGELOG
 * This opcode is used to send data for the combat log when you damage someone with a non melee
 * spell, ie frostbolt.
 * The data that needs to be sent is the following in the same order:
 * - The victims Pack GUID (see \ref Object::GetPackGUID)
 * - The \ref Player s Pack GUID (see \ref Object::GetPackGUID)
 * - Id of the spell that was used as a \ref uint32
 * - The amount of damage that was done (not including resisted damage etc) as a \ref uint32
 * - The \ref SpellSchoolMask of the \ref Spell as a \ref uint8, should be from the representation
 * in \ref SpellSchools though, to do this one can use \ref GetFirstSchoolInMask
 * - The amount of absorbed damage as a \ref uint32
 * - The amount of resisted damage as a \ref uint32
 * - A \ref uint8 which if it is 1 shows the spell name for the client, ie: "%s's ranged shot
 * hit %s for %d damage" (taken from source) and if it's 0 no message is shown
 * - A \ref uint8 value that seems to be unused
 * - The amount of blocked damage as a \ref uint32
 * - The \ref HitInfo as a \ref uint32 which tells what happened it would seem
 * - A \ref uint8 that's usually 0 and is used as a flag to use extended data (taken from source)
 *
 * To not create this packet and send it all the time you need it you can use
 * \ref Unit::SendSpellNonMeleeDamageLog
 *
 * Also, this should be sent with \ref Object::SendMessageToSet so that all nearby (in
 * the same \ref Cell) \ref Player s get the information.
 * \todo Is it actually for the combat log?
 * \todo Is it in the same \ref Cell?
 */

/**
 * \var OpcodesList::SMSG_SPELLENERGIZELOG
 * This opcode is used to send data for the combat log when you gain energy in some way.
 * The data that needs to be sent is the following in the same order:
 * - The victims Pack GUID (see \ref Object::GetPackGUID)
 * - The \ref Player s Pack GUID (see \ref Object::GetPackGUID)
 * - the spellid as a \ref uint32
 * - the powertype as a \ref uint32, see \ref Powers for the available power types
 * - the damage or in this case gain as a \ref uint32
 *
 * To not create this packet and send it all the time you need it you can use
 * \ref Unit::SendEnergizeSpellLog
 * Also, this should be sent with \ref Object::SendMessageToSet so that all nearby (in
 * the same \ref Cell) \ref Player s get the information.
 * \todo Is it actually for the combat log?
 * \todo Is it in the same \ref Cell?
 */

/**
 * \var OpcodesList::SMSG_SPELLHEALLOG
 * This opcode is used to send data for the combat log when healing is done. The data
 * that needs to be sent is the following in the same order:
 * - The victims Pack GUID (see \ref Object::GetPackGUID)
 * - The \ref Player s Pack GUID (see \ref Object::GetPackGUID)
 * - The spellid as a \ref uint32
 * - The damage/healing done as a \ref uint32
 * - If it was critical or not as a \ref uint8 (1 meaning critical, 0 meaning normal)
 * - And a \ref uint8 with the value 0 which doesn't seem to be used in the client
 *
 * To not create this packet and send it all the time you need it you can use
 * \ref Unit::SendHealSpellLog
 * Also, this should be sent with \ref Object::SendMessageToSet so that all nearby (in
 * the same \ref Cell) \ref Player s get the information.
 * \todo Is it actually for the combat log?
 * \todo Is it in the same \ref Cell?
 */

/**
 * \var OpcodesList::SMSG_ATTACKERSTATEUPDATE
 * This opcode is used to send information about a recent hit, who it hit, how
 * much damage it did and so forth. See the \ref CalcDamageInfo structure for more
 * info on what will be sent. The data that needs to be sent is the following in
 * the same order:
 * - The \ref CalcDamageInfo::HitInfo as a \ref uint32
 * - The \ref Unit s Pack GUID (see \ref Object::GetPackGUID)
 * - The targets Pack GUID (see \ref Object::GetPackGUID)
 * - The full damage that was done as a \ref uint32
 * - A 1 as a \ref uint8, this acts as the subdamage count (could it be higher?)
 * - A \ref uint32 of \code{.cpp} GetFirstSchoolInMask(damageInfo->damageSchoolMask) \endcode
 * Need to find out what this does
 * - A float representation of the damage (seen as sub damage from comments)
 * - A \ref uint32 representation of the same damage
 * - A \ref uint32 representation of how much was absorbed (see \ref CalcDamageInfo::absorb)
 * - A \ref uint32 representation of how much was resisted (see \ref CalcDamageInfo::resist)
 * - The targets state as a \ref uint32 (see \ref CalcDamageInfo::TargetState)
 * - If the absorbed part is zero add a 0 as an \ref uint32 otherwise add a -1 as an \ref uint32
 * - The spell id as a \ref uint32 if a spell was used, although in
 * \ref Unit::SendAttackStateUpdate it is always 0.
 * - The blocked amount as a \ref uint32 (see \ref CalcDamageInfo::blocked_amount) this is
 * normally \ref HitInfo::HITINFO_NOACTION according to comments in \ref Unit::SendAttackStateUpdate
 *
 * It appears this should also be sent with \ref Object::SendMessageToSet to that all nearby (in
 * the same \ref Cell) \ref Player s can get take part of the info
 * \see VictimState
 * \todo Is this correct? Is it really about a recent hit?
 */


/// @}
