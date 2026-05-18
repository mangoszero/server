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
 * @file BattleGroundWS.h
 * @brief Warsong Gulch battleground header
 *
 * This header defines the Warsong Gulch battleground implementation including:
 * - Flag capture and control mechanics
 * - Flag state management (on base, wait respawn, on player, on ground)
 * - Team-based flag possession and delivery
 * - Scoring system based on flag captures
 * - World state updates for client synchronization
 *
 * Key features:
 * - 2-flag capture points (Silverwing, Gold Mine)
 * - Flag pickup and drop mechanics
 * - Team-based scoring (first to 3 captures wins)
 * - Respawn timers for flags
 *
 * @see BattleGroundWS for implementation
 * @see BattleGround for base class
 */

#ifndef MANGOS_H_BATTLEGROUNDWS
#define MANGOS_H_BATTLEGROUNDWS

#include "BattleGround.h"

#define BG_WS_MAX_TEAM_SCORE      3
#define BG_WS_FLAG_RESPAWN_TIME   (23*IN_MILLISECONDS)
#define BG_WS_FLAG_DROP_TIME      (10*IN_MILLISECONDS)

/**
 * @brief Enum for Warsong Gulch sound effects.
 */
enum BG_WS_Sound
{
    BG_WS_SOUND_FLAG_CAPTURED_ALLIANCE  = 8173,     ///< Alliance flag captured
    BG_WS_SOUND_FLAG_CAPTURED_HORDE     = 8213,     ///< Horde flag captured
    BG_WS_SOUND_FLAG_PLACED             = 8232,     ///< Flag placed at base
    BG_WS_SOUND_FLAG_RETURNED           = 8192,     ///< Flag returned to base
    BG_WS_SOUND_HORDE_FLAG_PICKED_UP    = 8212,     ///< Horde picked up Alliance flag
    BG_WS_SOUND_ALLIANCE_FLAG_PICKED_UP = 8174,     ///< Alliance picked up Horde flag
    BG_WS_SOUND_FLAGS_RESPAWNED         = 8232      ///< Both flags respawned
};

/**
 * @brief Enum for Warsong Gulch spell IDs.
 */
enum BG_WS_SpellId
{
    BG_WS_SPELL_WARSONG_FLAG            = 23333,     ///< Warsong Flag spell
    BG_WS_SPELL_WARSONG_FLAG_DROPPED    = 23334,     ///< Warsong Flag Dropped spell
    BG_WS_SPELL_SILVERWING_FLAG         = 23335,     ///< Silverwing Flag spell
    BG_WS_SPELL_SILVERWING_FLAG_DROPPED = 23336      ///< Silverwing Flag Dropped spell
};

/**
 * @brief Enum for Warsong Gulch world states.
 */
enum BG_WS_WorldStates
{
    BG_WS_FLAG_UNK_ALLIANCE       = 1545,       ///< Alliance flag unknown
    BG_WS_FLAG_UNK_HORDE          = 1546,       ///< Horde flag unknown
    // BG_FLAG_UNK                = 1547,
    BG_WS_FLAG_CAPTURES_ALLIANCE  = 1581,       ///< Alliance flag captures
    BG_WS_FLAG_CAPTURES_HORDE     = 1582,       ///< Horde flag captures
    BG_WS_FLAG_CAPTURES_MAX       = 1601,       ///< Maximum flag captures reached
    BG_WS_FLAG_STATE_HORDE        = 2338,       ///< Horde flag state
    BG_WS_FLAG_STATE_ALLIANCE     = 2339,       ///< Alliance flag state
};

/**
 * @brief Enum for Warsong Gulch flag states.
 */
enum BG_WS_FlagState
{
    BG_WS_FLAG_STATE_ON_BASE      = 0,        ///< Flag at base
    BG_WS_FLAG_STATE_WAIT_RESPAWN = 1,        ///< Flag waiting to respawn
    BG_WS_FLAG_STATE_ON_PLAYER    = 2,        ///< Flag carried by player
    BG_WS_FLAG_STATE_ON_GROUND    = 3         ///< Flag dropped on ground
};

/**
 * @brief Enum for Warsong Gulch graveyards.
 */
enum BG_WS_Graveyards
{
    WS_GRAVEYARD_FLAGROOM_ALLIANCE = 769,     ///< Alliance flag room graveyard
    WS_GRAVEYARD_FLAGROOM_HORDE    = 770,     ///< Horde flag room graveyard
    WS_GRAVEYARD_MAIN_ALLIANCE     = 771,     ///< Alliance main graveyard
    WS_GRAVEYARD_MAIN_HORDE        = 772      ///< Horde main graveyard
};

/**
 * @brief Class for storing Warsong Gulch score.
 *
 * Extends BattleGroundScore with WS-specific attributes:
 * - Flag captures and returns
 * - Team-based scoring
 */
class BattleGroundWGScore : public BattleGroundScore
{
    public:
        /**
         * @brief Constructor for BattleGroundWGScore.
         */
        BattleGroundWGScore() : FlagCaptures(0), FlagReturns(0) {};
        /**
         * @brief Destructor for BattleGroundWGScore.
         */
        virtual ~BattleGroundWGScore() {};

        // Accessors for WS-specific attributes
        uint32 GetAttr1() const { return FlagCaptures; }  ///< Number of flag captures
        uint32 GetAttr2() const { return FlagReturns; }   ///< Number of flag returns

        uint32 FlagCaptures; /**< Number of flag captures. */
        uint32 FlagReturns; /**< Number of flag returns. */
};

/**
 * @brief Enum for Warsong Gulch events.
 *
 * Defines event IDs used in Warsong Gulch battleground.
 */
enum BG_WS_Events
{
    WS_EVENT_FLAG_A               = 0,
    WS_EVENT_FLAG_H               = 1,
    // spiritguides will spawn (same moment, like WS_EVENT_DOOR_OPEN)
    WS_EVENT_SPIRITGUIDES_SPAWN   = 2
};

// Honor granted depending on player's level
const uint32 BG_WSG_FlagCapturedHonor[MAX_BATTLEGROUND_BRACKETS] = {48, 82, 136, 226, 378, 396}; /**< Honor for flag capture. */
const uint32 BG_WSG_WinMatchHonor[MAX_BATTLEGROUND_BRACKETS] = {24, 41, 68, 113, 189, 198}; /**< Honor for winning match. */

/**
 * @brief Class for Warsong Gulch battleground.
 *
 * Extends BattleGround with WS-specific mechanics:
 * - Flag capture and return system
 * - Team-based scoring (first to 3 captures wins)
 * - Flag respawn timers
 * - Graveyard management
 *
 * @see BattleGround for base class
 * @see BattleGroundWGScore for scoring
 */
class BattleGroundWS : public BattleGround
{
        friend class BattleGroundMgr;

    public:
        /**
         * @brief Constructor for BattleGroundWS.
         */
        BattleGroundWS();
        /**
         * @brief Updates the battleground.
         * @param diff The time difference.
         */
        void Update(uint32 diff) override;

        /**
         * @brief Adds a player to the battleground.
         * @param plr Pointer to the player.
         */
        void AddPlayer(Player* plr) override;
        /**
         * @brief Opens the doors at the start of the event.
         */
        void StartingEventOpenDoors() override;

        /**
         * @brief Gets the GUID of the alliance flag carrier.
         * @return ObjectGuid The GUID of the alliance flag carrier.
         */
        ObjectGuid GetAllianceFlagCarrierGuid() const { return m_flagCarrierAlliance; }
        /**
         * @brief Gets the GUID of the horde flag carrier.
         * @return ObjectGuid The GUID of the horde flag carrier.
         */
        ObjectGuid GetHordeFlagCarrierGuid() const { return m_flagCarrierHorde; }

        /**
         * @brief Sets the GUID of the alliance flag carrier.
         * @param guid The GUID of the alliance flag carrier.
         */
        void SetAllianceFlagCarrier(ObjectGuid guid) { m_flagCarrierAlliance = guid; }
        /**
         * @brief Sets the GUID of the horde flag carrier.
         * @param guid The GUID of the horde flag carrier.
         */
        void SetHordeFlagCarrier(ObjectGuid guid) { m_flagCarrierHorde = guid; }

        /**
         * @brief Clears the GUID of the alliance flag carrier.
         */
        void ClearAllianceFlagCarrier() { m_flagCarrierAlliance.Clear(); }
        /**
         * @brief Clears the GUID of the horde flag carrier.
         */
        void ClearHordeFlagCarrier() { m_flagCarrierHorde.Clear(); }

        /**
         * @brief Checks if the alliance flag is picked up.
         * @return bool True if the alliance flag is picked up, false otherwise.
         */
        bool IsAllianceFlagPickedUp() const { return !m_flagCarrierAlliance.IsEmpty(); }
        /**
         * @brief Checks if the horde flag is picked up.
         * @return bool True if the horde flag is picked up, false otherwise.
         */
        bool IsHordeFlagPickedUp() const { return !m_flagCarrierHorde.IsEmpty(); }

        /**
         * @brief Respawns the flag for a team.
         * @param team The team.
         * @param captured True if the flag was captured, false otherwise.
         */
        void RespawnFlag(Team team, bool captured);
        /**
         * @brief Respawns the dropped flag for a team.
         * @param team The team.
         */
        void RespawnDroppedFlag(Team team);
        /**
         * @brief Gets the flag state for a team.
         * @param team The team.
         * @return uint8 The flag state.
         */
        uint8 GetFlagState(Team team) { return m_FlagState[GetTeamIndexByTeamId(team)]; }

        /**
         * @brief Handles the event when a player drops the flag.
         * @param source The player who dropped the flag.
         */
        void EventPlayerDroppedFlag(Player* source) override;
        /**
         * @brief Handles the event when a player clicks on the flag.
         * @param source The player who clicked on the flag.
         * @param target_obj The flag object.
         */
        void EventPlayerClickedOnFlag(Player* source, GameObject* target_obj) override;
        /**
         * @brief Handles the event when a player captures the flag.
         * @param source The player who captured the flag.
         */
        void EventPlayerCapturedFlag(Player* source) override;

        /**
         * @brief Removes a player from the battleground.
         * @param plr The player to remove.
         * @param guid The GUID of the player.
         */
        void RemovePlayer(Player* plr, ObjectGuid guid) override;
        /**
         * @brief Handles area triggers.
         * @param source The player who triggered the area.
         * @param trigger The trigger ID.
         * @return bool True if the trigger was handled, false otherwise.
         */
        bool HandleAreaTrigger(Player* source, uint32 trigger) override;
        /**
         * @brief Handles the event when a player is killed.
         * @param player The player who was killed.
         * @param killer The player who killed.
         */
        void HandleKillPlayer(Player* player, Player* killer) override;
        /**
         * @brief Resets the battleground.
         */
        void Reset() override;
        /**
         * @brief Ends the battleground.
         * @param winner The winning team.
         */
        void EndBattleGround(Team winner) override;
        /**
         * @brief Gets the closest graveyard for a player.
         * @param player The player.
         * @return const WorldSafeLocsEntry The closest graveyard.
         */
        WorldSafeLocsEntry const* GetClosestGraveYard(Player* player) override;

        /**
         * @brief Updates the flag state for a team.
         * @param team The team.
         * @param value The flag state value.
         */
        void UpdateFlagState(Team team, uint32 value);
        /**
         * @brief Updates the team score.
         * @param team The team.
         */
        void UpdateTeamScore(Team team);
        /**
         * @brief Updates the player score.
         * @param source The player.
         * @param type The score type.
         * @param value The score value.
         */
        void UpdatePlayerScore(Player* source, uint32 type, uint32 value) override;
        /**
         * @brief Sets the GUID of the dropped flag for a team.
         * @param guid The GUID of the dropped flag.
         * @param team The team.
         */
        void SetDroppedFlagGuid(ObjectGuid guid, Team team)  { m_DroppedFlagGuid[GetTeamIndexByTeamId(team)] = guid;}
        /**
         * @brief Clears the GUID of the dropped flag for a team.
         * @param team The team.
         */
        void ClearDroppedFlagGuid(Team team)  { m_DroppedFlagGuid[GetTeamIndexByTeamId(team)].Clear();}
        /**
         * @brief Gets the GUID of the dropped flag for a team.
         * @param team The team.
         * @return const ObjectGuid The GUID of the dropped flag.
         */
        ObjectGuid const& GetDroppedFlagGuid(Team team) const { return m_DroppedFlagGuid[GetTeamIndexByTeamId(team)];}
        /**
         * @brief Fills the initial world states.
         * @param data The world packet data.
         * @param count The count of world states.
         */
        void FillInitialWorldStates(WorldPacket& data, uint32& count) override;
        /**
         * @brief Gets the premature winner of the battleground.
         * @return Team The premature winner.
         */
        Team GetPrematureWinner() override;

    private:
        ObjectGuid m_flagCarrierAlliance; /**< GUID of the alliance flag carrier. */
        ObjectGuid m_flagCarrierHorde; /**< GUID of the horde flag carrier. */

        ObjectGuid m_DroppedFlagGuid[PVP_TEAM_COUNT]; /**< GUIDs of the dropped flags. */
        uint8 m_FlagState[PVP_TEAM_COUNT]; /**< States of the flags. */
        int32 m_FlagsTimer[PVP_TEAM_COUNT]; /**< Timers for the flags. */
        int32 m_FlagsDropTimer[PVP_TEAM_COUNT]; /**< Drop timers for the flags. */

        uint32 m_ReputationCapture; /**< Reputation for capturing the flag. */
        uint32 m_HonorWinKills; /**< Honor for winning kills. */
        uint32 m_HonorEndKills; /**< Honor for end kills. */
};
#endif
