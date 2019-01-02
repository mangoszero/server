/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2019  MaNGOS project <https://getmangos.eu>
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

#ifndef MANGOS_H_BATTLEGROUNDWS
#define MANGOS_H_BATTLEGROUNDWS

#include "BattleGround.h"

#define BG_WS_MAX_TEAM_SCORE      3
#define BG_WS_FLAG_RESPAWN_TIME   (23*IN_MILLISECONDS)
#define BG_WS_FLAG_DROP_TIME      (10*IN_MILLISECONDS)

/**
 * @brief
 *
 */
enum BG_WS_Sound
{
    BG_WS_SOUND_FLAG_CAPTURED_ALLIANCE  = 8173,
    BG_WS_SOUND_FLAG_CAPTURED_HORDE     = 8213,
    BG_WS_SOUND_FLAG_PLACED             = 8232,
    BG_WS_SOUND_FLAG_RETURNED           = 8192,
    BG_WS_SOUND_HORDE_FLAG_PICKED_UP    = 8212,
    BG_WS_SOUND_ALLIANCE_FLAG_PICKED_UP = 8174,
    BG_WS_SOUND_FLAGS_RESPAWNED         = 8232
};

/**
 * @brief
 *
 */
enum BG_WS_SpellId
{
    BG_WS_SPELL_WARSONG_FLAG            = 23333,
    BG_WS_SPELL_WARSONG_FLAG_DROPPED    = 23334,
    BG_WS_SPELL_SILVERWING_FLAG         = 23335,
    BG_WS_SPELL_SILVERWING_FLAG_DROPPED = 23336
};

/**
 * @brief
 *
 */
enum BG_WS_WorldStates
{
    BG_WS_FLAG_UNK_ALLIANCE       = 1545,
    BG_WS_FLAG_UNK_HORDE          = 1546,
    // BG_FLAG_UNK                   = 1547,
    BG_WS_FLAG_CAPTURES_ALLIANCE  = 1581,
    BG_WS_FLAG_CAPTURES_HORDE     = 1582,
    BG_WS_FLAG_CAPTURES_MAX       = 1601,
    BG_WS_FLAG_STATE_HORDE        = 2338,
    BG_WS_FLAG_STATE_ALLIANCE     = 2339
};

/**
 * @brief
 *
 */
enum BG_WS_FlagState
{
    BG_WS_FLAG_STATE_ON_BASE      = 0,
    BG_WS_FLAG_STATE_WAIT_RESPAWN = 1,
    BG_WS_FLAG_STATE_ON_PLAYER    = 2,
    BG_WS_FLAG_STATE_ON_GROUND    = 3
};

/**
 * @brief
 *
 */
enum BG_WS_Graveyards
{
    WS_GRAVEYARD_FLAGROOM_ALLIANCE = 769,
    WS_GRAVEYARD_FLAGROOM_HORDE    = 770,
    WS_GRAVEYARD_MAIN_ALLIANCE     = 771,
    WS_GRAVEYARD_MAIN_HORDE        = 772
};

/**
 * @brief
 *
 */
class BattleGroundWGScore : public BattleGroundScore
{
    public:
        /**
         * @brief
         *
         */
        BattleGroundWGScore() : FlagCaptures(0), FlagReturns(0) {};
        /**
         * @brief
         *
         */
        virtual ~BattleGroundWGScore() {};

        uint32 GetAttr1() const { return FlagCaptures; }
        uint32 GetAttr2() const { return FlagReturns; }

        uint32 FlagCaptures; /**< TODO */
        uint32 FlagReturns; /**< TODO */
};

/**
 * @brief
 *
 */
enum BG_WS_Events
{
    WS_EVENT_FLAG_A               = 0,
    WS_EVENT_FLAG_H               = 1,
    // spiritguides will spawn (same moment, like WS_EVENT_DOOR_OPEN)
    WS_EVENT_SPIRITGUIDES_SPAWN   = 2
};

// Honor granted depending on player's level
const uint32 BG_WSG_FlagCapturedHonor[MAX_BATTLEGROUND_BRACKETS] = {48, 82, 136, 226, 378, 396}; /**< TODO */
const uint32 BG_WSG_WinMatchHonor[MAX_BATTLEGROUND_BRACKETS] = {24, 41, 68, 113, 189, 198}; /**< TODO */

/**
 * @brief
 *
 */
class BattleGroundWS : public BattleGround
{
        friend class BattleGroundMgr;

    public:
        /**
         * @brief Construction
         *
         */
        BattleGroundWS();
        /**
         * @brief
         *
         * @param diff
         */
        void Update(uint32 diff) override;

        /**
         * @brief inherited from BattlegroundClass
         *
         * @param plr
         */
        virtual void AddPlayer(Player* plr) override;
        /**
         * @brief
         *
         */
        virtual void StartingEventOpenDoors() override;

        /**
         * @brief BG Flags
         *
         * @return ObjectGuid
         */
        ObjectGuid GetAllianceFlagCarrierGuid() const { return m_flagCarrierAlliance; }
        /**
         * @brief
         *
         * @return ObjectGuid
         */
        ObjectGuid GetHordeFlagCarrierGuid() const { return m_flagCarrierHorde; }

        /**
         * @brief
         *
         * @param guid
         */
        void SetAllianceFlagCarrier(ObjectGuid guid) { m_flagCarrierAlliance = guid; }
        /**
         * @brief
         *
         * @param guid
         */
        void SetHordeFlagCarrier(ObjectGuid guid) { m_flagCarrierHorde = guid; }

        /**
         * @brief
         *
         */
        void ClearAllianceFlagCarrier() { m_flagCarrierAlliance.Clear(); }
        /**
         * @brief
         *
         */
        void ClearHordeFlagCarrier() { m_flagCarrierHorde.Clear(); }

        /**
         * @brief
         *
         * @return bool
         */
        bool IsAllianceFlagPickedUp() const { return !m_flagCarrierAlliance.IsEmpty(); }
        /**
         * @brief
         *
         * @return bool
         */
        bool IsHordeFlagPickedUp() const { return !m_flagCarrierHorde.IsEmpty(); }

        /**
         * @brief
         *
         * @param team
         * @param captured
         */
        void RespawnFlag(Team team, bool captured);
        /**
         * @brief
         *
         * @param team
         */
        void RespawnDroppedFlag(Team team);
        /**
         * @brief
         *
         * @param team
         * @return uint8
         */
        uint8 GetFlagState(Team team) { return m_FlagState[GetTeamIndexByTeamId(team)]; }

        /**
         * @brief Battleground Events
         *
         * @param source
         */
        virtual void EventPlayerDroppedFlag(Player* source) override;
        /**
         * @brief
         *
         * @param source
         * @param target_obj
         */
        virtual void EventPlayerClickedOnFlag(Player* source, GameObject* target_obj) override;
        /**
         * @brief
         *
         * @param source
         */
        virtual void EventPlayerCapturedFlag(Player* source) override;

        /**
         * @brief
         *
         * @param plr
         * @param guid
         */
        void RemovePlayer(Player* plr, ObjectGuid guid) override;
        /**
         * @brief
         *
         * @param source
         * @param trigger
         */
        bool HandleAreaTrigger(Player* source, uint32 trigger) override;
        /**
         * @brief
         *
         * @param player
         * @param killer
         */
        void HandleKillPlayer(Player* player, Player* killer) override;
        /**
         * @brief
         *
         */
        virtual void Reset() override;
        /**
         * @brief
         *
         * @param winner
         */
        void EndBattleGround(Team winner) override;
        /**
         * @brief
         *
         * @param player
         * @return const WorldSafeLocsEntry
         */
        virtual WorldSafeLocsEntry const* GetClosestGraveYard(Player* player) override;

        /**
         * @brief
         *
         * @param team
         * @param value
         */
        void UpdateFlagState(Team team, uint32 value);
        /**
         * @brief
         *
         * @param team
         */
        void UpdateTeamScore(Team team);
        /**
         * @brief
         *
         * @param source
         * @param type
         * @param value
         */
        void UpdatePlayerScore(Player* source, uint32 type, uint32 value) override;
        /**
         * @brief
         *
         * @param guid
         * @param team
         */
        void SetDroppedFlagGuid(ObjectGuid guid, Team team)  { m_DroppedFlagGuid[GetTeamIndexByTeamId(team)] = guid;}
        /**
         * @brief
         *
         * @param team
         */
        void ClearDroppedFlagGuid(Team team)  { m_DroppedFlagGuid[GetTeamIndexByTeamId(team)].Clear();}
        /**
         * @brief
         *
         * @param team
         * @return const ObjectGuid
         */
        ObjectGuid const& GetDroppedFlagGuid(Team team) const { return m_DroppedFlagGuid[GetTeamIndexByTeamId(team)];}
        /**
         * @brief
         *
         * @param data
         * @param count
         */
        virtual void FillInitialWorldStates(WorldPacket& data, uint32& count) override;
        /**
         * @brief
         *
         */
        virtual Team GetPrematureWinner() override;

    private:
        ObjectGuid m_flagCarrierAlliance; /**< TODO */
        ObjectGuid m_flagCarrierHorde; /**< TODO */

        ObjectGuid m_DroppedFlagGuid[PVP_TEAM_COUNT]; /**< TODO */
        uint8 m_FlagState[PVP_TEAM_COUNT]; /**< TODO */
        int32 m_FlagsTimer[PVP_TEAM_COUNT]; /**< TODO */
        int32 m_FlagsDropTimer[PVP_TEAM_COUNT]; /**< TODO */

        uint32 m_ReputationCapture; /**< TODO */
        uint32 m_HonorWinKills; /**< TODO */
        uint32 m_HonorEndKills; /**< TODO */
};
#endif
