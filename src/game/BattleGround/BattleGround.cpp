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

/**
 * @file BattleGround.cpp
 * @brief Core implementation of the battleground system.
 *
 * This file contains the implementation of the BattleGround base class, which provides:
 * - Battleground state management (waiting, in-progress, finished)
 * - Player management (joining, leaving, tracking)
 * - Event handling and broadcasting
 * - Reward distribution and scoring
 * - World state synchronization
 * - Team management and raid groups
 * - Creature and game object spawning
 */

#include "Object.h"
#include "Player.h"
#include "BattleGround.h"
#include "BattleGroundMgr.h"
#include "Creature.h"
#include "MapManager.h"
#include "Language.h"
#include "SpellAuras.h"
#include "World.h"
#include "Group.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "Mail.h"
#include "WorldPacket.h"
#include "Formulas.h"
#include "GridNotifiersImpl.h"
#include "Chat.h"
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */

namespace MaNGOS
{
    class BattleGroundChatBuilder
    {
        public:
            /// <summary>
            /// Initializes a new instance of the <see cref="BattleGroundChatBuilder"/> class.
            /// </summary>
            /// <param name="msgtype">The msgtype.</param>
            /// <param name="textId">The text id.</param>
            /// <param name="source">The source.</param>
            /// <param name="args">The args.</param>
            BattleGroundChatBuilder(ChatMsg msgtype, int32 textId, Player const* source, va_list* args = nullptr)
                : i_msgtype(msgtype), i_textId(textId), i_source(source), i_args(args) {}
            void operator()(WorldPacket& data, int32 loc_idx)
            {
                char const* text = sObjectMgr.GetMangosString(i_textId, loc_idx);

                ObjectGuid sourceGuid = i_source ? i_source->GetObjectGuid() : ObjectGuid();
                std::string sourceName = i_source ? i_source->GetName() : "";

                if (i_args)
                {
                    // we need copy va_list before use or original va_list will corrupted
                    va_list ap;
                    va_copy(ap, *i_args);

                    char str[2048];
                    vsnprintf(str, 2048, text, ap);
                    va_end(ap);

                    ChatHandler::BuildChatPacket(data, i_msgtype, &str[0], LANG_UNIVERSAL, CHAT_TAG_NONE, sourceGuid, sourceName.c_str());
                }
                else
                {
                    ChatHandler::BuildChatPacket(data, i_msgtype, text, LANG_UNIVERSAL, CHAT_TAG_NONE, sourceGuid, sourceName.c_str(), sourceGuid, sourceName.c_str());
                }
            }
        private:
            ChatMsg i_msgtype;
            int32 i_textId;
            Player const* i_source;
            va_list* i_args;
    };

    class BattleGroundYellBuilder
    {
        public:
            /// <summary>
            /// Initializes a new instance of the <see cref="BattleGroundYellBuilder"/> class.
            /// </summary>
            /// <param name="language">The language.</param>
            /// <param name="textId">The text id.</param>
            /// <param name="source">The source.</param>
            /// <param name="args">The args.</param>
            BattleGroundYellBuilder(Language language, int32 textId, Creature const* source, va_list* args = NULL)
                : i_language(language), i_textId(textId), i_source(source), i_args(args) {}
            void operator()(WorldPacket& data, int32 loc_idx)
            {
                char const* text = sObjectMgr.GetMangosString(i_textId, loc_idx);

                if (i_args)
                {
                    // we need copy va_list before use or original va_list will corrupted
                    va_list ap;
                    va_copy(ap, *i_args);

                    char str[2048];
                    vsnprintf(str, 2048, text, ap);
                    va_end(ap);

                    ChatHandler::BuildChatPacket(data, CHAT_MSG_MONSTER_YELL, &str[0], i_language, CHAT_TAG_NONE, i_source->GetObjectGuid(), i_source->GetName());
                }
                else
                {
                    ChatHandler::BuildChatPacket(data, CHAT_MSG_MONSTER_YELL, text, i_language, CHAT_TAG_NONE, i_source->GetObjectGuid(), i_source->GetName());
                }
            }
        private:
            Language i_language;
            int32 i_textId;
            Creature const* i_source;
            va_list* i_args;
    };

    class BattleGround2ChatBuilder
    {
        public:
            /// <summary>
            /// Initializes a new instance of the <see cref="BattleGround2ChatBuilder"/> class.
            /// </summary>
            /// <param name="msgtype">The msgtype.</param>
            /// <param name="textId">The text id.</param>
            /// <param name="source">The source.</param>
            /// <param name="arg1">The arg1.</param>
            /// <param name="arg2">The arg2.</param>
            BattleGround2ChatBuilder(ChatMsg msgtype, int32 textId, Player const* source, int32 arg1, int32 arg2)
                : i_msgtype(msgtype), i_textId(textId), i_source(source), i_arg1(arg1), i_arg2(arg2) {}
            void operator()(WorldPacket& data, int32 loc_idx)
            {
                char const* text = sObjectMgr.GetMangosString(i_textId, loc_idx);
                char const* arg1str = i_arg1 ? sObjectMgr.GetMangosString(i_arg1, loc_idx) : "";
                char const* arg2str = i_arg2 ? sObjectMgr.GetMangosString(i_arg2, loc_idx) : "";

                char str[2048];
                snprintf(str, 2048, text, arg1str, arg2str);

                ObjectGuid guid;
                if (i_source)
                {
                    guid = i_source->GetObjectGuid();
                }
                ChatHandler::BuildChatPacket(data, i_msgtype, str, LANG_UNIVERSAL, CHAT_TAG_NONE, guid);
            }
        private:
            ChatMsg i_msgtype;
            int32 i_textId;
            Player const* i_source;
            int32 i_arg1;
            int32 i_arg2;
    };

    class BattleGround2YellBuilder
    {
        public:
            /// <summary>
            /// Initializes a new instance of the <see cref="BattleGround2YellBuilder"/> class.
            /// </summary>
            /// <param name="language">The language.</param>
            /// <param name="textId">The text id.</param>
            /// <param name="source">The source.</param>
            /// <param name="arg1">The arg1.</param>
            /// <param name="arg2">The arg2.</param>
            BattleGround2YellBuilder(uint32 language, int32 textId, Creature const* source, int32 arg1, int32 arg2)
                : i_language(language), i_textId(textId), i_source(source), i_arg1(arg1), i_arg2(arg2) {}
            void operator()(WorldPacket& data, int32 loc_idx)
            {
                char const* text = sObjectMgr.GetMangosString(i_textId, loc_idx);
                char const* arg1str = i_arg1 ? sObjectMgr.GetMangosString(i_arg1, loc_idx) : "";
                char const* arg2str = i_arg2 ? sObjectMgr.GetMangosString(i_arg2, loc_idx) : "";

                char str[2048];
                snprintf(str, 2048, text, arg1str, arg2str);

                ChatHandler::BuildChatPacket(data, CHAT_MSG_MONSTER_YELL, str, LANG_UNIVERSAL, CHAT_TAG_NONE, i_source ? i_source->GetObjectGuid() : ObjectGuid(), i_source ? i_source->GetName() : "");
            }
        private:

            uint32 i_language;
            int32 i_textId;
            Creature const* i_source;
            int32 i_arg1;
            int32 i_arg2;
    };
} // namespace MaNGOS

template<class Do>

/**
 * @brief Broadcasts a worker function to all players in the battleground.
 *
 * @param _do The worker function.
 */
void BattleGround::BroadcastWorker(Do& _do)
{
    for (BattleGroundPlayerMap::const_iterator itr = m_Players.begin(); itr != m_Players.end(); ++itr)
    {
        if (Player* plr = sObjectAccessor.FindPlayer(itr->first))
        {
            _do(plr);
        }
    }
}

/**
 * @brief Constructor for BattleGround.
 */
BattleGround::BattleGround()
{
    m_TypeID = BattleGroundTypeId(0);
    m_Status = STATUS_NONE;
    m_ClientInstanceID = 0;
    m_EndTime = 0;
    m_BracketId = BG_BRACKET_ID_TEMPLATE;
    m_InvitedAlliance = 0;
    m_InvitedHorde = 0;
    m_Winner = TEAM_NONE;
    m_StartTime = 0;
    m_Events = 0;
    m_Name = "";
    m_LevelMin = 0;
    m_LevelMax = 0;
    m_InBGFreeSlotQueue = false;
    m_BuffChange = false;
    m_MaxPlayersPerTeam = 0;
    m_MaxPlayers = 0;
    m_MinPlayersPerTeam = 0;
    m_MinPlayers = 0;
    m_StartDelayTime = 0;
    m_MapId = 0;
    m_Map = NULL;
    m_startMaxDist = 0;
    m_validStartPositionTimer = 0;

    m_TeamStartLocX[TEAM_INDEX_ALLIANCE]   = 0;
    m_TeamStartLocX[TEAM_INDEX_HORDE] = 0;

    m_TeamStartLocY[TEAM_INDEX_ALLIANCE] = 0;
    m_TeamStartLocY[TEAM_INDEX_HORDE] = 0;

    m_TeamStartLocZ[TEAM_INDEX_ALLIANCE] = 0;
    m_TeamStartLocZ[TEAM_INDEX_HORDE] = 0;

    m_TeamStartLocO[TEAM_INDEX_ALLIANCE] = 0;
    m_TeamStartLocO[TEAM_INDEX_HORDE] = 0;

    m_BgRaids[TEAM_INDEX_ALLIANCE] = NULL;
    m_BgRaids[TEAM_INDEX_HORDE] = NULL;

    m_PlayersCount[TEAM_INDEX_ALLIANCE] = 0;
    m_PlayersCount[TEAM_INDEX_HORDE] = 0;

    m_TeamScores[TEAM_INDEX_ALLIANCE] = 0;
    m_TeamScores[TEAM_INDEX_HORDE] = 0;

    m_PrematureCountDown = false;
    m_PrematureCountDownTimer = 0;

    m_StartDelayTimes[BG_STARTING_EVENT_FIRST] = BG_START_DELAY_2M;
    m_StartDelayTimes[BG_STARTING_EVENT_SECOND] = BG_START_DELAY_1M;
    m_StartDelayTimes[BG_STARTING_EVENT_THIRD] = BG_START_DELAY_30S;
    m_StartDelayTimes[BG_STARTING_EVENT_FOURTH] = BG_START_DELAY_NONE;
    // we must set to some default existing values
    m_StartMessageIds[BG_STARTING_EVENT_FIRST] = 0;
    m_StartMessageIds[BG_STARTING_EVENT_SECOND] = LANG_BG_WS_START_ONE_MINUTE;
    m_StartMessageIds[BG_STARTING_EVENT_THIRD] = LANG_BG_WS_START_HALF_MINUTE;
    m_StartMessageIds[BG_STARTING_EVENT_FOURTH] = LANG_BG_WS_HAS_BEGUN;
}

/**
 * @brief Destructor for BattleGround.
 */
BattleGround::~BattleGround()
{
#ifdef ENABLE_ELUNA
    // sEluna->OnBGDestroy(this, GetTypeID(), GetInstanceID());
#endif /* ENABLE_ELUNA */

    // remove objects and creatures
    // (this is done automatically in mapmanager update, when the instance is reset after the reset time)
    sBattleGroundMgr.RemoveBattleGround(GetInstanceID(), GetTypeID());

    // skip template bgs as they were never added to visible bg list
    BattleGroundBracketId bracketId = GetBracketId();
    if (bracketId != BG_BRACKET_ID_TEMPLATE)
    {
        sBattleGroundMgr.DeleteClientVisibleInstanceId(GetTypeID(), bracketId, GetClientInstanceID());
    }

    // unload map
    // map can be null at bg destruction
    if (m_Map)
    {
        m_Map->SetUnload();
    }

    // remove from bg free slot queue
    this->RemoveFromBGFreeSlotQueue();

    for (BattleGroundScoreMap::const_iterator itr = m_PlayerScores.begin(); itr != m_PlayerScores.end(); ++itr)
    {
        delete itr->second;
    }
}

/**
 * @brief Updates the battleground.
 *
 * @param diff Time difference since last update.
 */
void BattleGround::Update(uint32 diff)
{
    if (!GetPlayersSize())
    {
        // BG is empty
        // if there are no players invited, delete BG
        // this will delete arena or bg object, where any player entered
        // [[   but if you use battleground object again (more battles possible to be played on 1 instance)
        //      then this condition should be removed and code:
        //      if (!GetInvitedCount(HORDE) && !GetInvitedCount(ALLIANCE))
        //          this->AddToFreeBGObjectsQueue(); // not yet implemented
        //      should be used instead of current
        // ]]
        // BattleGround Template instance can not be updated, because it would be deleted
        if (!GetInvitedCount(HORDE) && !GetInvitedCount(ALLIANCE))
        {
            delete this;
        }
        return;
    }

    // remove offline players from bg after 5 minutes
    if (!m_OfflineQueue.empty())
    {
        BattleGroundPlayerMap::iterator itr = m_Players.find(*(m_OfflineQueue.begin()));
        if (itr != m_Players.end())
        {
            if (itr->second.OfflineRemoveTime <= sWorld.GetGameTime())
            {
                RemovePlayerAtLeave(itr->first, true, true);// remove player from BG
                m_OfflineQueue.pop_front();                 // remove from offline queue
                // do not use itr for anything, because it is erased in RemovePlayerAtLeave()
            }
        }
    }

    /*********************************************************/
    /***           BATTLEGROUND BALANCE SYSTEM              ***/
    /*********************************************************/

    // if less than minimum players are in on one side, then start premature finish timer
    if (GetStatus() == STATUS_IN_PROGRESS && sBattleGroundMgr.GetPrematureFinishTime() && (GetPlayersCountByTeam(ALLIANCE) < GetMinPlayersPerTeam() || GetPlayersCountByTeam(HORDE) < GetMinPlayersPerTeam()))
    {
        if (!m_PrematureCountDown)
        {
            m_PrematureCountDown = true;
            m_PrematureCountDownTimer = sBattleGroundMgr.GetPrematureFinishTime();
        }
        else if (m_PrematureCountDownTimer < diff)
        {
            EndBattleGround(GetPrematureWinner());
            m_PrematureCountDown = false;
        }
        else if (!sBattleGroundMgr.isTesting())
        {
            uint32 newtime = m_PrematureCountDownTimer - diff;
            // announce every minute
            if (newtime > (MINUTE * IN_MILLISECONDS))
            {
                if (newtime / (MINUTE * IN_MILLISECONDS) != m_PrematureCountDownTimer / (MINUTE * IN_MILLISECONDS))
                {
                    PSendMessageToAll(LANG_BATTLEGROUND_PREMATURE_FINISH_WARNING, CHAT_MSG_SYSTEM, NULL, (uint32)(m_PrematureCountDownTimer / (MINUTE * IN_MILLISECONDS)));
                }
            }
            else
            {
                // announce every 15 seconds
                if (newtime / (15 * IN_MILLISECONDS) != m_PrematureCountDownTimer / (15 * IN_MILLISECONDS))
                {
                    PSendMessageToAll(LANG_BATTLEGROUND_PREMATURE_FINISH_WARNING_SECS, CHAT_MSG_SYSTEM, NULL, (uint32)(m_PrematureCountDownTimer / IN_MILLISECONDS));
                }
            }
            m_PrematureCountDownTimer = newtime;
        }
    }
    else if (m_PrematureCountDown)
    {
        m_PrematureCountDown = false;
    }

    /*********************************************************/
    /***           BATTLEGROUND STARTING SYSTEM             ***/
    /*********************************************************/

    if (GetStatus() == STATUS_WAIT_JOIN && GetPlayersSize())
    {
        float maxDist = GetStartMaxDist();
        if (maxDist > 0.0f)
        {
            if (m_validStartPositionTimer < diff)
            {
                for (BattleGroundPlayerMap::const_iterator itr = GetPlayers().begin(); itr != GetPlayers().end(); ++itr)
                {
                    if (Player* player = sObjectMgr.GetPlayer(itr->first))
                    {
                        float x, y, z, o;
                        GetTeamStartLoc(player->GetTeam(), x, y, z, o);
                        if (!player->IsWithinDist3d(x, y, z, maxDist))
                        {
                            player->TeleportTo(GetMapId(), x, y, z, o);
                        }
                    }
                }
                m_validStartPositionTimer = CHECK_PLAYER_POSITION_INVERVAL;
            }
            else
            {
                m_validStartPositionTimer -= diff;
            }
        }

        ModifyStartDelayTime(diff);

        if (!(m_Events & BG_STARTING_EVENT_1))
        {
            m_Events |= BG_STARTING_EVENT_1;

            StartingEventCloseDoors();
            SetStartDelayTime(m_StartDelayTimes[BG_STARTING_EVENT_FIRST]);
            // first start warning - 2 or 1 minute, only if defined
            if (m_StartMessageIds[BG_STARTING_EVENT_FIRST])
            {
                SendMessageToAll(m_StartMessageIds[BG_STARTING_EVENT_FIRST], CHAT_MSG_BG_SYSTEM_NEUTRAL);
            }
        }
        // After 1 minute or 30 seconds, warning is signaled
        else if (GetStartDelayTime() <= m_StartDelayTimes[BG_STARTING_EVENT_SECOND] && !(m_Events & BG_STARTING_EVENT_2))
        {
            m_Events |= BG_STARTING_EVENT_2;
            SendMessageToAll(m_StartMessageIds[BG_STARTING_EVENT_SECOND], CHAT_MSG_BG_SYSTEM_NEUTRAL);
        }
        // After 30 or 15 seconds, warning is signaled
        else if (GetStartDelayTime() <= m_StartDelayTimes[BG_STARTING_EVENT_THIRD] && !(m_Events & BG_STARTING_EVENT_3))
        {
            m_Events |= BG_STARTING_EVENT_3;
            SendMessageToAll(m_StartMessageIds[BG_STARTING_EVENT_THIRD], CHAT_MSG_BG_SYSTEM_NEUTRAL);
        }
        // Delay expired (after 2 or 1 minute)
        else if (GetStartDelayTime() <= 0 && !(m_Events & BG_STARTING_EVENT_4))
        {
            m_Events |= BG_STARTING_EVENT_4;

#ifdef ENABLE_ELUNA
            if (Eluna* e = this->GetBgMap()->GetEluna())
            {
                e->OnBGCreate(this, GetTypeID(), GetInstanceID());
            }
#endif /* ENABLE_ELUNA */

            StartingEventOpenDoors();

            SendMessageToAll(m_StartMessageIds[BG_STARTING_EVENT_FOURTH], CHAT_MSG_BG_SYSTEM_NEUTRAL);
            SetStatus(STATUS_IN_PROGRESS);
            SetStartDelayTime(m_StartDelayTimes[BG_STARTING_EVENT_FOURTH]);

            {
                PlaySoundToAll(SOUND_BG_START);

                // Announce BG starting
                if (sWorld.getConfig(CONFIG_BOOL_BATTLEGROUND_QUEUE_ANNOUNCER_START))
                {
                    sWorld.SendWorldText(LANG_BG_STARTED_ANNOUNCE_WORLD, GetName(), GetMinLevel(), GetMaxLevel());
                }
            }
        }
    }

    /*********************************************************/
    /***           BATTLEGROUND ENDING SYSTEM              ***/
    /*********************************************************/

    if (GetStatus() == STATUS_WAIT_LEAVE)
    {
        // Remove all players from battleground after 2 minutes
        m_EndTime -= diff;
        if (m_EndTime <= 0)
        {
            m_EndTime = 0;
            BattleGroundPlayerMap::iterator itr, next;
            for (itr = m_Players.begin(); itr != m_Players.end(); itr = next)
            {
                next = itr;
                ++next;
                // itr is erased here!
                RemovePlayerAtLeave(itr->first, true, true); // Remove player from BG
                // Do not change any battleground's private variables
            }
        }
    }

    // Update start time
    m_StartTime += diff;
}

/**
 * @brief Sets the team start location.
 *
 * @param team The team.
 * @param X The X coordinate.
 * @param Y The Y coordinate.
 * @param Z The Z coordinate.
 * @param O The orientation.
 */
void BattleGround::SetTeamStartLoc(Team team, float X, float Y, float Z, float O)
{
    PvpTeamIndex teamIdx = GetTeamIndexByTeamId(team);
    m_TeamStartLocX[teamIdx] = X;
    m_TeamStartLocY[teamIdx] = Y;
    m_TeamStartLocZ[teamIdx] = Z;
    m_TeamStartLocO[teamIdx] = O;
}

/**
 * @brief Sends a packet to all players in the battleground.
 *
 * @param packet The packet to send.
 */
void BattleGround::SendPacketToAll(WorldPacket* packet)
{
    for (BattleGroundPlayerMap::const_iterator itr = m_Players.begin(); itr != m_Players.end(); ++itr)
    {
        if (itr->second.OfflineRemoveTime)
        {
            continue;
        }

        if (Player* plr = sObjectMgr.GetPlayer(itr->first))
        {
            plr->GetSession()->SendPacket(packet);
        }
        else
        {
            sLog.outError("BattleGround:SendPacketToAll: %s not found!", itr->first.GetString().c_str());
        }
    }
}

/**
 * @brief Sends a packet to a specific team in the battleground.
 *
 * @param teamId The team ID.
 * @param packet The packet to send.
 * @param sender The sender of the packet.
 * @param self Whether to send the packet to the sender.
 */
void BattleGround::SendPacketToTeam(Team teamId, WorldPacket* packet, Player* sender, bool self)
{
    for (BattleGroundPlayerMap::const_iterator itr = m_Players.begin(); itr != m_Players.end(); ++itr)
    {
        if (itr->second.OfflineRemoveTime)
        {
            continue;
        }

        Player* plr = sObjectMgr.GetPlayer(itr->first);
        if (!plr)
        {
            sLog.outError("BattleGround:SendPacketToTeam: %s not found!", itr->first.GetString().c_str());
            continue;
        }

        if (!self && sender == plr)
        {
            continue;
        }

        Team team = itr->second.PlayerTeam;
        if (!team)
        {
            team = plr->GetTeam();
        }

        if (team == teamId)
        {
            plr->GetSession()->SendPacket(packet);
        }
    }
}

/**
 * @brief Plays a sound to all players in the battleground.
 *
 * @param SoundID The sound ID.
 */
void BattleGround::PlaySoundToAll(uint32 SoundID)
{
    WorldPacket data;
    sBattleGroundMgr.BuildPlaySoundPacket(&data, SoundID);
    SendPacketToAll(&data);
}

/**
 * @brief Plays a sound to a specific team in the battleground.
 *
 * @param SoundID The sound ID.
 * @param teamId The team ID.
 */
void BattleGround::PlaySoundToTeam(uint32 SoundID, Team teamId)
{
    WorldPacket data;

    for (BattleGroundPlayerMap::const_iterator itr = m_Players.begin(); itr != m_Players.end(); ++itr)
    {
        if (itr->second.OfflineRemoveTime)
        {
            continue;
        }

        Player* plr = sObjectMgr.GetPlayer(itr->first);
        if (!plr)
        {
            sLog.outError("BattleGround:PlaySoundToTeam: %s not found!", itr->first.GetString().c_str());
            continue;
        }

        Team team = itr->second.PlayerTeam;
        if (!team)
        {
            team = plr->GetTeam();
        }

        if (team == teamId)
        {
            sBattleGroundMgr.BuildPlaySoundPacket(&data, SoundID);
            plr->GetSession()->SendPacket(&data);
        }
    }
}

/**
 * @brief Casts a spell on a specific team in the battleground.
 *
 * @param SpellID The spell ID.
 * @param teamId The team ID.
 */
void BattleGround::CastSpellOnTeam(uint32 SpellID, Team teamId)
{
    for (BattleGroundPlayerMap::const_iterator itr = m_Players.begin(); itr != m_Players.end(); ++itr)
    {
        if (itr->second.OfflineRemoveTime)
        {
            continue;
        }

        Player* plr = sObjectMgr.GetPlayer(itr->first);

        if (!plr)
        {
            sLog.outError("BattleGround:CastSpellOnTeam: %s not found!", itr->first.GetString().c_str());
            continue;
        }

        Team team = itr->second.PlayerTeam;
        if (!team)
        {
            team = plr->GetTeam();
        }

        if (team == teamId)
        {
            plr->CastSpell(plr, SpellID, true);
        }
    }
}













/**
 * @brief Blocks the movement of the player.
 *
 * @param plr The player to block movement for.
 */
void BattleGround::BlockMovement(Player* plr)
{
    plr->SetClientControl(plr, 0);                          // movement disabled NOTE: the effect will be automatically removed by client when the player is teleported from the battleground, so no need to send with uint8(1) in RemovePlayerAtLeave()
}

/**
 * @brief Removes the player from the battleground when they leave.
 *
 * @param guid The GUID of the player.
 * @param Transport Whether to transport the player out of the battleground.
 * @param SendPacket Whether to send a packet to the player.
 */
void BattleGround::RemovePlayerAtLeave(ObjectGuid guid, bool Transport, bool SendPacket)
{
    Team team = GetPlayerTeam(guid);
    bool participant = false;
    // Remove from lists/maps
    BattleGroundPlayerMap::iterator itr = m_Players.find(guid);
    if (itr != m_Players.end())
    {
        UpdatePlayersCountByTeam(team, true);               // -1 player
        m_Players.erase(itr);
        // check if the player was a participant of the match, or only entered through gm command (goname)
        participant = true;
    }

    BattleGroundScoreMap::iterator itr2 = m_PlayerScores.find(guid);
    if (itr2 != m_PlayerScores.end())
    {
        delete itr2->second;                                // delete player's score
        m_PlayerScores.erase(itr2);
    }

    Player* plr = sObjectMgr.GetPlayer(guid);

    if (plr)
    {
        // should remove spirit of redemption
        if (plr->HasAuraType(SPELL_AURA_SPIRIT_OF_REDEMPTION))
        {
            plr->RemoveSpellsCausingAura(SPELL_AURA_MOD_SHAPESHIFT);
        }

        plr->RemoveSpellsCausingAura(SPELL_AURA_MOUNTED);

        if (!plr->IsAlive())                                // resurrect on exit
        {
            plr->ResurrectPlayer(1.0f);
            plr->SpawnCorpseBones();
        }
    }

    RemovePlayer(plr, guid);                                // BG subclass specific code

    if (participant) // if the player was a match participant, remove auras, calc rating, update queue
    {
        BattleGroundTypeId bgTypeId = GetTypeID();
        BattleGroundQueueTypeId bgQueueTypeId = BattleGroundMgr::BGQueueTypeId(GetTypeID());
        if (plr)
        {
            if (!team)
            {
                team = plr->GetTeam();
            }

            if (SendPacket)
            {
                WorldPacket data;
                sBattleGroundMgr.BuildBattleGroundStatusPacket(&data, this, plr->GetBattleGroundQueueIndex(bgQueueTypeId), STATUS_NONE, 0, 0);
                plr->GetSession()->SendPacket(&data);
            }

            // this call is important, because player, when joins to battleground, this method is not called, so it must be called when leaving bg
            plr->RemoveBattleGroundQueueId(bgQueueTypeId);
        }

        // remove from raid group if player is member
        if (Group* group = GetBgRaid(team))
        {
            if (!group->RemoveMember(guid, 0))              // group was disbanded
            {
                SetBgRaid(team, NULL);
                delete group;
            }
        }
        DecreaseInvitedCount(team);
        // we should update battleground queue, but only if bg isn't ending
        if (GetStatus() < STATUS_WAIT_LEAVE)
        {
            // a player has left the battleground, so there are free slots -> add to queue
            AddToBGFreeSlotQueue();
            sBattleGroundMgr.ScheduleQueueUpdate(bgQueueTypeId, bgTypeId, GetBracketId());
        }

        // Let others know
        WorldPacket data;
        sBattleGroundMgr.BuildPlayerLeftBattleGroundPacket(&data, guid);
        SendPacketToTeam(team, &data, plr, false);
    }

    if (plr)
    {
        // Do next only if found in battleground
        plr->SetBattleGroundId(0, BATTLEGROUND_TYPE_NONE);  // We're not in BG.
        // reset destination bg team
        plr->SetBGTeam(TEAM_NONE);

        if (Transport)
        {
            plr->TeleportToBGEntryPoint();
        }

        DETAIL_LOG("BATTLEGROUND: Removed player %s from BattleGround.", plr->GetName());
    }

    // battleground object will be deleted next BattleGround::Update() call
}

/**
 * @brief Resets the battleground when no players remain.
 */
void BattleGround::Reset()
{
    SetWinner(TEAM_NONE);
    SetStatus(STATUS_WAIT_QUEUE);
    SetStartTime(0);
    SetEndTime(0);

    m_Events = 0;

    // door-event2 is always 0
    m_ActiveEvents[BG_EVENT_DOOR] = 0;

    if (m_InvitedAlliance > 0 || m_InvitedHorde > 0)
    {
        sLog.outError("BattleGround system: bad counter, m_InvitedAlliance: %d, m_InvitedHorde: %d", m_InvitedAlliance, m_InvitedHorde);
    }

    m_InvitedAlliance = 0;
    m_InvitedHorde = 0;
    m_InBGFreeSlotQueue = false;

    m_Players.clear();

    for (BattleGroundScoreMap::const_iterator itr = m_PlayerScores.begin(); itr != m_PlayerScores.end(); ++itr)
    {
        delete itr->second;
    }
    m_PlayerScores.clear();
}

/**
 * @brief Starts the battleground.
 */
void BattleGround::StartBattleGround()
{
    SetStartTime(0);

    // add BG to free slot queue
    AddToBGFreeSlotQueue();

    // add bg to update list
    // This must be done here, because we need to have already invited some players when first BG::Update() method is executed
    // and it doesn't matter if we call StartBattleGround() more times, because m_BattleGrounds is a map and instance id never changes
    sBattleGroundMgr.AddBattleGround(GetInstanceID(), GetTypeID(), this);

#ifdef ENABLE_ELUNA
    if (Eluna* e = GetBgMap()->GetEluna())
    {
        e->OnBGCreate(this, GetTypeID(), GetInstanceID());
    }
#endif /* ENABLE_ELUNA */
}

/**
 * @brief Adds a player to the battleground.
 *
 * @param plr The player to add.
 */
void BattleGround::AddPlayer(Player* plr)
{
    // remove afk from player
    if (plr->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_AFK))
    {
        plr->ToggleAFK();
    }

    // score struct must be created in inherited class

    ObjectGuid guid = plr->GetObjectGuid();
    Team team = plr->GetBGTeam();

    BattleGroundPlayer bp;
    bp.OfflineRemoveTime = 0;
    bp.PlayerTeam = team;

    // Add to list/maps
    m_Players[guid] = bp;

    bool const isInBattleground = IsPlayerInBattleGround(guid);
    if (!isInBattleground)
    {
        UpdatePlayersCountByTeam(team, false); // +1 player
    }

    WorldPacket data;
    sBattleGroundMgr.BuildPlayerJoinedBattleGroundPacket(&data, plr);
    SendPacketToTeam(team, &data, plr, false);

    // setup BG group membership
    PlayerAddedToBGCheckIfBGIsRunning(plr);
    AddOrSetPlayerToCorrectBgGroup(plr, guid, team);

    // Log
    DETAIL_LOG("BATTLEGROUND: Player %s joined the battle.", plr->GetName());
}

/**
 * @brief Adds a player to their team's battleground group or sets their correct group if already in a group.
 *
 * @param plr The player to add or set.
 * @param plr_guid The GUID of the player.
 * @param team The team of the player.
 */
void BattleGround::AddOrSetPlayerToCorrectBgGroup(Player* plr, ObjectGuid plr_guid, Team team)
{
    if (Group* group = GetBgRaid(team))                     // raid already exist
    {
        if (group->IsMember(plr_guid))
        {
            uint8 subgroup = group->GetMemberGroup(plr_guid);
            plr->SetBattleGroundRaid(group, subgroup);
        }
        else
        {
            group->AddMember(plr_guid, plr->GetName());
            if (Group* originalGroup = plr->GetOriginalGroup())
            {
                if (originalGroup->IsLeader(plr_guid))
                {
                    group->ChangeLeader(plr_guid);
                }
            }
        }
    }
    else                                                    // first player joined
    {
        group = new Group;
        SetBgRaid(team, group);
        group->Create(plr_guid, plr->GetName());
    }
}

/**
 * @brief Handles player login to a running battleground.
 *
 * @param player The player logging in.
 */
void BattleGround::EventPlayerLoggedIn(Player* player)
{
    ObjectGuid playerGuid = player->GetObjectGuid();

    // player is correct pointer
    for (OfflineQueue::iterator itr = m_OfflineQueue.begin(); itr != m_OfflineQueue.end(); ++itr)
    {
        if (*itr == playerGuid)
        {
            m_OfflineQueue.erase(itr);
            break;
        }
    }
    m_Players[playerGuid].OfflineRemoveTime = 0;
    PlayerAddedToBGCheckIfBGIsRunning(player);
    // if battleground is starting, then add preparation aura
    // we don't have to do that, because preparation aura isn't removed when player logs out
}

/**
 * @brief Handles player logout from a running battleground.
 *
 * @param player The player logging out.
 */
void BattleGround::EventPlayerLoggedOut(Player* player)
{
    // player is correct pointer, it is checked in WorldSession::LogoutPlayer()
    m_OfflineQueue.push_back(player->GetObjectGuid());
    m_Players[player->GetObjectGuid()].OfflineRemoveTime = sWorld.GetGameTime() + MAX_OFFLINE_TIME;
    if (GetStatus() == STATUS_IN_PROGRESS)
    {
        // drop flag and handle other cleanups
        RemovePlayer(player, player->GetObjectGuid());
    }
}

/**
 * @brief Adds the battleground to the free slot queue.
 * This method should be called only once.
 */
void BattleGround::AddToBGFreeSlotQueue()
{
    // make sure to add only once
    if (!m_InBGFreeSlotQueue)
    {
        sBattleGroundMgr.BGFreeSlotQueue[m_TypeID].push_front(this);
        m_InBGFreeSlotQueue = true;
    }
}

/**
 * @brief Removes the battleground from the free slot queue.
 * This method must be called when deleting the battleground.
 */
void BattleGround::RemoveFromBGFreeSlotQueue()
{
    // set to be able to re-add if needed
    m_InBGFreeSlotQueue = false;
    BGFreeSlotQueueType& bgFreeSlot = sBattleGroundMgr.BGFreeSlotQueue[m_TypeID];
    for (BGFreeSlotQueueType::iterator itr = bgFreeSlot.begin(); itr != bgFreeSlot.end(); ++itr)
    {
        if ((*itr)->GetInstanceID() == GetInstanceID())
        {
            bgFreeSlot.erase(itr);
            return;
        }
    }
}

/**
 * @brief Gets the number of free slots for a team.
 *
 * @param team The team.
 * @returns The number of free slots for the team.
 */
uint32 BattleGround::GetFreeSlotsForTeam(Team team) const
{
    // return free slot count to MaxPlayerPerTeam
    if (GetStatus() == STATUS_WAIT_JOIN || GetStatus() == STATUS_IN_PROGRESS)
    {
        return (GetInvitedCount(team) < GetMaxPlayersPerTeam()) ? GetMaxPlayersPerTeam() - GetInvitedCount(team) : 0;
    }

    return 0;
}

/**
 * @brief Determines whether the battleground has free slots.
 *
 * @returns True if the battleground has free slots, false otherwise.
 */
bool BattleGround::HasFreeSlots() const
{
    return GetPlayersSize() < GetMaxPlayers();
}

/**
 * @brief Updates the player's score.
 *
 * @param Source The player whose score is being updated.
 * @param type The type of score to update.
 * @param value The value to update the score by.
 */
void BattleGround::UpdatePlayerScore(Player* Source, uint32 type, uint32 value)
{
    // this procedure is called from virtual function implemented in bg subclass
    BattleGroundScoreMap::const_iterator itr = m_PlayerScores.find(Source->GetObjectGuid());

    if (itr == m_PlayerScores.end())                        // player not found...
    {
        return;
    }

    switch (type)
    {
        case SCORE_KILLING_BLOWS:                           // Killing blows
            itr->second->KillingBlows += value;
            break;
        case SCORE_DEATHS:                                  // Deaths
            itr->second->Deaths += value;
            break;
        case SCORE_HONORABLE_KILLS:                         // Honorable kills
            itr->second->HonorableKills += value;
            break;
        case SCORE_BONUS_HONOR:                             // Honor bonus
            // reward honor instantly
            if (Source->AddHonorCP(value, HONORABLE, 0, 0))
            {
                itr->second->BonusHonor += value;
            }
            break;
        default:
            sLog.outError("BattleGround: Unknown player score type %u", type);
            break;
    }
}











/**
 * @brief Sends a message to all players in the battleground.
 *
 * @param entry The entry ID of the message.
 * @param type The type of chat message.
 * @param source The source player of the message.
 */
void BattleGround::SendMessageToAll(int32 entry, ChatMsg type, Player const* source)
{
    MaNGOS::BattleGroundChatBuilder bg_builder(type, entry, source);
    MaNGOS::LocalizedPacketDo<MaNGOS::BattleGroundChatBuilder> bg_do(bg_builder);
    BroadcastWorker(bg_do);
}

/**
 * @brief Sends a yell to all players in the battleground.
 *
 * @param entry The entry ID of the yell.
 * @param language The language of the yell.
 * @param guid The GUID of the creature yelling.
 */
void BattleGround::SendYellToAll(int32 entry, uint32 language, ObjectGuid guid)
{
    Creature* source = GetBgMap()->GetCreature(guid);
    if (!source)
    {
        return;
    }
    MaNGOS::BattleGroundYellBuilder bg_builder(Language(language), entry, source);
    MaNGOS::LocalizedPacketDo<MaNGOS::BattleGroundYellBuilder> bg_do(bg_builder);
    BroadcastWorker(bg_do);
}

/**
 * @brief Sends a formatted message to all players in the battleground.
 *
 * @param entry The entry ID of the message.
 * @param type The type of chat message.
 * @param source The source player of the message.
 * @param ... The arguments for the formatted message.
 */
void BattleGround::PSendMessageToAll(int32 entry, ChatMsg type, Player const* source, ...)
{
    va_list ap;
    va_start(ap, source);

    MaNGOS::BattleGroundChatBuilder bg_builder(type, entry, source, &ap);
    MaNGOS::LocalizedPacketDo<MaNGOS::BattleGroundChatBuilder> bg_do(bg_builder);
    BroadcastWorker(bg_do);

    va_end(ap);
}

/**
 * @brief Sends a formatted message with two arguments to all players in the battleground.
 *
 * @param entry The entry ID of the message.
 * @param type The type of chat message.
 * @param source The source player of the message.
 * @param arg1 The first argument for the formatted message.
 * @param arg2 The second argument for the formatted message.
 */
void BattleGround::SendMessage2ToAll(int32 entry, ChatMsg type, Player const* source, int32 arg1, int32 arg2)
{
    MaNGOS::BattleGround2ChatBuilder bg_builder(type, entry, source, arg1, arg2);
    MaNGOS::LocalizedPacketDo<MaNGOS::BattleGround2ChatBuilder> bg_do(bg_builder);
    BroadcastWorker(bg_do);
}

/**
 * @brief Sends a formatted yell with two arguments to all players in the battleground.
 *
 * @param entry The entry ID of the yell.
 * @param language The language of the yell.
 * @param guid The GUID of the creature yelling.
 * @param arg1 The first argument for the formatted yell.
 * @param arg2 The second argument for the formatted yell.
 */
void BattleGround::SendYell2ToAll(int32 entry, uint32 language, ObjectGuid guid, int32 arg1, int32 arg2)
{
    Creature* source = GetBgMap()->GetCreature(guid);
    if (!source)
    {
        return;
    }
    MaNGOS::BattleGround2YellBuilder bg_builder(language, entry, source, arg1, arg2);
    MaNGOS::LocalizedPacketDo<MaNGOS::BattleGround2YellBuilder> bg_do(bg_builder);
    BroadcastWorker(bg_do);
}

/**
 * @brief Ends the battleground immediately.
 */
void BattleGround::EndNow()
{
    RemoveFromBGFreeSlotQueue();
    SetStatus(STATUS_WAIT_LEAVE);
    SetEndTime(0);
}

/**
 * @brief Handles the triggering of a buff in the battleground.
 *
 * @param go_guid The GUID of the game object representing the buff.
 */
void BattleGround::HandleTriggerBuff(ObjectGuid go_guid)
{
    GameObject* obj = GetBgMap()->GetGameObject(go_guid);
    if (!obj || obj->GetGoType() != GAMEOBJECT_TYPE_TRAP || !obj->isSpawned())
    {
        return;
    }

    obj->SetLootState(GO_JUST_DEACTIVATED);             // can be despawned or destroyed
    return;
}

/**
 * @brief Handles the event of a player being killed in the battleground.
 *
 * @param player The player who was killed.
 * @param killer The player who killed the other player.
 */
void BattleGround::HandleKillPlayer(Player* player, Player* killer)
{
    // add +1 deaths
    UpdatePlayerScore(player, SCORE_DEATHS, 1);

    // add +1 kills to group and +1 killing_blows to killer
    if (killer)
    {
        UpdatePlayerScore(killer, SCORE_HONORABLE_KILLS, 1);
        UpdatePlayerScore(killer, SCORE_KILLING_BLOWS, 1);

        for (BattleGroundPlayerMap::const_iterator itr = m_Players.begin(); itr != m_Players.end(); ++itr)
        {
            Player* plr = sObjectMgr.GetPlayer(itr->first);

            if (!plr || plr == killer)
            {
                continue;
            }

            if (plr->GetTeam() == killer->GetTeam() && plr->IsAtGroupRewardDistance(player))
            {
                UpdatePlayerScore(plr, SCORE_HONORABLE_KILLS, 1);
            }
        }
    }

    player->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SKINNABLE);
}

/**
 * @brief Returns the player's team based on battleground player info.
 * Used mainly in same faction arena matches.
 *
 * @param guid The GUID of the player.
 * @returns The team of the player.
 */
Team BattleGround::GetPlayerTeam(ObjectGuid guid)
{
    BattleGroundPlayerMap::const_iterator itr = m_Players.find(guid);
    if (itr != m_Players.end())
    {
        return itr->second.PlayerTeam;
    }
    return TEAM_NONE;
}

/**
 * @brief Determines whether a player is in the battleground.
 *
 * @param guid The GUID of the player.
 * @returns True if the player is in the battleground, false otherwise.
 */
bool BattleGround::IsPlayerInBattleGround(ObjectGuid guid)
{
    BattleGroundPlayerMap::const_iterator itr = m_Players.find(guid);
    if (itr != m_Players.end())
    {
        return true;
    }
    return false;
}

/**
 * @brief Checks if the battleground is running and updates the player's status accordingly.
 *
 * @param plr The player to check and update.
 */
void BattleGround::PlayerAddedToBGCheckIfBGIsRunning(Player* plr)
{
    if (GetStatus() != STATUS_WAIT_LEAVE)
    {
        return;
    }

    WorldPacket data;
    BattleGroundQueueTypeId bgQueueTypeId = BattleGroundMgr::BGQueueTypeId(GetTypeID());

    BlockMovement(plr);

    sBattleGroundMgr.BuildPvpLogDataPacket(&data, this);
    plr->GetSession()->SendPacket(&data);

    sBattleGroundMgr.BuildBattleGroundStatusPacket(&data, this, plr->GetBattleGroundQueueIndex(bgQueueTypeId), STATUS_IN_PROGRESS, GetEndTime(), GetStartTime());
    plr->GetSession()->SendPacket(&data);
}

/**
 * @brief Gets the count of alive players by team.
 *
 * @param team The team to get the count for.
 * @returns The count of alive players in the specified team.
 */
uint32 BattleGround::GetAlivePlayersCountByTeam(Team team) const
{
    int count = 0;
    for (BattleGroundPlayerMap::const_iterator itr = m_Players.begin(); itr != m_Players.end(); ++itr)
    {
        if (itr->second.PlayerTeam == team)
        {
            Player* pl = sObjectMgr.GetPlayer(itr->first);
            if (pl && pl->IsAlive())
            {
                ++count;
            }
        }
    }
    return count;
}

/**
 * @brief Sets the battleground raid group for a team.
 *
 * @param team The team to set the raid group for.
 * @param bg_raid The raid group to set.
 */
void BattleGround::SetBgRaid(Team team, Group* bg_raid)
{
    Group*& old_raid = m_BgRaids[GetTeamIndexByTeamId(team)];

    if (old_raid)
    {
        old_raid->SetBattlegroundGroup(NULL);
    }

    if (bg_raid)
    {
        bg_raid->SetBattlegroundGroup(this);
    }

    old_raid = bg_raid;
}

/**
 * @brief Gets the closest graveyard for a player.
 *
 * @param player The player to get the closest graveyard for.
 * @returns The closest graveyard entry.
 */
WorldSafeLocsEntry const* BattleGround::GetClosestGraveYard(Player* player)
{
    return sObjectMgr.GetClosestGraveYard(player->GetPositionX(), player->GetPositionY(), player->GetPositionZ(), player->GetMapId(), player->GetTeam());
}

/**
 * @brief Gets the winner in case of a premature finish of the battleground.
 * Different battlegrounds may have different criteria for choosing the winner besides simple player accounting.
 *
 * @returns The winner team.
 */
Team BattleGround::GetPrematureWinner()
{
    uint32 hordePlayers = GetPlayersCountByTeam(HORDE);
    uint32 alliancePlayers = GetPlayersCountByTeam(ALLIANCE);

    if (alliancePlayers > hordePlayers)
    {
        return ALLIANCE;
    }

    if (hordePlayers > alliancePlayers)
    {
        return HORDE;
    }

    return TEAM_NONE;
}
