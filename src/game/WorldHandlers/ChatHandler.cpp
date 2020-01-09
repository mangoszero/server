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

#include "Common.h"
#include "Log.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "World.h"
#include "Opcodes.h"
#include "ObjectMgr.h"
#include "Chat.h"
#include "ChannelMgr.h"
#include "Group.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Player.h"
#include "SpellAuras.h"
#include "Language.h"
#include "Util.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */
#ifdef ENABLE_PLAYERBOTS
#include "playerbot.h"
#include "RandomPlayerbotMgr.h"
#endif

bool WorldSession::processChatmessageFurtherAfterSecurityChecks(std::string& msg, uint32 lang)
{
    if (lang != LANG_ADDON)
    {
        //client can't send more than 255 character's, so if we break limit - that's cheater
        if (msg.size() > 255)
        {
            sLog.outError("Player %s (GUID: %u) tries send a chatmessage with more than 255 symbols", GetPlayer()->GetName(), GetPlayer()->GetGUIDLow());
            return false;
        }

        // strip invisible characters for non-addon messages
        if (sWorld.getConfig(CONFIG_BOOL_CHAT_FAKE_MESSAGE_PREVENTING))
            { stripLineInvisibleChars(msg); }

        if (sWorld.getConfig(CONFIG_UINT32_CHAT_STRICT_LINK_CHECKING_SEVERITY) && GetSecurity() < SEC_MODERATOR
            && !ChatHandler(this).isValidChatMessage(msg.c_str()))
        {
            sLog.outError("Player %s (GUID: %u) sent a chatmessage with an invalid link: %s", GetPlayer()->GetName(),
                          GetPlayer()->GetGUIDLow(), msg.c_str());
            if (sWorld.getConfig(CONFIG_UINT32_CHAT_STRICT_LINK_CHECKING_KICK))
                { KickPlayer(); }
            return false;
        }
    }

    return true;
}

void WorldSession::HandleMessagechatOpcode(WorldPacket& recv_data)
{
    uint32 type;
    uint32 lang;

    recv_data >> type;
    recv_data >> lang;

    if (type >= MAX_CHAT_MSG_TYPE)
    {
        sLog.outError("CHAT: Wrong message type received: %u", type);
        return;
    }

    DEBUG_LOG("CHAT: packet received. type %u, lang %u", type, lang);

    // prevent talking at unknown language (cheating)
    LanguageDesc const* langDesc = GetLanguageDescByID(lang);
    if (!langDesc)
    {
        SendNotification(LANG_UNKNOWN_LANGUAGE);
        return;
    }

    //prevent cheating, by sending LANG_UNIVERSAL
    if ((langDesc->lang_id == LANG_UNIVERSAL && !sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHAT) && GetSecurity() == SEC_PLAYER) ||
         (langDesc->skill_id != 0 && !_player->HasSkill(langDesc->skill_id)))
    {
        SendNotification(LANG_NOT_LEARNED_LANGUAGE);
        return;
    }

    if (lang == LANG_ADDON)
    {
        // Disabled addon channel?
        if (!sWorld.getConfig(CONFIG_BOOL_ADDON_CHANNEL))
        { return; }
    }
    // LANG_ADDON should not be changed nor be affected by flood control
    else
    {
        // send in universal language if player in .gmon mode (ignore spell effects)
        if (_player->isGameMaster())
            { lang = LANG_UNIVERSAL; }
        else
        {
            // send in universal language in two side iteration allowed mode
            if (sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHAT))
                { lang = LANG_UNIVERSAL; }
            else
            {
                switch (type)
                {
                    case CHAT_MSG_PARTY:
                    case CHAT_MSG_RAID:
                    case CHAT_MSG_RAID_LEADER:
                    case CHAT_MSG_RAID_WARNING:
                        // allow two side chat at group channel if two side group allowed
                        if (sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GROUP))
                            { lang = LANG_UNIVERSAL; }
                        break;
                    case CHAT_MSG_GUILD:
                    case CHAT_MSG_OFFICER:
                        // allow two side chat at guild channel if two side guild allowed
                        if (sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GUILD))
                            { lang = LANG_UNIVERSAL; }
                        break;
                }
            }

            // but overwrite it by SPELL_AURA_MOD_LANGUAGE auras (only single case used)
            Unit::AuraList const& ModLangAuras = _player->GetAurasByType(SPELL_AURA_MOD_LANGUAGE);
            if (!ModLangAuras.empty())
                { lang = ModLangAuras.front()->GetModifier()->m_miscvalue; }
        }

        if (type != CHAT_MSG_AFK && type != CHAT_MSG_DND)
        {
            if (!_player->CanSpeak())
            {
                std::string timeStr = secsToTimeString(m_muteTime - time(NULL));
                SendNotification(GetMangosString(LANG_WAIT_BEFORE_SPEAKING), timeStr.c_str());
                return;
            }

            GetPlayer()->UpdateSpeakTime();
        }
    }

    switch (type)
    {
        case CHAT_MSG_SAY:
        case CHAT_MSG_EMOTE:
        case CHAT_MSG_YELL:
        {
            std::string msg;
            recv_data >> msg;

            if (msg.empty())
                { break; }

            if (ChatHandler(this).ParseCommands(msg.c_str()))
                { break; }

            if (!processChatmessageFurtherAfterSecurityChecks(msg, lang))
                { return; }

            if (msg.empty())
                { break; }

            if (type == CHAT_MSG_SAY)
            {
#ifdef ENABLE_ELUNA
                if (!sEluna->OnChat(GetPlayer(), type, lang, msg))
                    return;
#endif /* ENABLE_ELUNA */
                 GetPlayer()->Say(msg, lang);
            }
            else if (type == CHAT_MSG_EMOTE)
            {
#ifdef ENABLE_ELUNA
                if (!sEluna->OnChat(GetPlayer(), type, LANG_UNIVERSAL, msg))
                    return;
#endif /* ENABLE_ELUNA */
                 GetPlayer()->TextEmote(msg);
            }
            else if (type == CHAT_MSG_YELL)
            {
#ifdef ENABLE_ELUNA
                if (!sEluna->OnChat(GetPlayer(), type, lang, msg))
                    return;
#endif /* ENABLE_ELUNA */
                 GetPlayer()->Yell(msg, lang);
            }
         } break;

        case CHAT_MSG_WHISPER:
        {
            std::string to, msg;
            recv_data >> to;
            recv_data >> msg;

            if (msg.empty())
                { break; }

            if (ChatHandler(this).ParseCommands(msg.c_str()))
                { break; }

            if (!processChatmessageFurtherAfterSecurityChecks(msg, lang))
                { return; }

            if (!normalizePlayerName(to))
            {
                SendPlayerNotFoundNotice(to);
                { break; }
            }

            Player* player = sObjectMgr.GetPlayer(to.c_str());
            uint32 tSecurity = GetSecurity();
            uint32 pSecurity = player ? player->GetSession()->GetSecurity() : SEC_PLAYER;
            if (!player || (tSecurity == SEC_PLAYER && pSecurity > SEC_PLAYER && !player->isAcceptWhispers()))
            {
                SendPlayerNotFoundNotice(to);
                return;
            }

            if (!sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHAT) && tSecurity == SEC_PLAYER && pSecurity == SEC_PLAYER)
            {
                if (GetPlayer()->GetTeam() != player->GetTeam())
                {
                    SendWrongFactionNotice();
                    return;
                }
            }

            // Used by Eluna
#ifdef ENABLE_ELUNA
            if (!sEluna->OnChat(GetPlayer(), type, lang, msg, player))
                return;
#endif /* ENABLE_ELUNA */
#ifdef ENABLE_PLAYERBOTS
            if (player->GetPlayerbotAI())
            {
                player->GetPlayerbotAI()->HandleCommand(type, msg, *GetPlayer());
                GetPlayer()->m_speakTime = 0;
                GetPlayer()->m_speakCount = 0;
            }
            else
#endif
                GetPlayer()->Whisper(msg, lang, player->GetObjectGuid());
        } break;

        case CHAT_MSG_PARTY:
        {
            std::string msg;
            recv_data >> msg;

            if (msg.empty())
                { break; }

            if (ChatHandler(this).ParseCommands(msg.c_str()))
                { break; }

            if (!processChatmessageFurtherAfterSecurityChecks(msg, lang))
                { return; }

            if (msg.empty())
                { break; }

            // if player is in battleground, he can not say to battleground members by /p
            Group* group = GetPlayer()->GetOriginalGroup();
            if (!group)
            {
                group = _player->GetGroup();
                if (!group || group->isBGGroup())
                    { return; }
            }

            // Used by Eluna
#ifdef ENABLE_ELUNA
            if (!sEluna->OnChat(GetPlayer(), type, lang, msg, group))
                return;
#endif /* ENABLE_ELUNA */

#ifdef ENABLE_PLAYERBOTS
            for (GroupReference* itr = group->GetFirstMember(); itr != NULL; itr = itr->next())
            {
                Player* player = itr->getSource();
                if (player && player->GetPlayerbotAI())
                {
                    player->GetPlayerbotAI()->HandleCommand(type, msg, *GetPlayer());
                    GetPlayer()->m_speakTime = 0;
                    GetPlayer()->m_speakCount = 0;
                }
            }
#endif

            WorldPacket data;
            ChatHandler::BuildChatPacket(data, ChatMsg(type), msg.c_str(), Language(lang), _player->GetChatTag(), _player->GetObjectGuid(), _player->GetName());
            group->BroadcastPacket(&data, false, group->GetMemberGroup(GetPlayer()->GetObjectGuid()));

            break;
        }
        case CHAT_MSG_GUILD:
        {
            std::string msg;
            recv_data >> msg;

            if (msg.empty())
                { break; }

            if (ChatHandler(this).ParseCommands(msg.c_str()))
                { break; }

            if (!processChatmessageFurtherAfterSecurityChecks(msg, lang))
                { return; }

            if (msg.empty())
                { break; }

            if (GetPlayer()->GetGuildId())
                if (Guild* guild = sGuildMgr.GetGuildById(GetPlayer()->GetGuildId()))
                {
                    // Used by Eluna
#ifdef ENABLE_ELUNA
                    if (!sEluna->OnChat(GetPlayer(), type, lang, msg, guild))
                        return;
#endif /* ENABLE_ELUNA */

                    guild->BroadcastToGuild(this, msg, lang == LANG_ADDON ? LANG_ADDON : LANG_UNIVERSAL);
                }

#ifdef ENABLE_PLAYERBOTS
                PlayerbotMgr *mgr = GetPlayer()->GetPlayerbotMgr();
                if (mgr)
                {
                    for (PlayerBotMap::const_iterator it = mgr->GetPlayerBotsBegin(); it != mgr->GetPlayerBotsEnd(); ++it)
                    {
                        Player* const bot = it->second;
                        if (bot->GetGuildId() == GetPlayer()->GetGuildId())
                            bot->GetPlayerbotAI()->HandleCommand(type, msg, *GetPlayer());
                    }
                }
#endif

            break;
        }
        case CHAT_MSG_OFFICER:
        {
            std::string msg;
            recv_data >> msg;

            if (msg.empty())
                { break; }

            if (ChatHandler(this).ParseCommands(msg.c_str()))
                { break; }

            if (!processChatmessageFurtherAfterSecurityChecks(msg, lang))
                { return; }

            if (msg.empty())
                { break; }

            if (GetPlayer()->GetGuildId())
                if (Guild* guild = sGuildMgr.GetGuildById(GetPlayer()->GetGuildId()))
                {
                    // Used by Eluna
#ifdef ENABLE_ELUNA
                    if (!sEluna->OnChat(GetPlayer(), type, lang, msg, guild))
                        return;
#endif /* ENABLE_ELUNA */

                    guild->BroadcastToOfficers(this, msg, lang == LANG_ADDON ? LANG_ADDON : LANG_UNIVERSAL);
                }

            break;
        }
        case CHAT_MSG_RAID:
        {
            std::string msg;
            recv_data >> msg;

            if (msg.empty())
                { break; }

            if (ChatHandler(this).ParseCommands(msg.c_str()))
                { break; }

            if (!processChatmessageFurtherAfterSecurityChecks(msg, lang))
                { return; }

            if (msg.empty())
                { break; }

            // if player is in battleground, he can not say to battleground members by /ra
            Group* group = GetPlayer()->GetOriginalGroup();
            if (!group)
            {
                group = GetPlayer()->GetGroup();
                if (!group || group->isBGGroup() || !group->isRaidGroup())
                    { return; }
            }

            // Used by Eluna
#ifdef ENABLE_ELUNA
            if (!sEluna->OnChat(GetPlayer(), type, lang, msg, group))
                return;
#endif /* ENABLE_ELUNA */

#ifdef ENABLE_PLAYERBOTS
            for (GroupReference* itr = group->GetFirstMember(); itr != NULL; itr = itr->next())
            {
                Player* player = itr->getSource();
                if (player && player->GetPlayerbotAI())
                {
                    player->GetPlayerbotAI()->HandleCommand(type, msg, *GetPlayer());
                    GetPlayer()->m_speakTime = 0;
                    GetPlayer()->m_speakCount = 0;
                }
            }
#endif

            WorldPacket data;
            ChatHandler::BuildChatPacket(data, CHAT_MSG_RAID, msg.c_str(), Language(lang), _player->GetChatTag(), _player->GetObjectGuid(), _player->GetName());
            group->BroadcastPacket(&data, false);
        } break;
        case CHAT_MSG_RAID_LEADER:
        {
            std::string msg;
            recv_data >> msg;

            if (msg.empty())
                { break; }

            if (ChatHandler(this).ParseCommands(msg.c_str()))
                { break; }

            if (!processChatmessageFurtherAfterSecurityChecks(msg, lang))
                { return; }

            if (msg.empty())
                { break; }

            // if player is in battleground, he can not say to battleground members by /ra
            Group* group = GetPlayer()->GetOriginalGroup();
            if (!group)
            {
                group = GetPlayer()->GetGroup();
                if (!group || group->isBGGroup() || !group->isRaidGroup() || !group->IsLeader(_player->GetObjectGuid()))
                    { return; }
            }

            // Used by Eluna
#ifdef ENABLE_ELUNA
            if (!sEluna->OnChat(GetPlayer(), type, lang, msg, group))
                return;
#endif /* ENABLE_ELUNA */

#ifdef ENABLE_PLAYERBOTS
            for (GroupReference* itr = group->GetFirstMember(); itr != NULL; itr = itr->next())
            {
                Player* player = itr->getSource();
                if (player && player->GetPlayerbotAI())
                {
                    player->GetPlayerbotAI()->HandleCommand(type, msg, *GetPlayer());
                    GetPlayer()->m_speakTime = 0;
                    GetPlayer()->m_speakCount = 0;
                }
            }
#endif

            WorldPacket data;
            ChatHandler::BuildChatPacket(data, CHAT_MSG_RAID_LEADER, msg.c_str(), Language(lang), _player->GetChatTag(), _player->GetObjectGuid(), _player->GetName());
            group->BroadcastPacket(&data, false);
        } break;

        case CHAT_MSG_RAID_WARNING:
        {
            std::string msg;
            recv_data >> msg;

            if (!processChatmessageFurtherAfterSecurityChecks(msg, lang))
                { return; }

            if (msg.empty())
                { break; }

            Group* group = GetPlayer()->GetGroup();
            if (!group || !group->isRaidGroup() ||
                !(group->IsLeader(GetPlayer()->GetObjectGuid()) || group->IsAssistant(GetPlayer()->GetObjectGuid())))
                { return; }

            // Used by Eluna
#ifdef ENABLE_ELUNA
            if (!sEluna->OnChat(GetPlayer(), type, lang, msg, group))
                return;
#endif /* ENABLE_ELUNA */

#ifdef ENABLE_PLAYERBOTS
            for (GroupReference* itr = group->GetFirstMember(); itr != NULL; itr = itr->next())
            {
                Player* player = itr->getSource();
                if (player && player->GetPlayerbotAI())
                {
                    player->GetPlayerbotAI()->HandleCommand(type, msg, *GetPlayer());
                    GetPlayer()->m_speakTime = 0;
                    GetPlayer()->m_speakCount = 0;
                }
            }
#endif

            WorldPacket data;
            // in battleground, raid warning is sent only to players in battleground - code is ok
            ChatHandler::BuildChatPacket(data, CHAT_MSG_RAID_WARNING, msg.c_str(), Language(lang), _player->GetChatTag(), _player->GetObjectGuid(), _player->GetName());
            group->BroadcastPacket(&data, false);
        } break;

        case CHAT_MSG_BATTLEGROUND:
        {
            std::string msg;
            recv_data >> msg;

            if (!processChatmessageFurtherAfterSecurityChecks(msg, lang))
                { return; }

            if (msg.empty())
                { break; }

            // battleground raid is always in Player->GetGroup(), never in GetOriginalGroup()
            Group* group = GetPlayer()->GetGroup();
            if (!group || !group->isBGGroup())
                { return; }

            // Used by Eluna
#ifdef ENABLE_ELUNA
            if (!sEluna->OnChat(GetPlayer(), type, lang, msg, group))
                return;
#endif /* ENABLE_ELUNA */

            WorldPacket data;
            ChatHandler::BuildChatPacket(data, CHAT_MSG_BATTLEGROUND, msg.c_str(), Language(lang), _player->GetChatTag(), _player->GetObjectGuid(), _player->GetName());
            group->BroadcastPacket(&data, false);
        } break;

        case CHAT_MSG_BATTLEGROUND_LEADER:
        {
            std::string msg;
            recv_data >> msg;

            if (!processChatmessageFurtherAfterSecurityChecks(msg, lang))
                { return; }

            if (msg.empty())
                { break; }

            // battleground raid is always in Player->GetGroup(), never in GetOriginalGroup()
            Group* group = GetPlayer()->GetGroup();
            if (!group || !group->isBGGroup() || !group->IsLeader(GetPlayer()->GetObjectGuid()))
                { return; }

            // Used by Eluna
#ifdef ENABLE_ELUNA
            if (!sEluna->OnChat(GetPlayer(), type, lang, msg, group))
                return;
#endif /* ENABLE_ELUNA */

            WorldPacket data;
            ChatHandler::BuildChatPacket(data, CHAT_MSG_BATTLEGROUND_LEADER, msg.c_str(), Language(lang), _player->GetChatTag(), _player->GetObjectGuid(), _player->GetName());
            group->BroadcastPacket(&data, false);
        } break;

        case CHAT_MSG_CHANNEL:
        {
            std::string channel, msg;
            recv_data >> channel;
            recv_data >> msg;

            if (!processChatmessageFurtherAfterSecurityChecks(msg, lang))
                { return; }

            if (msg.empty())
                { break; }

            if (ChannelMgr* cMgr = channelMgr(_player->GetTeam()))
            {
                if (Channel* chn = cMgr->GetChannel(channel, _player))
                {
                    // Used by Eluna
#ifdef ENABLE_ELUNA
                    if (!sEluna->OnChat(GetPlayer(), type, lang, msg, chn))
                        return;
#endif /* ENABLE_ELUNA */
#ifdef ENABLE_PLAYERBOTS
                    if (_player->GetPlayerbotMgr() && chn->GetFlags() & 0x18)
                    {
                        _player->GetPlayerbotMgr()->HandleCommand(type, msg);
                    }
                    sRandomPlayerbotMgr.HandleCommand(type, msg, *_player);
#endif /* ENABLE_PLAYERBOTS */
                    chn->Say(_player, msg.c_str(), lang); 
                }
            }

        } break;

        case CHAT_MSG_AFK:
        {
            std::string msg;
            recv_data >> msg;

            if (!_player->IsInCombat())
            {
                if (_player->isAFK())                       // Already AFK
                {
                    if (msg.empty())
                        { _player->ToggleAFK(); }               // Remove AFK
                    else
                        { _player->autoReplyMsg = msg; }        // Update message
                }
                else                                        // New AFK mode
                {
                    _player->autoReplyMsg = msg.empty() ? GetMangosString(LANG_PLAYER_AFK_DEFAULT) : msg;

                    if (_player->isDND())
                        { _player->ToggleDND(); }

                    _player->ToggleAFK();
                }
                // Used by Eluna
#ifdef ENABLE_ELUNA
                if (!sEluna->OnChat(GetPlayer(), type, lang, msg))
                    return;
#endif /* ENABLE_ELUNA */
            }
            break;
        }
        case CHAT_MSG_DND:
        {
            std::string msg;
            recv_data >> msg;

            if (_player->isDND())                           // Already DND
            {
                if (msg.empty())
                    { _player->ToggleDND(); }                   // Remove DND
                else
                    { _player->autoReplyMsg = msg; }            // Update message
            }
            else                                            // New DND mode
            {
                _player->autoReplyMsg = msg.empty() ? GetMangosString(LANG_PLAYER_DND_DEFAULT) : msg;

                if (_player->isAFK())
                    { _player->ToggleAFK(); }

                _player->ToggleDND();
            }
            // Used by Eluna
#ifdef ENABLE_ELUNA
            if (!sEluna->OnChat(GetPlayer(), type, lang, msg))
                return;
#endif /* ENABLE_ELUNA */

            break;
        }

        default:
            sLog.outError("CHAT: unknown message type %u, lang: %u", type, lang);
            break;
    }
}

void WorldSession::HandleEmoteOpcode(WorldPacket& recv_data)
{
    if (!GetPlayer()->IsAlive() || GetPlayer()->hasUnitState(UNIT_STAT_DIED))
        { return; }

    uint32 emote;
    recv_data >> emote;

    // Used by Eluna
#ifdef ENABLE_ELUNA
    sEluna->OnEmote(GetPlayer(), emote);
#endif /* ENABLE_ELUNA */
    GetPlayer()->HandleEmoteCommand(emote);
}

namespace MaNGOS
{
    class EmoteChatBuilder
    {
        public:
            EmoteChatBuilder(Player const& pl, uint32 text_emote, uint32 emote_num, Unit const* target)
                : i_player(pl), i_text_emote(text_emote), i_emote_num(emote_num), i_target(target) {}

            void operator()(WorldPacket& data, int32 loc_idx)
            {
                char const* nam = i_target ? i_target->GetNameForLocaleIdx(loc_idx) : NULL;
                uint32 namlen = (nam ? strlen(nam) : 0) + 1;

                data.Initialize(SMSG_TEXT_EMOTE, (20 + namlen));
                data << ObjectGuid(i_player.GetObjectGuid());
                data << uint32(i_text_emote);
                data << uint32(i_emote_num);
                data << uint32(namlen);
                if (namlen > 1)
                    { data.append(nam, namlen); }
                else
                    { data << uint8(0x00); }
            }

        private:
            Player const& i_player;
            uint32        i_text_emote;
            uint32        i_emote_num;
            Unit const*   i_target;
    };
}                                                           // namespace MaNGOS

void WorldSession::HandleTextEmoteOpcode(WorldPacket& recv_data)
{
    if (!GetPlayer()->IsAlive())
        { return; }

    if (!GetPlayer()->CanSpeak())
    {
        std::string timeStr = secsToTimeString(m_muteTime - time(NULL));
        SendNotification(GetMangosString(LANG_WAIT_BEFORE_SPEAKING), timeStr.c_str());
        return;
    }

    uint32 text_emote, emoteNum;
    ObjectGuid guid;

    recv_data >> text_emote;
    recv_data >> emoteNum;
    recv_data >> guid;

    // Used by Eluna
#ifdef ENABLE_ELUNA
    sEluna->OnTextEmote(GetPlayer(), text_emote, emoteNum, guid);
#endif /* ENABLE_ELUNA */

    EmotesTextEntry const* em = sEmotesTextStore.LookupEntry(text_emote);
    if (!em)
        { return; }

    uint32 emote_id = em->textid;

    switch (emote_id)
    {
        case EMOTE_STATE_SLEEP:
        case EMOTE_STATE_SIT:
        case EMOTE_STATE_KNEEL:
        case EMOTE_ONESHOT_NONE:
            break;
        default:
        {
            // in feign death state allowed only text emotes.
            if (GetPlayer()->hasUnitState(UNIT_STAT_DIED))
                { break; }

            GetPlayer()->HandleEmoteCommand(emote_id);
            break;
        }
    }

    Unit* unit = GetPlayer()->GetMap()->GetUnit(guid);

    MaNGOS::EmoteChatBuilder emote_builder(*GetPlayer(), text_emote, emoteNum, unit);
    MaNGOS::LocalizedPacketDo<MaNGOS::EmoteChatBuilder > emote_do(emote_builder);
    MaNGOS::CameraDistWorker<MaNGOS::LocalizedPacketDo<MaNGOS::EmoteChatBuilder > > emote_worker(GetPlayer(), sWorld.getConfig(CONFIG_FLOAT_LISTEN_RANGE_TEXTEMOTE), emote_do);
    Cell::VisitWorldObjects(GetPlayer(), emote_worker,  sWorld.getConfig(CONFIG_FLOAT_LISTEN_RANGE_TEXTEMOTE));

    // Send scripted event call
    if (unit && unit->GetTypeId() == TYPEID_UNIT && ((Creature*)unit)->AI())
        { ((Creature*)unit)->AI()->ReceiveEmote(GetPlayer(), text_emote); }
}

void WorldSession::HandleChatIgnoredOpcode(WorldPacket& recv_data)
{
    ObjectGuid iguid;
    // DEBUG_LOG("WORLD: Received opcode CMSG_CHAT_IGNORED");

    recv_data >> iguid;

    Player* player = sObjectMgr.GetPlayer(iguid);
    if (!player || !player->GetSession())
        { return; }

    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_IGNORED, _player->GetName(), LANG_UNIVERSAL, CHAT_TAG_NONE, _player->GetObjectGuid());
    player->GetSession()->SendPacket(&data);
}

void WorldSession::SendPlayerNotFoundNotice(const std::string &name)
{
    WorldPacket data(SMSG_CHAT_PLAYER_NOT_FOUND, name.size() + 1);
    data << name;
    SendPacket(&data);
}

void WorldSession::SendWrongFactionNotice()
{
    WorldPacket data(SMSG_CHAT_WRONG_FACTION, 0);
    SendPacket(&data);
}

void WorldSession::SendChatRestrictedNotice()
{
    WorldPacket data(SMSG_CHAT_RESTRICTED, 0);
    SendPacket(&data);
}
