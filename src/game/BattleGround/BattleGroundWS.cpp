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
 * @file BattleGroundWS.cpp
 * @brief Implementation of Warsong Gulch battleground.
 *
 * This file contains the implementation of the Warsong Gulch battleground (BattleGroundWS),
 * which features:
 * - Flag capture and control mechanics
 * - Flag return and respawn logic
 * - Flag drop and pickup handling
 * - Team-based flag possession tracking
 * - Victory condition (first to 3 flag captures)
 * - Integration with the base BattleGround class
 *
 * Warsong Gulch is a team-based battleground focused on capturing and delivering
 * the opposing team's flag to achieve victory points.
 */

#include "Object.h"
#include "Player.h"
#include "BattleGround.h"
#include "BattleGroundWS.h"
#include "GameObject.h"
#include "ObjectMgr.h"
#include "BattleGroundMgr.h"
#include "WorldPacket.h"
#include "Language.h"

/**
 * @brief Constructor for BattleGroundWS.
 *
 * Initializes Warsong Gulch with default start messages and game state.
 */
BattleGroundWS::BattleGroundWS()
{
    m_StartMessageIds[BG_STARTING_EVENT_FIRST]  = 0;
    m_StartMessageIds[BG_STARTING_EVENT_SECOND] = LANG_BG_WS_START_ONE_MINUTE;
    m_StartMessageIds[BG_STARTING_EVENT_THIRD]  = LANG_BG_WS_START_HALF_MINUTE;
    m_StartMessageIds[BG_STARTING_EVENT_FOURTH] = LANG_BG_WS_HAS_BEGUN;
}

/**
 * @brief Updates the Warsong Gulch battleground.
 *
 * Handles flag state transitions, respawn timers for dropped flags, and checks for victory.
 * Called once per world tick while the battleground is in progress.
 *
 * @param diff The time elapsed since the last update in milliseconds.
 */
void BattleGroundWS::Update(uint32 diff)
{
    BattleGround::Update(diff);

    if (GetStatus() == STATUS_IN_PROGRESS)
    {
        if (m_FlagState[TEAM_INDEX_ALLIANCE] == BG_WS_FLAG_STATE_WAIT_RESPAWN)
        {
            m_FlagsTimer[TEAM_INDEX_ALLIANCE] -= diff;

            if (m_FlagsTimer[TEAM_INDEX_ALLIANCE] < 0)
            {
                m_FlagsTimer[TEAM_INDEX_ALLIANCE] = 0;
                RespawnFlag(ALLIANCE, true);
            }
        }
        if (m_FlagState[TEAM_INDEX_ALLIANCE] == BG_WS_FLAG_STATE_ON_GROUND)
        {
            m_FlagsDropTimer[TEAM_INDEX_ALLIANCE] -= diff;

            if (m_FlagsDropTimer[TEAM_INDEX_ALLIANCE] < 0)
            {
                m_FlagsDropTimer[TEAM_INDEX_ALLIANCE] = 0;
                RespawnDroppedFlag(ALLIANCE);
            }
        }
        if (m_FlagState[TEAM_INDEX_HORDE] == BG_WS_FLAG_STATE_WAIT_RESPAWN)
        {
            m_FlagsTimer[TEAM_INDEX_HORDE] -= diff;

            if (m_FlagsTimer[TEAM_INDEX_HORDE] < 0)
            {
                m_FlagsTimer[TEAM_INDEX_HORDE] = 0;
                RespawnFlag(HORDE, true);
            }
        }
        if (m_FlagState[TEAM_INDEX_HORDE] == BG_WS_FLAG_STATE_ON_GROUND)
        {
            m_FlagsDropTimer[TEAM_INDEX_HORDE] -= diff;

            if (m_FlagsDropTimer[TEAM_INDEX_HORDE] < 0)
            {
                m_FlagsDropTimer[TEAM_INDEX_HORDE] = 0;
                RespawnDroppedFlag(HORDE);
            }
        }
    }
}

/**
 * @brief Opens doors and spawns initial Warsong Gulch objects.
 *
 * Triggers door opening and spawns the flags and spirit guides at the start of the battleground.
 */
void BattleGroundWS::StartingEventOpenDoors()
{
    OpenDoorEvent(BG_EVENT_DOOR);

    // TODO implement timer to despawn doors after a short while

    SpawnEvent(WS_EVENT_SPIRITGUIDES_SPAWN, 0, true);
    SpawnEvent(WS_EVENT_FLAG_A, 0, true);
    SpawnEvent(WS_EVENT_FLAG_H, 0, true);
}

/**
 * @brief Adds a player to the Warsong Gulch battleground.
 *
 * Initializes the player's score entry for this battleground.
 *
 * @param plr Pointer to the player to add.
 */
void BattleGroundWS::AddPlayer(Player* plr)
{
    BattleGround::AddPlayer(plr);
    // create score and add it to map, default values are set in constructor
    BattleGroundWGScore* sc = new BattleGroundWGScore;

    m_PlayerScores[plr->GetObjectGuid()] = sc;
}

/**
 * @brief Respawns a flag at its base location.
 *
 * Resets a captured or dropped flag back to its base and broadcasts the respawn event
 * to all players. Handles the initial placement message and sound.
 *
 * @param team The team whose flag to respawn (ALLIANCE or HORDE).
 * @param captured Whether the flag was captured (true) or merely dropped (false).
 */
void BattleGroundWS::RespawnFlag(Team team, bool captured)
{
    if (team == ALLIANCE)
    {
        DEBUG_LOG("Respawn Alliance flag");
        m_FlagState[TEAM_INDEX_ALLIANCE] = BG_WS_FLAG_STATE_ON_BASE;
        SpawnEvent(WS_EVENT_FLAG_A, 0, true);
    }
    else
    {
        DEBUG_LOG("Respawn Horde flag");
        m_FlagState[TEAM_INDEX_HORDE] = BG_WS_FLAG_STATE_ON_BASE;
        SpawnEvent(WS_EVENT_FLAG_H, 0, true);
    }

    if (captured)
    {
        // when map_update will be allowed for battlegrounds this code will be useless
        SpawnEvent(WS_EVENT_FLAG_A, 0, true);
        SpawnEvent(WS_EVENT_FLAG_H, 0, true);
        SendMessageToAll(LANG_BG_WS_F_PLACED, CHAT_MSG_BG_SYSTEM_NEUTRAL);
        PlaySoundToAll(BG_WS_SOUND_FLAGS_RESPAWNED);        // flag respawned sound...
    }
}

/**
 * @brief Respawns a flag that was dropped on the ground.
 *
 * Restores the specified team's flag to its base, broadcasts the proper message,
 * removes the dropped flag object from the map, and clears the stored dropped-flag GUID.
 *
 * @param team The team whose dropped flag should be respawned.
 */
void BattleGroundWS::RespawnDroppedFlag(Team team)
{
    if (GetStatus() != STATUS_IN_PROGRESS)
    {
        return;
    }

    RespawnFlag(team, false);
    if (team == ALLIANCE)
    {
        SendMessageToAll(LANG_BG_WS_ALLIANCE_FLAG_RESPAWNED, CHAT_MSG_BG_SYSTEM_NEUTRAL);
    }
    else
    {
        SendMessageToAll(LANG_BG_WS_HORDE_FLAG_RESPAWNED, CHAT_MSG_BG_SYSTEM_NEUTRAL);
    }

    PlaySoundToAll(BG_WS_SOUND_FLAGS_RESPAWNED);

    GameObject* obj = GetBgMap()->GetGameObject(GetDroppedFlagGuid(team));
    if (obj)
    {
        obj->Delete();
    }
    else
    {
        sLog.outError("Unknown dropped flag bg: %s", GetDroppedFlagGuid(team).GetString().c_str());
    }

    ClearDroppedFlagGuid(team);
}

/**
 * @brief Processes a flag capture event in Warsong Gulch.
 *
 * Called when a player successfully carries the enemy flag back to their base.
 * Updates team score, removes the flag aura from the carrier, spawns the captured flag
 * for respawn, and grants reputation/honor rewards.
 *
 * @param source The player who captured the flag.
 */
void BattleGroundWS::EventPlayerCapturedFlag(Player* source)
{
    if (GetStatus() != STATUS_IN_PROGRESS)
    {
        return;
    }

    source->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_ENTER_PVP_COMBAT);
    if (source->GetTeam() == ALLIANCE)
    {
        if (!IsHordeFlagPickedUp())
        {
            return;
        }
        ClearHordeFlagCarrier();                            // must be before aura remove to prevent 2 events (drop+capture) at the same time
        // horde flag in base (but not respawned yet)
        m_FlagState[TEAM_INDEX_HORDE] = BG_WS_FLAG_STATE_WAIT_RESPAWN;
        // Drop Horde Flag from Player
        source->RemoveAurasDueToSpell(BG_WS_SPELL_WARSONG_FLAG);
        if (m_TeamScores[TEAM_INDEX_ALLIANCE] < BG_WS_MAX_TEAM_SCORE)
        {
            m_TeamScores[TEAM_INDEX_ALLIANCE] += 1;
        }
        PlaySoundToAll(BG_WS_SOUND_FLAG_CAPTURED_ALLIANCE);
        RewardReputationToTeam(890, m_ReputationCapture, ALLIANCE);
    }
    else
    {
        if (!IsAllianceFlagPickedUp())
        {
            return;
        }
        ClearAllianceFlagCarrier();                         // must be before aura remove to prevent 2 events (drop+capture) at the same time
        // alliance flag in base (but not respawned yet)
        m_FlagState[TEAM_INDEX_ALLIANCE] = BG_WS_FLAG_STATE_WAIT_RESPAWN;
        // Drop Alliance Flag from Player
        source->RemoveAurasDueToSpell(BG_WS_SPELL_SILVERWING_FLAG);
        if (m_TeamScores[TEAM_INDEX_HORDE] < BG_WS_MAX_TEAM_SCORE)
        {
            m_TeamScores[TEAM_INDEX_HORDE] += 1;
        }
        PlaySoundToAll(BG_WS_SOUND_FLAG_CAPTURED_HORDE);
        RewardReputationToTeam(889, m_ReputationCapture, HORDE);
    }
    // for flag capture is reward distributed according level range
    RewardHonorToTeam(BG_WSG_FlagCapturedHonor[GetBracketId()], source->GetTeam());

    // despawn flags
    SpawnEvent(WS_EVENT_FLAG_A, 0, false);
    SpawnEvent(WS_EVENT_FLAG_H, 0, false);

    if (source->GetTeam() == ALLIANCE)
    {
        SendMessageToAll(LANG_BG_WS_CAPTURED_HF, CHAT_MSG_BG_SYSTEM_ALLIANCE, source);
    }
    else
    {
        SendMessageToAll(LANG_BG_WS_CAPTURED_AF, CHAT_MSG_BG_SYSTEM_HORDE, source);
    }

    UpdateFlagState(source->GetTeam(), 1);                  // flag state none
    UpdateTeamScore(source->GetTeam());
    // only flag capture should be updated
    UpdatePlayerScore(source, SCORE_FLAG_CAPTURES, 1);      // +1 flag captures

    Team winner = TEAM_NONE;
    if (m_TeamScores[TEAM_INDEX_ALLIANCE] == BG_WS_MAX_TEAM_SCORE)
    {
        winner = ALLIANCE;
    }
    else if (m_TeamScores[TEAM_INDEX_HORDE] == BG_WS_MAX_TEAM_SCORE)
    {
        winner = HORDE;
    }

    if (winner)
    {
        UpdateWorldState(BG_WS_FLAG_UNK_ALLIANCE, 0);
        UpdateWorldState(BG_WS_FLAG_UNK_HORDE, 0);
        UpdateWorldState(BG_WS_FLAG_STATE_ALLIANCE, 1);
        UpdateWorldState(BG_WS_FLAG_STATE_HORDE, 1);

        EndBattleGround(winner);
    }
    else
    {
        m_FlagsTimer[GetOtherTeamIndex(GetTeamIndexByTeamId(source->GetTeam()))] = BG_WS_FLAG_RESPAWN_TIME;
    }
}

/**
 * @brief Processes a flag drop event in Warsong Gulch.
 *
 * Called when a flag carrier dies or leaves the battleground, dropping the flag.
 * Spawns the flag on the ground, sets the flag drop timer, and removes the flag carrier status.
 * Resets the flag respawn timer for the other team.
 *
 * @param source The player dropping the flag.
 */
void BattleGroundWS::EventPlayerDroppedFlag(Player* source)
{
    if (GetStatus() != STATUS_IN_PROGRESS)
    {
        // if not running, do not cast things at the dropper player (prevent spawning the "dropped" flag), neither send unnecessary messages
        // just take off the aura
        if (source->GetTeam() == ALLIANCE)
        {
            if (!IsHordeFlagPickedUp())
            {
                return;
            }
            if (GetHordeFlagCarrierGuid() == source->GetObjectGuid())
            {
                ClearHordeFlagCarrier();
                source->RemoveAurasDueToSpell(BG_WS_SPELL_WARSONG_FLAG);
            }
        }
        else
        {
            if (!IsAllianceFlagPickedUp())
            {
                return;
            }
            if (GetAllianceFlagCarrierGuid() == source->GetObjectGuid())
            {
                ClearAllianceFlagCarrier();
                source->RemoveAurasDueToSpell(BG_WS_SPELL_SILVERWING_FLAG);
            }
        }
        return;
    }

    bool set = false;

    if (source->GetTeam() == ALLIANCE)
    {
        if (!IsHordeFlagPickedUp())
        {
            return;
        }
        if (GetHordeFlagCarrierGuid() == source->GetObjectGuid())
        {
            ClearHordeFlagCarrier();
            source->RemoveAurasDueToSpell(BG_WS_SPELL_WARSONG_FLAG);
            m_FlagState[TEAM_INDEX_HORDE] = BG_WS_FLAG_STATE_ON_GROUND;
            source->CastSpell(source, BG_WS_SPELL_WARSONG_FLAG_DROPPED, true);
            set = true;
        }
    }
    else
    {
        if (!IsAllianceFlagPickedUp())
        {
            return;
        }
        if (GetAllianceFlagCarrierGuid() == source->GetObjectGuid())
        {
            ClearAllianceFlagCarrier();
            source->RemoveAurasDueToSpell(BG_WS_SPELL_SILVERWING_FLAG);
            m_FlagState[TEAM_INDEX_ALLIANCE] = BG_WS_FLAG_STATE_ON_GROUND;
            source->CastSpell(source, BG_WS_SPELL_SILVERWING_FLAG_DROPPED, true);
            set = true;
        }
    }

    if (set)
    {
        UpdateFlagState(source->GetTeam(), 1);

        if (source->GetTeam() == ALLIANCE)
        {
            SendMessageToAll(LANG_BG_WS_DROPPED_HF, CHAT_MSG_BG_SYSTEM_HORDE, source);
            UpdateWorldState(BG_WS_FLAG_UNK_HORDE, uint32(-1));
        }
        else
        {
            SendMessageToAll(LANG_BG_WS_DROPPED_AF, CHAT_MSG_BG_SYSTEM_ALLIANCE, source);
            UpdateWorldState(BG_WS_FLAG_UNK_ALLIANCE, uint32(-1));
        }

        m_FlagsDropTimer[GetOtherTeamIndex(GetTeamIndexByTeamId(source->GetTeam()))] = BG_WS_FLAG_DROP_TIME;
    }
}

/**
 * @brief Handles flag pickup by a player.
 *
 * Processes when a player picks up a flag, applying the appropriate aura and updating
 * the flag state. Prevents pickup if the flag is not in a valid state.
 *
 * @param source Pointer to the player picking up the flag.
 * @param target_obj Pointer to the flag game object.
 */
void BattleGroundWS::EventPlayerClickedOnFlag(Player* source, GameObject* target_obj)
{
    if (GetStatus() != STATUS_IN_PROGRESS)
    {
        return;
    }

    int32 message_id = 0;
    ChatMsg type;

    uint8 event = (sBattleGroundMgr.GetGameObjectEventIndex(target_obj->GetGUIDLow())).event1;

    // alliance flag picked up from base
    if (source->GetTeam() == HORDE && GetFlagState(ALLIANCE) == BG_WS_FLAG_STATE_ON_BASE
        && event == WS_EVENT_FLAG_A)
    {
        message_id = LANG_BG_WS_PICKEDUP_AF;
        type = CHAT_MSG_BG_SYSTEM_HORDE;
        PlaySoundToAll(BG_WS_SOUND_ALLIANCE_FLAG_PICKED_UP);
        SpawnEvent(WS_EVENT_FLAG_A, 0, false);
        SetAllianceFlagCarrier(source->GetObjectGuid());
        m_FlagState[TEAM_INDEX_ALLIANCE] = BG_WS_FLAG_STATE_ON_PLAYER;
        // update world state to show correct flag carrier
        UpdateFlagState(HORDE, BG_WS_FLAG_STATE_ON_PLAYER);
        UpdateWorldState(BG_WS_FLAG_UNK_ALLIANCE, 1);
        source->CastSpell(source, BG_WS_SPELL_SILVERWING_FLAG, true);
    }

    // horde flag picked up from base
    if (source->GetTeam() == ALLIANCE && GetFlagState(HORDE) == BG_WS_FLAG_STATE_ON_BASE
        && event == WS_EVENT_FLAG_H)
    {
        message_id = LANG_BG_WS_PICKEDUP_HF;
        type = CHAT_MSG_BG_SYSTEM_ALLIANCE;
        PlaySoundToAll(BG_WS_SOUND_HORDE_FLAG_PICKED_UP);
        SpawnEvent(WS_EVENT_FLAG_H, 0, false);
        SetHordeFlagCarrier(source->GetObjectGuid());
        m_FlagState[TEAM_INDEX_HORDE] = BG_WS_FLAG_STATE_ON_PLAYER;
        // update world state to show correct flag carrier
        UpdateFlagState(ALLIANCE, BG_WS_FLAG_STATE_ON_PLAYER);
        UpdateWorldState(BG_WS_FLAG_UNK_HORDE, 1);
        source->CastSpell(source, BG_WS_SPELL_WARSONG_FLAG, true);
    }

    // Alliance flag on ground(not in base) (returned or picked up again from ground!)
    if (GetFlagState(ALLIANCE) == BG_WS_FLAG_STATE_ON_GROUND && source->IsWithinDistInMap(target_obj, 10))
    {
        if (source->GetTeam() == ALLIANCE)
        {
            message_id = LANG_BG_WS_RETURNED_AF;
            type = CHAT_MSG_BG_SYSTEM_ALLIANCE;
            UpdateFlagState(HORDE, BG_WS_FLAG_STATE_WAIT_RESPAWN);
            RespawnFlag(ALLIANCE, false);
            PlaySoundToAll(BG_WS_SOUND_FLAG_RETURNED);
            UpdatePlayerScore(source, SCORE_FLAG_RETURNS, 1);
        }
        else
        {
            message_id = LANG_BG_WS_PICKEDUP_AF;
            type = CHAT_MSG_BG_SYSTEM_HORDE;
            PlaySoundToAll(BG_WS_SOUND_ALLIANCE_FLAG_PICKED_UP);
            SpawnEvent(WS_EVENT_FLAG_A, 0, false);
            SetAllianceFlagCarrier(source->GetObjectGuid());
            source->CastSpell(source, BG_WS_SPELL_SILVERWING_FLAG, true);
            m_FlagState[TEAM_INDEX_ALLIANCE] = BG_WS_FLAG_STATE_ON_PLAYER;
            UpdateFlagState(HORDE, BG_WS_FLAG_STATE_ON_PLAYER);
            UpdateWorldState(BG_WS_FLAG_UNK_ALLIANCE, 1);
        }
        // called in HandleGameObjectUseOpcode:
        // target_obj->Delete();
    }

    // Horde flag on ground(not in base) (returned or picked up again)
    if (GetFlagState(HORDE) == BG_WS_FLAG_STATE_ON_GROUND && source->IsWithinDistInMap(target_obj, 10))
    {
        if (source->GetTeam() == HORDE)
        {
            message_id = LANG_BG_WS_RETURNED_HF;
            type = CHAT_MSG_BG_SYSTEM_HORDE;
            UpdateFlagState(ALLIANCE, BG_WS_FLAG_STATE_WAIT_RESPAWN);
            RespawnFlag(HORDE, false);
            PlaySoundToAll(BG_WS_SOUND_FLAG_RETURNED);
            UpdatePlayerScore(source, SCORE_FLAG_RETURNS, 1);
        }
        else
        {
            message_id = LANG_BG_WS_PICKEDUP_HF;
            type = CHAT_MSG_BG_SYSTEM_ALLIANCE;
            PlaySoundToAll(BG_WS_SOUND_HORDE_FLAG_PICKED_UP);
            SpawnEvent(WS_EVENT_FLAG_H, 0, false);
            SetHordeFlagCarrier(source->GetObjectGuid());
            source->CastSpell(source, BG_WS_SPELL_WARSONG_FLAG, true);
            m_FlagState[TEAM_INDEX_HORDE] = BG_WS_FLAG_STATE_ON_PLAYER;
            UpdateFlagState(ALLIANCE, BG_WS_FLAG_STATE_ON_PLAYER);
            UpdateWorldState(BG_WS_FLAG_UNK_HORDE, 1);
        }
        // called in HandleGameObjectUseOpcode:
        // target_obj->Delete();
    }

    if (!message_id)
    {
        return;
    }

    SendMessageToAll(message_id, type, source);
    source->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_ENTER_PVP_COMBAT);
}

/**
 * @brief Removes a player from Warsong Gulch flag tracking.
 *
 * Ensures that any flag carried by the leaving or disconnected player is dropped
 * or respawned correctly before the player is fully removed from the battleground.
 *
 * @param plr Pointer to the player, or NULL if the player is offline.
 * @param guid The GUID of the player being removed.
 */
void BattleGroundWS::RemovePlayer(Player* plr, ObjectGuid guid)
{
    // sometimes flag aura not removed :(
    if (IsAllianceFlagPickedUp() && m_flagCarrierAlliance == guid)
    {
        if (!plr)
        {
            sLog.outError("BattleGroundWS: Removing offline player who has the FLAG!!");
            ClearAllianceFlagCarrier();
            RespawnFlag(ALLIANCE, false);
        }
        else
        {
            EventPlayerDroppedFlag(plr);
        }
    }
    if (IsHordeFlagPickedUp() && m_flagCarrierHorde == guid)
    {
        if (!plr)
        {
            sLog.outError("BattleGroundWS: Removing offline player who has the FLAG!!");
            ClearHordeFlagCarrier();
            RespawnFlag(HORDE, false);
        }
        else
        {
            EventPlayerDroppedFlag(plr);
        }
    }
}

/**
 * @brief Updates the displayed flag state for a team.
 *
 * Writes the current flag state value into the appropriate Warsong Gulch world
 * state field for the specified team.
 *
 * @param team The team whose flag state is being updated.
 * @param value The new world state value.
 */
void BattleGroundWS::UpdateFlagState(Team team, uint32 value)
{
    if (team == ALLIANCE)
    {
        UpdateWorldState(BG_WS_FLAG_STATE_ALLIANCE, value);
    }
    else
    {
        UpdateWorldState(BG_WS_FLAG_STATE_HORDE, value);
    }
}

/**
 * @brief Updates the team score display for Warsong Gulch.
 *
 * Updates the world state to display the current flag capture count for each team.
 *
 * @param team The team whose score to update (ALLIANCE or HORDE).
 */
void BattleGroundWS::UpdateTeamScore(Team team)
{
    if (team == ALLIANCE)
    {
        UpdateWorldState(BG_WS_FLAG_CAPTURES_ALLIANCE, m_TeamScores[TEAM_INDEX_ALLIANCE]);
    }
    else
    {
        UpdateWorldState(BG_WS_FLAG_CAPTURES_HORDE, m_TeamScores[TEAM_INDEX_HORDE]);
    }
}

/**
 * @brief Handles area trigger entry points for Warsong Gulch.
 *
 * Processes when players enter capture zones or exit portals. Handles flag capture
 * detection, team validation, and battleground exit triggers.
 *
 * @param source Pointer to the player entering the trigger.
 * @param trigger The trigger ID.
 * @return true if the trigger was handled, false otherwise.
 */
bool BattleGroundWS::HandleAreaTrigger(Player* source, uint32 trigger)
{
    // this is wrong way to implement these things. On official it done by gameobject spell cast.
    if (GetStatus() != STATUS_IN_PROGRESS)
    {
        return false;
    }

    switch (trigger)
    {
        case 3646:                                          // Alliance Flag spawn
            if (m_FlagState[TEAM_INDEX_HORDE] && !m_FlagState[TEAM_INDEX_ALLIANCE])
                if (GetHordeFlagCarrierGuid() == source->GetObjectGuid())
                {
                    EventPlayerCapturedFlag(source);
                }
            break;
        case 3647:                                          // Horde Flag spawn
            if (m_FlagState[TEAM_INDEX_ALLIANCE] && !m_FlagState[TEAM_INDEX_HORDE])
                if (GetAllianceFlagCarrierGuid() == source->GetObjectGuid())
                {
                    EventPlayerCapturedFlag(source);
                }
            break;
        case 3669: // horde portal
            if (source->GetTeam() != HORDE)
            {
                source->GetSession()->SendNotification(LANG_BATTLEGROUND_ONLY_HORDE_USE);
            }
            else
            {
                source->LeaveBattleground();
            }
            break;
        case 3671: // alliance portal
            if (source->GetTeam() != ALLIANCE)
            {
                source->GetSession()->SendNotification(LANG_BATTLEGROUND_ONLY_ALLIANCE_USE);
            }
            else
            {
                source->LeaveBattleground();
            }
            break;
        default:
            return false;
    }
    return true;
}

/**
 * @brief Resets Warsong Gulch to its initial state.
 *
 * Clears active events, resets team scores, restores flag states to base,
 * clears carrier and dropped-flag tracking, and recalculates weekend reward values.
 */
void BattleGroundWS::Reset()
{
    // call parent's class reset
    BattleGround::Reset();

    // spiritguides and flags not spawned at beginning
    m_ActiveEvents[WS_EVENT_SPIRITGUIDES_SPAWN] = BG_EVENT_NONE;
    m_ActiveEvents[WS_EVENT_FLAG_A] = BG_EVENT_NONE;
    m_ActiveEvents[WS_EVENT_FLAG_H] = BG_EVENT_NONE;

    for (uint8 i = 0; i < PVP_TEAM_COUNT; ++i)
    {
        m_DroppedFlagGuid[i].Clear();
        m_FlagState[i]       = BG_WS_FLAG_STATE_ON_BASE;
        m_TeamScores[i]      = 0;
    }

    m_flagCarrierAlliance.Clear();
    m_flagCarrierHorde.Clear();

    bool isBGWeekend = BattleGroundMgr::IsBGWeekend(GetTypeID());
    m_ReputationCapture = (isBGWeekend) ? 25 : 15;
    m_HonorWinKills = (isBGWeekend) ? 3 : 1;
    m_HonorEndKills = (isBGWeekend) ? 4 : 2;
}

/**
 * @brief Ends the Warsong Gulch battleground.
 *
 * Rewards the winning team with honor and calls parent class to finish.
 *
 * @param winner The winning team (ALLIANCE or HORDE).
 */
void BattleGroundWS::EndBattleGround(Team winner)
{
    // win reward
    if (winner == ALLIANCE)
    {
        RewardHonorToTeam(BG_WSG_WinMatchHonor[GetBracketId()], ALLIANCE);
    }
    if (winner == HORDE)
    {
        RewardHonorToTeam(BG_WSG_WinMatchHonor[GetBracketId()], HORDE);
    }

    BattleGround::EndBattleGround(winner);
}

/**
 * @brief Handles a player death in Warsong Gulch.
 *
 * Processes player kills and drops flags if the killed player was carrying one.
 *
 * @param player Pointer to the killed player.
 * @param killer Pointer to the player who killed them.
 */
void BattleGroundWS::HandleKillPlayer(Player* player, Player* killer)
{
    if (GetStatus() != STATUS_IN_PROGRESS)
    {
        return;
    }

    EventPlayerDroppedFlag(player);

    BattleGround::HandleKillPlayer(player, killer);
}

/**
 * @brief Updates a player's Warsong Gulch score entry.
 *
 * Applies score changes specific to flag captures and returns, delegating all
 * other score updates to the base battleground implementation.
 *
 * @param source The player whose score is being updated.
 * @param type The score category to modify.
 * @param value The amount to add to the score category.
 */
void BattleGroundWS::UpdatePlayerScore(Player* source, uint32 type, uint32 value)
{
    BattleGroundScoreMap::iterator itr = m_PlayerScores.find(source->GetObjectGuid());
    if (itr == m_PlayerScores.end())                        // player not found
    {
        return;
    }

    switch (type)
    {
        case SCORE_FLAG_CAPTURES:                           // flags captured
            ((BattleGroundWGScore*)itr->second)->FlagCaptures += value;
            break;
        case SCORE_FLAG_RETURNS:                            // flags returned
            ((BattleGroundWGScore*)itr->second)->FlagReturns += value;
            break;
        default:
            BattleGround::UpdatePlayerScore(source, type, value);
            break;
    }
}

/**
 * @brief Gets the closest graveyard for a player in Warsong Gulch.
 *
 * Returns the appropriate graveyard based on game status. During preparation,
 * returns the flagroom graveyard to prevent exploiting. During battle, returns
 * the main graveyards.
 *
 * @param player Pointer to the player.
 * @return Pointer to the closest graveyard entry, or NULL if none found.
 */
WorldSafeLocsEntry const* BattleGroundWS::GetClosestGraveYard(Player* player)
{
    // if status in progress, it returns main graveyards with spiritguides
    // else it will return the graveyard in the flagroom - this is especially good
    // if a player dies in preparation phase - then the player can't cheat
    // and teleport to the graveyard outside the flagroom
    // and start running around, while the doors are still closed
    if (player->GetTeam() == ALLIANCE)
    {
        if (GetStatus() == STATUS_IN_PROGRESS)
        {
            return sWorldSafeLocsStore.LookupEntry(WS_GRAVEYARD_MAIN_ALLIANCE);
        }
        else
        {
            return sWorldSafeLocsStore.LookupEntry(WS_GRAVEYARD_FLAGROOM_ALLIANCE);
        }
    }
    else
    {
        if (GetStatus() == STATUS_IN_PROGRESS)
        {
            return sWorldSafeLocsStore.LookupEntry(WS_GRAVEYARD_MAIN_HORDE);
        }
        else
        {
            return sWorldSafeLocsStore.LookupEntry(WS_GRAVEYARD_FLAGROOM_HORDE);
        }
    }
}

/**
 * @brief Fills initial world state values for Warsong Gulch.
 *
 * Sends all initial world state values to clients when they enter, including
 * flag positions, capture counts, and flag carrier information.
 *
 * @param data The packet to write world state data to.
 * @param count Reference to the count of world state entries.
 */
void BattleGroundWS::FillInitialWorldStates(WorldPacket& data, uint32& count)
{
    FillInitialWorldState(data, count, BG_WS_FLAG_CAPTURES_ALLIANCE, m_TeamScores[TEAM_INDEX_ALLIANCE]);
    FillInitialWorldState(data, count, BG_WS_FLAG_CAPTURES_HORDE, m_TeamScores[TEAM_INDEX_HORDE]);

    if (m_FlagState[TEAM_INDEX_ALLIANCE] == BG_WS_FLAG_STATE_ON_GROUND)
    {
        FillInitialWorldState(data, count, BG_WS_FLAG_UNK_ALLIANCE, -1);
    }
    else if (m_FlagState[TEAM_INDEX_ALLIANCE] == BG_WS_FLAG_STATE_ON_PLAYER)
    {
        FillInitialWorldState(data, count, BG_WS_FLAG_UNK_ALLIANCE, 1);
    }
    else
    {
        FillInitialWorldState(data, count, BG_WS_FLAG_UNK_ALLIANCE, 0);
    }

    if (m_FlagState[TEAM_INDEX_HORDE] == BG_WS_FLAG_STATE_ON_GROUND)
    {
        FillInitialWorldState(data, count, BG_WS_FLAG_UNK_HORDE, -1);
    }
    else if (m_FlagState[TEAM_INDEX_HORDE] == BG_WS_FLAG_STATE_ON_PLAYER)
    {
        FillInitialWorldState(data, count, BG_WS_FLAG_UNK_HORDE, 1);
    }
    else
    {
        FillInitialWorldState(data, count, BG_WS_FLAG_UNK_HORDE, 0);
    }

    FillInitialWorldState(data, count, BG_WS_FLAG_CAPTURES_MAX, BG_WS_MAX_TEAM_SCORE);

    if (m_FlagState[TEAM_INDEX_HORDE] == BG_WS_FLAG_STATE_ON_PLAYER)
    {
        FillInitialWorldState(data, count, BG_WS_FLAG_STATE_HORDE, 2);
    }
    else
    {
        FillInitialWorldState(data, count, BG_WS_FLAG_STATE_HORDE, 1);
    }

    if (m_FlagState[TEAM_INDEX_ALLIANCE] == BG_WS_FLAG_STATE_ON_PLAYER)
    {
        FillInitialWorldState(data, count, BG_WS_FLAG_STATE_ALLIANCE, 2);
    }
    else
    {
        FillInitialWorldState(data, count, BG_WS_FLAG_STATE_ALLIANCE, 1);
    }
}

/**
 * @brief Determines the premature winner if Warsong Gulch ends early.
 *
 * Compares flag capture scores to determine which team would win if the battleground
 * ended prematurely (e.g., due to one team disconnecting).
 *
 * @return The winning team, or TEAM_NONE if tied.
 */
Team BattleGroundWS::GetPrematureWinner()
{
    int32 hordeScore = m_TeamScores[TEAM_INDEX_HORDE];
    int32 allianceScore = m_TeamScores[TEAM_INDEX_ALLIANCE];

    if (hordeScore > allianceScore)
    {
        return HORDE;
    }
    if (allianceScore > hordeScore)
    {
        return ALLIANCE;
    }

    // If the values are equal, fall back to number of players on each team
    return BattleGround::GetPrematureWinner();
}
