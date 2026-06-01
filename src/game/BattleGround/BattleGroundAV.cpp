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
 * @file BattleGroundAV.cpp
 * @brief Implementation of Alterac Valley battleground.
 *
 * This file contains the implementation of the Alterac Valley battleground (BattleGroundAV),
 * which features:
 * - Tower and graveyard capture mechanics
 * - Resource point management
 * - Boss encounters and captain interactions
 * - Score-based victory system (first to 1200 points)
 * - Complex objective hierarchy and dependencies
 * - NPC-driven gameplay with multiple factions
 * - Integration with the base BattleGround class
 *
 * Alterac Valley is a large-scale battleground with multiple objectives, NPCs,
 * towers, and graveyards competing for resource control and ultimate victory.
 */

#include "Player.h"
#include "BattleGround.h"
#include "BattleGroundAV.h"
#include "BattleGroundMgr.h"
#include "Creature.h"
#include "GameObject.h"
#include "Language.h"
#include "WorldPacket.h"
// TODO REMOVE this when graveyard handling for pvp is updated
#include "DBCStores.h"

/**
 * @brief Constructor for BattleGroundAV.
 *
 * Initializes Alterac Valley with default start messages and game state.
 */
BattleGroundAV::BattleGroundAV()
{
    m_StartMessageIds[BG_STARTING_EVENT_FIRST]  = 0;
    m_StartMessageIds[BG_STARTING_EVENT_SECOND] = LANG_BG_AV_START_ONE_MINUTE;
    m_StartMessageIds[BG_STARTING_EVENT_THIRD]  = LANG_BG_AV_START_HALF_MINUTE;
    m_StartMessageIds[BG_STARTING_EVENT_FOURTH] = LANG_BG_AV_HAS_BEGUN;
}

/**
 * @brief Handles a player death in Alterac Valley.
 *
 * Processes player kill events and updates team scores based on losses.
 * Adjusts reputation scores for each team death.
 *
 * @param player Pointer to the killed player.
 * @param killer Pointer to the player who killed them.
 */
void BattleGroundAV::HandleKillPlayer(Player* player, Player* killer)
{
    if (GetStatus() != STATUS_IN_PROGRESS)
    {
        return;
    }

    BattleGround::HandleKillPlayer(player, killer);
    UpdateScore(GetTeamIndexByTeamId(player->GetTeam()), -1);
}

/**
 * @brief Handles the death of an important creature in Alterac Valley.
 *
 * Processes kills of important NPCs using event-based system:
 * - Boss NPCs (ends battle, grants reputation/honor)
 * - Team captains (removes reinforcements, spawns death events)
 * - Mine bosses (changes mine ownership)
 *
 * Uses creature event index to determine NPC type rather than entry.
 *
 * @param creature Pointer to the killed creature.
 * @param killer Pointer to the player who killed the creature.
 */
void BattleGroundAV::HandleKillUnit(Creature* creature, Player* killer)
{
    DEBUG_LOG("BattleGroundAV: HandleKillUnit %i", creature->GetEntry());
    if (GetStatus() != STATUS_IN_PROGRESS)
    {
        return;
    }
    uint8 event1 = (sBattleGroundMgr.GetCreatureEventIndex(creature->GetGUIDLow())).event1;
    if (event1 == BG_EVENT_NONE)
    {
        return;
    }
    switch (event1)
    {
        case BG_AV_BOSS_A:
            CastSpellOnTeam(BG_AV_BOSS_KILL_QUEST_SPELL, HORDE);   // this is a spell which finishes a quest where a player has to kill the boss
            RewardReputationToTeam(BG_AV_FACTION_H, m_RepBoss, HORDE);
            RewardHonorToTeam(GetBonusHonorFromKill(BG_AV_KILL_BOSS), HORDE);
            SendYellToAll(LANG_BG_AV_A_GENERAL_DEAD, LANG_UNIVERSAL, GetSingleCreatureGuid(BG_AV_HERALD, 0));
            EndBattleGround(HORDE);
            break;
        case BG_AV_BOSS_H:
            CastSpellOnTeam(BG_AV_BOSS_KILL_QUEST_SPELL, ALLIANCE); // this is a spell which finishes a quest where a player has to kill the boss
            RewardReputationToTeam(BG_AV_FACTION_A, m_RepBoss, ALLIANCE);
            RewardHonorToTeam(GetBonusHonorFromKill(BG_AV_KILL_BOSS), ALLIANCE);
            SendYellToAll(LANG_BG_AV_H_GENERAL_DEAD, LANG_UNIVERSAL, GetSingleCreatureGuid(BG_AV_HERALD, 0));
            EndBattleGround(ALLIANCE);
            break;
        case BG_AV_CAPTAIN_A:
            if (IsActiveEvent(BG_AV_NodeEventCaptainDead_A, 0))
            {
                return;
            }
            RewardReputationToTeam(BG_AV_FACTION_H, m_RepCaptain, HORDE);
            RewardHonorToTeam(GetBonusHonorFromKill(BG_AV_KILL_CAPTAIN), HORDE);
            UpdateScore(TEAM_INDEX_ALLIANCE, (-1) * BG_AV_RES_CAPTAIN);
            // spawn destroyed aura
            SpawnEvent(BG_AV_NodeEventCaptainDead_A, 0, true);
            break;
        case BG_AV_CAPTAIN_H:
            if (IsActiveEvent(BG_AV_NodeEventCaptainDead_H, 0))
            {
                return;
            }
            RewardReputationToTeam(BG_AV_FACTION_A, m_RepCaptain, ALLIANCE);
            RewardHonorToTeam(GetBonusHonorFromKill(BG_AV_KILL_CAPTAIN), ALLIANCE);
            UpdateScore(TEAM_INDEX_HORDE, (-1) * BG_AV_RES_CAPTAIN);
            // spawn destroyed aura
            SpawnEvent(BG_AV_NodeEventCaptainDead_H, 0, true);
            break;
        case BG_AV_MINE_BOSSES_NORTH:
            ChangeMineOwner(BG_AV_NORTH_MINE, GetAVTeamIndexByTeamId(killer->GetTeam()));
            break;
        case BG_AV_MINE_BOSSES_SOUTH:
            ChangeMineOwner(BG_AV_SOUTH_MINE, GetAVTeamIndexByTeamId(killer->GetTeam()));
            break;
    }
}

/**
 * @brief Handles quest completion in Alterac Valley.
 *
 * Processes various quest types including:
 * - Scrap collection quests (upgrades units)
 * - Commander quests (unlocks upgrades)
 * - Boss quests (turn-in items)
 * - Mine quests (assault preparation)
 * - Rider quests (cavalry preparation)
 *
 * @param questid The ID of the completed quest
 * @param player The player who completed the quest
 */
void BattleGroundAV::HandleQuestComplete(uint32 questid, Player* player)
{
    if (GetStatus() != STATUS_IN_PROGRESS)
    {
        return;
    }
    BattleGroundAVTeamIndex teamIdx = GetAVTeamIndexByTeamId(player->GetTeam());
    MANGOS_ASSERT(teamIdx != BG_AV_TEAM_NEUTRAL);

    uint32 reputation = 0;                                  // reputation for the whole team (other reputation must be done in db)
    // TODO add events (including quest not available anymore, next quest availabe, go/npc de/spawning)
    sLog.outError("BattleGroundAV: Quest %i completed", questid);
    switch (questid)
    {
        case BG_AV_QUEST_A_SCRAPS1:
        case BG_AV_QUEST_A_SCRAPS2:
        case BG_AV_QUEST_H_SCRAPS1:
        case BG_AV_QUEST_H_SCRAPS2:
            m_Team_QuestStatus[teamIdx][0] += 20;
            reputation = 1;
            if (m_Team_QuestStatus[teamIdx][0] == 500 || m_Team_QuestStatus[teamIdx][0] == 1000 || m_Team_QuestStatus[teamIdx][0] == 1500)  // 25,50,75 turn ins
            {
                DEBUG_LOG("BattleGroundAV: Quest %i completed starting with unit upgrading..", questid);
                for (BG_AV_Nodes i = BG_AV_NODES_FIRSTAID_STATION; i <= BG_AV_NODES_FROSTWOLF_HUT; ++i)
                    if (m_Nodes[i].Owner == teamIdx && m_Nodes[i].State == POINT_CONTROLLED)
                    {
                        PopulateNode(i);
                    }
            }
            break;
        case BG_AV_QUEST_A_COMMANDER1:
        case BG_AV_QUEST_H_COMMANDER1:
            m_Team_QuestStatus[teamIdx][1]++;
            reputation = 1;
            if (m_Team_QuestStatus[teamIdx][1] == 120)
            {
                DEBUG_LOG("BattleGroundAV: Quest %i completed (need to implement some events here", questid);
            }
            break;
        case BG_AV_QUEST_A_COMMANDER2:
        case BG_AV_QUEST_H_COMMANDER2:
            m_Team_QuestStatus[teamIdx][2]++;
            reputation = 2;
            if (m_Team_QuestStatus[teamIdx][2] == 60)
            {
                DEBUG_LOG("BattleGroundAV: Quest %i completed (need to implement some events here", questid);
            }
            break;
        case BG_AV_QUEST_A_COMMANDER3:
        case BG_AV_QUEST_H_COMMANDER3:
            m_Team_QuestStatus[teamIdx][3]++;
            reputation = 5;
            if (m_Team_QuestStatus[teamIdx][1] == 30)
            {
                DEBUG_LOG("BattleGroundAV: Quest %i completed (need to implement some events here", questid);
            }
            break;
        case BG_AV_QUEST_A_BOSS1:
        case BG_AV_QUEST_H_BOSS1:
            m_Team_QuestStatus[teamIdx][4] += 4;            // there are 2 quests where you can turn in 5 or 1 item.. ( + 4 cause +1 will be done some lines below)
            reputation = 4;
        case BG_AV_QUEST_A_BOSS2:
        case BG_AV_QUEST_H_BOSS2:
            m_Team_QuestStatus[teamIdx][4]++;
            reputation += 1;
            if (m_Team_QuestStatus[teamIdx][4] >= 200)
            {
                DEBUG_LOG("BattleGroundAV: Quest %i completed (need to implement some events here", questid);
            }
            break;
        case BG_AV_QUEST_A_NEAR_MINE:
        case BG_AV_QUEST_H_NEAR_MINE:
            m_Team_QuestStatus[teamIdx][5]++;
            reputation = 2;
            if (m_Team_QuestStatus[teamIdx][5] == 28)
            {
                DEBUG_LOG("BattleGroundAV: Quest %i completed (need to implement some events here", questid);
                if (m_Team_QuestStatus[teamIdx][6] == 7)
                {
                    DEBUG_LOG("BattleGroundAV: Quest %i completed (need to implement some events here - ground assault ready", questid);
                }
            }
            break;
        case BG_AV_QUEST_A_OTHER_MINE:
        case BG_AV_QUEST_H_OTHER_MINE:
            m_Team_QuestStatus[teamIdx][6]++;
            reputation = 3;
            if (m_Team_QuestStatus[teamIdx][6] == 7)
            {
                DEBUG_LOG("BattleGroundAV: Quest %i completed (need to implement some events here", questid);
                if (m_Team_QuestStatus[teamIdx][5] == 20)
                {
                    DEBUG_LOG("BattleGroundAV: Quest %i completed (need to implement some events here - ground assault ready", questid);
                }
            }
            break;
        case BG_AV_QUEST_A_RIDER_HIDE:
        case BG_AV_QUEST_H_RIDER_HIDE:
            m_Team_QuestStatus[teamIdx][7]++;
            reputation = 1;
            if (m_Team_QuestStatus[teamIdx][7] == 25)
            {
                DEBUG_LOG("BattleGroundAV: Quest %i completed (need to implement some events here", questid);
                if (m_Team_QuestStatus[teamIdx][8] == 25)
                {
                    DEBUG_LOG("BattleGroundAV: Quest %i completed (need to implement some events here - rider assault ready", questid);
                }
            }
            break;
        case BG_AV_QUEST_A_RIDER_TAME:
        case BG_AV_QUEST_H_RIDER_TAME:
            m_Team_QuestStatus[teamIdx][8]++;
            reputation = 1;
            if (m_Team_QuestStatus[teamIdx][8] == 25)
            {
                DEBUG_LOG("BattleGroundAV: Quest %i completed (need to implement some events here", questid);
                if (m_Team_QuestStatus[teamIdx][7] == 25)
                {
                    DEBUG_LOG("BattleGroundAV: Quest %i completed (need to implement some events here - rider assault ready", questid);
                }
            }
            break;
        default:
            DEBUG_LOG("BattleGroundAV: Quest %i completed but is not interesting for us", questid);
            return;
            break;
    }
    if (reputation)
    {
        RewardReputationToTeam((player->GetTeam() == ALLIANCE) ? BG_AV_FACTION_A : BG_AV_FACTION_H, reputation, player->GetTeam());
    }
}

/**
 * @brief Updates team score in Alterac Valley.
 *
 * Manages reinforcement points and win conditions:
 * - Negative points remove reinforcements
 * - Positive points add reinforcements
 * - Ends battle when team runs out
 * - Shows near-loss warnings
 *
 * @param teamIdx The team index to update
 * @param points The points to add (negative removes reinforcements)
 */
void BattleGroundAV::UpdateScore(PvpTeamIndex teamIdx, int32 points)
{
    // note: to remove reinforcements points must be negative, for adding reinforcements points must be positive
    MANGOS_ASSERT(teamIdx == TEAM_INDEX_ALLIANCE || teamIdx == TEAM_INDEX_HORDE);
    m_TeamScores[teamIdx] += points;                      // m_TeamScores is int32 - so no problems here

    if (points < 0)
    {
        if (m_TeamScores[teamIdx] < 1)
        {
            m_TeamScores[teamIdx] = 0;
            // other team will win:
            EndBattleGround((teamIdx == TEAM_INDEX_ALLIANCE) ? HORDE : ALLIANCE);
        }
        else if (!m_IsInformedNearLose[teamIdx] && m_TeamScores[teamIdx] < BG_AV_SCORE_NEAR_LOSE)
        {
            SendMessageToAll((teamIdx == TEAM_INDEX_HORDE) ? LANG_BG_AV_H_NEAR_LOSE : LANG_BG_AV_A_NEAR_LOSE, CHAT_MSG_BG_SYSTEM_NEUTRAL);
            PlaySoundToAll(BG_AV_SOUND_NEAR_LOSE);
            m_IsInformedNearLose[teamIdx] = true;
        }
    }
    // must be called here, else it could display a negative value
    UpdateWorldState(((teamIdx == TEAM_INDEX_HORDE) ? BG_AV_Horde_Score : BG_AV_Alliance_Score), m_TeamScores[teamIdx]);
}

/**
 * @brief Updates Alterac Valley battleground state.
 *
 * Processes timed events:
 * - Mine resource generation and reclamation
 * - Node capture timers and destruction
 * - Base class update for core functionality
 *
 * @param diff Time difference since last update in milliseconds
 */
void BattleGroundAV::Update(uint32 diff)
{
    BattleGround::Update(diff);

    if (GetStatus() != STATUS_IN_PROGRESS)
    {
        return;
    }

    // add points from mine owning, and look if the neutral team can reclaim the mine
    for (uint8 mine = 0; mine < BG_AV_MAX_MINES; ++mine)
    {
        if (m_Mine_Owner[mine] != BG_AV_TEAM_NEUTRAL)
        {
            m_Mine_Timer[mine] -= diff;
            if (m_Mine_Timer[mine] <= 0)
            {
                UpdateScore(PvpTeamIndex(m_Mine_Owner[mine]), WORLD_STATE_ADD);
                m_Mine_Timer[mine] = BG_AV_MINE_TICK_TIMER;
            }

            if (m_Mine_Reclaim_Timer[mine] > diff)
            {
                m_Mine_Reclaim_Timer[mine] -= diff;
            }
            else
            {
                ChangeMineOwner(mine, BG_AV_TEAM_NEUTRAL);
            }
        }
    }

    // looks for all timers of nodes and destroy the building (for graveyards the building wont get destroyed, it goes just to the other team
    for (BG_AV_Nodes i = BG_AV_NODES_FIRSTAID_STATION; i < BG_AV_NODES_MAX; ++i)
    {
        if (m_Nodes[i].State == POINT_ASSAULTED)
        {
            if (m_Nodes[i].Timer > diff)
            {
                m_Nodes[i].Timer -= diff;
            }
            else
            {
                EventPlayerDestroyedPoint(i);
            }
        }
    }
}

/**
 * @brief Opens doors and starts Alterac Valley battle.
 *
 * Shows team scores and opens the main doors.
 * Called when the countdown completes.
 */
void BattleGroundAV::StartingEventOpenDoors()
{
    UpdateWorldState(BG_AV_SHOW_H_SCORE, WORLD_STATE_ADD);
    UpdateWorldState(BG_AV_SHOW_A_SCORE, WORLD_STATE_ADD);

    OpenDoorEvent(BG_EVENT_DOOR);
}

/**
 * @brief Adds a player to Alterac Valley.
 *
 * Creates AV-specific score tracking for the player
 * and adds them to the battleground.
 *
 * @param plr The player to add
 */
void BattleGroundAV::AddPlayer(Player* plr)
{
    BattleGround::AddPlayer(plr);
    // create score and add it to map, default values are set in constructor
    BattleGroundAVScore* sc = new BattleGroundAVScore;
    m_PlayerScores[plr->GetObjectGuid()] = sc;
}

/**
 * @brief Ends Alterac Valley battleground.
 *
 * Calculates and distributes rewards based on:
 * - Surviving towers (bonus honor/reputation)
 * - Controlled graveyards (reputation bonus)
 * - Controlled mines (reputation bonus)
 * - Surviving captains (honor/reputation bonus)
 * - Map completion bonus
 *
 * @param winner The winning team
 */
void BattleGroundAV::EndBattleGround(Team winner)
{
    // calculate bonuskills for both teams:
    uint32 tower_survived[PVP_TEAM_COUNT]  = {0, 0};
    uint32 graves_owned[PVP_TEAM_COUNT]    = {0, 0};
    uint32 mines_owned[PVP_TEAM_COUNT]     = {0, 0};
    // towers all not destroyed:
    for (BG_AV_Nodes i = BG_AV_NODES_DUNBALDAR_SOUTH; i <= BG_AV_NODES_STONEHEART_BUNKER; ++i)
        if (m_Nodes[i].State == POINT_CONTROLLED)
            if (m_Nodes[i].TotalOwner == BG_AV_TEAM_ALLIANCE)
            {
                ++tower_survived[TEAM_INDEX_ALLIANCE];
            }
    for (BG_AV_Nodes i = BG_AV_NODES_ICEBLOOD_TOWER; i <= BG_AV_NODES_FROSTWOLF_WTOWER; ++i)
        if (m_Nodes[i].State == POINT_CONTROLLED)
            if (m_Nodes[i].TotalOwner == BG_AV_TEAM_HORDE)
            {
                ++tower_survived[TEAM_INDEX_HORDE];
            }

    // graves all controlled
    for (BG_AV_Nodes i = BG_AV_NODES_FIRSTAID_STATION; i < BG_AV_NODES_MAX; ++i)
        if (m_Nodes[i].State == POINT_CONTROLLED && m_Nodes[i].Owner != BG_AV_TEAM_NEUTRAL)
        {
            ++graves_owned[m_Nodes[i].Owner];
        }

    for (uint8 i = 0; i < BG_AV_MAX_MINES; ++i)
        if (m_Mine_Owner[i] != BG_AV_TEAM_NEUTRAL)
        {
            ++mines_owned[m_Mine_Owner[i]];
        }

    // now we have the values give the honor/reputation to the teams:
    Team team[PVP_TEAM_COUNT]      = { ALLIANCE, HORDE };
    uint32 faction[PVP_TEAM_COUNT]   = { BG_AV_FACTION_A, BG_AV_FACTION_H };
    for (uint8 i = 0; i < PVP_TEAM_COUNT; ++i)
    {
        if (tower_survived[i])
        {
            RewardReputationToTeam(faction[i], tower_survived[i] * m_RepSurviveTower, team[i]);
            RewardHonorToTeam(GetBonusHonorFromKill(tower_survived[i] * BG_AV_KILL_SURVIVING_TOWER), team[i]);
        }
        DEBUG_LOG("BattleGroundAV: EndbattleGround: bgteam: %u towers:%u honor:%u rep:%u", i, tower_survived[i], GetBonusHonorFromKill(tower_survived[i] * BG_AV_KILL_SURVIVING_TOWER), tower_survived[i] * BG_AV_REP_SURVIVING_TOWER);
        if (graves_owned[i])
        {
            RewardReputationToTeam(faction[i], graves_owned[i] * m_RepOwnedGrave, team[i]);
        }
        if (mines_owned[i])
        {
            RewardReputationToTeam(faction[i], mines_owned[i] * m_RepOwnedMine, team[i]);
        }
        // captain survived?:
        if (!IsActiveEvent(BG_AV_NodeEventCaptainDead_A + GetTeamIndexByTeamId(team[i]), 0))
        {
            RewardReputationToTeam(faction[i], m_RepSurviveCaptain, team[i]);
            RewardHonorToTeam(GetBonusHonorFromKill(BG_AV_KILL_SURVIVING_CAPTAIN), team[i]);
        }
    }

    // both teams:
    if (m_HonorMapComplete)
    {
        RewardHonorToTeam(m_HonorMapComplete, ALLIANCE);
        RewardHonorToTeam(m_HonorMapComplete, HORDE);
    }
    BattleGround::EndBattleGround(winner);
}

/**
 * @brief Handles area trigger in Alterac Valley.
 *
 * Processes team-specific area triggers:
 * - Alliance-only areas (removes Horde players)
 * - Horde-only areas (removes Alliance players)
 *
 * Note: Official implementation uses gameobject spells
 *
 * @param source The player triggering the area
 * @param trigger The trigger ID
 * @return true if trigger was handled, false otherwise
 */
bool BattleGroundAV::HandleAreaTrigger(Player* source, uint32 trigger)
{
    // this is wrong way to implement these things. On official it done by gameobject spell cast.
    switch (trigger)
    {
        case 95:
        case 2608:
            if (source->GetTeam() != ALLIANCE)
            {
                source->GetSession()->SendNotification(LANG_BATTLEGROUND_ONLY_ALLIANCE_USE);
            }
            else
            {
                source->LeaveBattleground();
            }
            break;
        case 2606:
            if (source->GetTeam() != HORDE)
            {
                source->GetSession()->SendNotification(LANG_BATTLEGROUND_ONLY_HORDE_USE);
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
 * @brief Updates a player's score in Alterac Valley.
 *
 * Tracks AV-specific achievements like graveyards and towers assaulted/defended,
 * as well as secondary objectives. Also handles generic battle ground score updates.
 *
 * @param source Pointer to the player.
 * @param type The score type to update (SCORE_GRAVEYARDS_ASSAULTED, etc.).
 * @param value The value to add to the score.
 */
void BattleGroundAV::UpdatePlayerScore(Player* source, uint32 type, uint32 value)
{
    BattleGroundScoreMap::iterator itr = m_PlayerScores.find(source->GetObjectGuid());
    if (itr == m_PlayerScores.end())                        // player not found...
    {
        return;
    }

    switch (type)
    {
        case SCORE_GRAVEYARDS_ASSAULTED:
            ((BattleGroundAVScore*)itr->second)->GraveyardsAssaulted += value;
            break;
        case SCORE_GRAVEYARDS_DEFENDED:
            ((BattleGroundAVScore*)itr->second)->GraveyardsDefended += value;
            break;
        case SCORE_TOWERS_ASSAULTED:
            ((BattleGroundAVScore*)itr->second)->TowersAssaulted += value;
            break;
        case SCORE_TOWERS_DEFENDED:
            ((BattleGroundAVScore*)itr->second)->TowersDefended += value;
            break;
        case SCORE_SECONDARY_OBJECTIVES:
            ((BattleGroundAVScore*)itr->second)->SecondaryObjectives += value;
            break;
        default:
            BattleGround::UpdatePlayerScore(source, type, value);
            break;
    }
}

/**
 * @brief Processes when a player destroys a point (tower/graveyard).
 *
 * Handles the destruction of controlled objectives, changing ownership,
 * despawning banners, and populating the node with neutral creatures.
 *
 * @param node The node index that was destroyed.
 */
void BattleGroundAV::EventPlayerDestroyedPoint(BG_AV_Nodes node)
{
    DEBUG_LOG("BattleGroundAV: player destroyed point node %i", node);

    MANGOS_ASSERT(m_Nodes[node].Owner != BG_AV_TEAM_NEUTRAL)
    PvpTeamIndex ownerTeamIdx = PvpTeamIndex(m_Nodes[node].Owner);
    Team ownerTeam = ownerTeamIdx == TEAM_INDEX_ALLIANCE ? ALLIANCE : HORDE;

    // despawn banner
    DestroyNode(node);
    PopulateNode(node);
    UpdateNodeWorldState(node);

    if (IsTower(node))
    {
        uint8 tmp = node - BG_AV_NODES_DUNBALDAR_SOUTH;
        // despawn marshal (one of those guys protecting the boss)
        SpawnEvent(BG_AV_MARSHAL_A_SOUTH + tmp, 0, false);

        UpdateScore(GetOtherTeamIndex(ownerTeamIdx), (-1) * BG_AV_RES_TOWER);
        RewardReputationToTeam((ownerTeam == ALLIANCE) ? BG_AV_FACTION_A : BG_AV_FACTION_H, m_RepTowerDestruction, ownerTeam);
        RewardHonorToTeam(GetBonusHonorFromKill(BG_AV_KILL_TOWER), ownerTeam);
        SendYell2ToAll(LANG_BG_AV_TOWER_TAKEN, LANG_UNIVERSAL, GetSingleCreatureGuid(BG_AV_HERALD, 0), GetNodeName(node), (ownerTeam == ALLIANCE) ? LANG_BG_ALLY : LANG_BG_HORDE);
    }
    else
    {
        SendYell2ToAll(LANG_BG_AV_GRAVE_TAKEN, LANG_UNIVERSAL, GetSingleCreatureGuid(BG_AV_HERALD, 0), GetNodeName(node), (ownerTeam == ALLIANCE) ? LANG_BG_ALLY : LANG_BG_HORDE);
    }
}

/**
 * @brief Changes ownership of an Alterac Valley mine.
 *
 * Updates mine ownership state, refreshes mine-related world states, spawns the
 * correct mine events for the new owner, and announces the capture when a team
 * successfully takes control.
 *
 * @param mine The mine index to update.
 * @param teamIdx The team that will own the mine after the change.
 */
void BattleGroundAV::ChangeMineOwner(uint8 mine, BattleGroundAVTeamIndex teamIdx)
{
    m_Mine_Timer[mine] = BG_AV_MINE_TICK_TIMER;
    // TODO implement quest 7122
    // mine=0 northmine, mine=1 southmine
    // TODO changing the owner should result in setting respawntime to infinite for current creatures (they should fight the new ones), spawning new mine owners creatures and changing the chest - objects so that the current owning team can use them
    MANGOS_ASSERT(mine == BG_AV_NORTH_MINE || mine == BG_AV_SOUTH_MINE);
    if (m_Mine_Owner[mine] == teamIdx)
    {
        return;
    }

    m_Mine_PrevOwner[mine] = m_Mine_Owner[mine];
    m_Mine_Owner[mine] = teamIdx;

    SendMineWorldStates(mine);

    SpawnEvent(BG_AV_MINE_EVENT + mine, teamIdx, true);
    SpawnEvent(BG_AV_MINE_BOSSES + mine, teamIdx, true);

    if (teamIdx != BG_AV_TEAM_NEUTRAL)
    {
        PlaySoundToAll((teamIdx == BG_AV_TEAM_ALLIANCE) ? BG_AV_SOUND_ALLIANCE_GOOD : BG_AV_SOUND_HORDE_GOOD);
        m_Mine_Reclaim_Timer[mine] = BG_AV_MINE_RECLAIM_TIMER;
        SendYell2ToAll(LANG_BG_AV_MINE_TAKEN , LANG_UNIVERSAL, GetSingleCreatureGuid(BG_AV_HERALD, 0),
                       (teamIdx == BG_AV_TEAM_ALLIANCE) ? LANG_BG_ALLY : LANG_BG_HORDE,
                       (mine == BG_AV_NORTH_MINE) ? LANG_BG_AV_MINE_NORTH : LANG_BG_AV_MINE_SOUTH);
    }
}

/**
 * @brief Checks whether a player can interact with a mine quest object.
 *
 * Validates ownership of the north or south mine against the player's team.
 * Non-mine objects are treated as valid.
 *
 * @param GOId The game object entry identifier.
 * @param team The player's team.
 * @return true if the player can use the quest object; otherwise, false.
 */
bool BattleGroundAV::PlayerCanDoMineQuest(int32 GOId, Team team)
{
    if (GOId == BG_AV_OBJECTID_MINE_N)
    {
        return (m_Mine_Owner[BG_AV_NORTH_MINE] == GetAVTeamIndexByTeamId(team));
    }
    if (GOId == BG_AV_OBJECTID_MINE_S)
    {
        return (m_Mine_Owner[BG_AV_SOUTH_MINE] == GetAVTeamIndexByTeamId(team));
    }
    return true;                                            // cause it's no mine'object it is ok if this is true
}

/// will spawn and despawn creatures around a node
/// more a wrapper around spawnevent cause graveyards are special
void BattleGroundAV::PopulateNode(BG_AV_Nodes node)
{
    BattleGroundAVTeamIndex teamIdx = m_Nodes[node].Owner;
    if (IsGrave(node) && teamIdx != BG_AV_TEAM_NEUTRAL)
    {
        uint32 graveDefenderType;
        if (m_Team_QuestStatus[teamIdx][0] < 500)
        {
            graveDefenderType = 0;
        }
        else if (m_Team_QuestStatus[teamIdx][0] < 1000)
        {
            graveDefenderType = 1;
        }
        else if (m_Team_QuestStatus[teamIdx][0] < 1500)
        {
            graveDefenderType = 2;
        }
        else
        {
            graveDefenderType = 3;
        }

        if (m_Nodes[node].State == POINT_CONTROLLED) // we can spawn the current owner event
        {
            SpawnEvent(BG_AV_NODES_MAX + node, teamIdx * BG_AV_MAX_GRAVETYPES + graveDefenderType, true);
        }
        else // we despawn the event from the prevowner
        {
            SpawnEvent(BG_AV_NODES_MAX + node, m_Nodes[node].PrevOwner * BG_AV_MAX_GRAVETYPES + graveDefenderType, false);
        }
    }
    SpawnEvent(node, (teamIdx * BG_AV_MAX_STATES) + m_Nodes[node].State, true);
}

/// called when using a banner
void BattleGroundAV::EventPlayerClickedOnFlag(Player* source, GameObject* target_obj)
{
    if (GetStatus() != STATUS_IN_PROGRESS)
    {
        return;
    }
    DEBUG_LOG("BattleGroundAV: using gameobject %i", target_obj->GetEntry());
    uint8 event = (sBattleGroundMgr.GetGameObjectEventIndex(target_obj->GetGUIDLow())).event1;
    if (event >= BG_AV_NODES_MAX)                           // not a node
    {
        return;
    }
    BG_AV_Nodes node = BG_AV_Nodes(event);
    switch ((sBattleGroundMgr.GetGameObjectEventIndex(target_obj->GetGUIDLow())).event2 % BG_AV_MAX_STATES)
    {
        case POINT_CONTROLLED:
            EventPlayerAssaultsPoint(source, node);
            break;
        case POINT_ASSAULTED:
            EventPlayerDefendsPoint(source, node);
            break;
        default:
            break;
    }
}

/**
 * @brief Handles when a player defends a point in Alterac Valley.
 *
 * Processes the successful defense of towers and graveyards, updating node ownership,
 * populating defenders, sending announcements, and updating player scores.
 *
 * @param player Pointer to the player defending the point.
 * @param node The node index being defended.
 */
void BattleGroundAV::EventPlayerDefendsPoint(Player* player, BG_AV_Nodes node)
{
    MANGOS_ASSERT(GetStatus() == STATUS_IN_PROGRESS);

    PvpTeamIndex teamIdx = GetTeamIndexByTeamId(player->GetTeam());

    if (m_Nodes[node].Owner == BattleGroundAVTeamIndex(teamIdx) || m_Nodes[node].State != POINT_ASSAULTED)
    {
        return;
    }
    if (m_Nodes[node].TotalOwner == BG_AV_TEAM_NEUTRAL)     // initial snowfall capture
    {
        // until snowfall doesn't belong to anyone it is better handled in assault - code (best would be to have a special function
        // for neutral nodes.. but doing this just for snowfall will be a bit to much i think
        MANGOS_ASSERT(node == BG_AV_NODES_SNOWFALL_GRAVE);  // currently the only neutral grave
        EventPlayerAssaultsPoint(player, node);
        return;
    }

    DEBUG_LOG("BattleGroundAV: player defends node: %i", node);
    if (m_Nodes[node].PrevOwner != BattleGroundAVTeamIndex(teamIdx))
    {
        sLog.outError("BattleGroundAV: player defends point which doesn't belong to his team %i", node);
        return;
    }

    DefendNode(node, teamIdx);                              // set the right variables for nodeinfo
    PopulateNode(node);                                     // spawn node-creatures (defender for example)
    UpdateNodeWorldState(node);                             // send new mapicon to the player

    if (IsTower(node))
    {
        SendYell2ToAll(LANG_BG_AV_TOWER_DEFENDED, LANG_UNIVERSAL, GetSingleCreatureGuid(BG_AV_HERALD, 0),
                       GetNodeName(node),
                       (teamIdx == TEAM_INDEX_ALLIANCE) ? LANG_BG_ALLY : LANG_BG_HORDE);
        UpdatePlayerScore(player, SCORE_TOWERS_DEFENDED, 1);
        PlaySoundToAll(BG_AV_SOUND_BOTH_TOWER_DEFEND);
    }
    else
    {
        SendYell2ToAll(LANG_BG_AV_GRAVE_DEFENDED, LANG_UNIVERSAL, GetSingleCreatureGuid(BG_AV_HERALD, 0),
                       GetNodeName(node),
                       (teamIdx == TEAM_INDEX_ALLIANCE) ? LANG_BG_ALLY : LANG_BG_HORDE);
        UpdatePlayerScore(player, SCORE_GRAVEYARDS_DEFENDED, 1);
        // update the statistic for the defending player
        PlaySoundToAll((teamIdx == TEAM_INDEX_ALLIANCE) ? BG_AV_SOUND_ALLIANCE_GOOD : BG_AV_SOUND_HORDE_GOOD);
    }
}

/**
 * @brief Handles when a player assaults a point in Alterac Valley.
 *
 * Processes the assault of towers and graveyards, updating node ownership from enemy control
 * to a contested state. Sends announcements, updates player scores, and plays appropriate sounds.
 *
 * @param player Pointer to the player assaulting the point.
 * @param node The node index being assaulted.
 */
void BattleGroundAV::EventPlayerAssaultsPoint(Player* player, BG_AV_Nodes node)
{
    // TODO implement quest 7101, 7081
    PvpTeamIndex teamIdx  = GetTeamIndexByTeamId(player->GetTeam());
    DEBUG_LOG("BattleGroundAV: player assaults node %i", node);
    if (m_Nodes[node].Owner == BattleGroundAVTeamIndex(teamIdx) || BattleGroundAVTeamIndex(teamIdx) == m_Nodes[node].TotalOwner)
    {
        return;
    }

    AssaultNode(node, teamIdx);                             // update nodeinfo variables
    UpdateNodeWorldState(node);                             // send mapicon
    PopulateNode(node);

    if (IsTower(node))
    {
        SendYell2ToAll(LANG_BG_AV_TOWER_ASSAULTED, LANG_UNIVERSAL, GetSingleCreatureGuid(BG_AV_HERALD, 0),
                       GetNodeName(node),
                       (teamIdx == TEAM_INDEX_ALLIANCE) ? LANG_BG_ALLY : LANG_BG_HORDE);
        UpdatePlayerScore(player, SCORE_TOWERS_ASSAULTED, 1);
    }
    else
    {
        SendYell2ToAll(LANG_BG_AV_GRAVE_ASSAULTED, LANG_UNIVERSAL, GetSingleCreatureGuid(BG_AV_HERALD, 0),
                       GetNodeName(node),
                       (teamIdx == TEAM_INDEX_ALLIANCE) ? LANG_BG_ALLY : LANG_BG_HORDE);
        // update the statistic for the assaulting player
        UpdatePlayerScore(player, SCORE_GRAVEYARDS_ASSAULTED, 1);
    }

    PlaySoundToAll((teamIdx == TEAM_INDEX_ALLIANCE) ? BG_AV_SOUND_ALLIANCE_ASSAULTS : BG_AV_SOUND_HORDE_ASSAULTS);
}

/**
 * @brief Fills initial world state values for Alterac Valley.
 *
 * Sends all node state information to clients when they enter the battleground,
 * including node ownership, captured towers, and graveyard status.
 *
 * @param data The packet to write world state data to.
 * @param count Reference to the count of world state entries.
 */
void BattleGroundAV::FillInitialWorldStates(WorldPacket& data, uint32& count)
{
    bool stateok;
    for (uint8 i = BG_AV_NODES_FIRSTAID_STATION; i < BG_AV_NODES_MAX; ++i)
    {
        for (uint8 j = 0; j < BG_AV_MAX_STATES; ++j)
        {
            stateok = (m_Nodes[i].State == j);
            FillInitialWorldState(data, count, BG_AV_NodeWorldStates[i][GetWorldStateType(j, BG_AV_TEAM_ALLIANCE)],
                                  m_Nodes[i].Owner == BG_AV_TEAM_ALLIANCE && stateok);
            FillInitialWorldState(data, count, BG_AV_NodeWorldStates[i][GetWorldStateType(j, BG_AV_TEAM_HORDE)],
                                  m_Nodes[i].Owner == BG_AV_TEAM_HORDE && stateok);
        }
    }

    if (m_Nodes[BG_AV_NODES_SNOWFALL_GRAVE].Owner == BG_AV_TEAM_NEUTRAL)    // cause neutral teams aren't handled generic
    {
        FillInitialWorldState(data, count, AV_SNOWFALL_N, WORLD_STATE_ADD);
    }

    FillInitialWorldState(data, count, BG_AV_Alliance_Score, m_TeamScores[TEAM_INDEX_ALLIANCE]);
    FillInitialWorldState(data, count, BG_AV_Horde_Score,    m_TeamScores[TEAM_INDEX_HORDE]);
    if (GetStatus() == STATUS_IN_PROGRESS)                  // only if game is running the teamscores are displayed
    {
        FillInitialWorldState(data, count, BG_AV_SHOW_A_SCORE, WORLD_STATE_ADD);
        FillInitialWorldState(data, count, BG_AV_SHOW_H_SCORE, WORLD_STATE_ADD);
    }
    else
    {
        FillInitialWorldState(data, count, BG_AV_SHOW_A_SCORE, WORLD_STATE_REMOVE);
        FillInitialWorldState(data, count, BG_AV_SHOW_H_SCORE, WORLD_STATE_REMOVE);
    }

    FillInitialWorldState(data, count, BG_AV_MineWorldStates[BG_AV_NORTH_MINE][m_Mine_Owner[BG_AV_NORTH_MINE]], WORLD_STATE_ADD);
    if (m_Mine_Owner[BG_AV_NORTH_MINE] != m_Mine_PrevOwner[BG_AV_NORTH_MINE])
    {
        FillInitialWorldState(data, count, BG_AV_MineWorldStates[BG_AV_NORTH_MINE][m_Mine_PrevOwner[BG_AV_NORTH_MINE]], WORLD_STATE_REMOVE);
    }

    FillInitialWorldState(data, count, BG_AV_MineWorldStates[BG_AV_SOUTH_MINE][m_Mine_Owner[BG_AV_SOUTH_MINE]], WORLD_STATE_ADD);
    if (m_Mine_Owner[BG_AV_SOUTH_MINE] != m_Mine_PrevOwner[BG_AV_SOUTH_MINE])
    {
        FillInitialWorldState(data, count, BG_AV_MineWorldStates[BG_AV_SOUTH_MINE][m_Mine_PrevOwner[BG_AV_SOUTH_MINE]], WORLD_STATE_REMOVE);
    }
}

/**
 * @brief Updates the displayed world state for a single node.
 *
 * Adds the current node state to the client world state display and removes the
 * previous one, including special handling for the neutral Snowfall graveyard.
 *
 * @param node The node whose world state should be refreshed.
 */
void BattleGroundAV::UpdateNodeWorldState(BG_AV_Nodes node)
{
    UpdateWorldState(BG_AV_NodeWorldStates[node][GetWorldStateType(m_Nodes[node].State, m_Nodes[node].Owner)], WORLD_STATE_ADD);
    if (m_Nodes[node].PrevOwner == BG_AV_TEAM_NEUTRAL)      // currently only snowfall is supported as neutral node
    {
        UpdateWorldState(AV_SNOWFALL_N, WORLD_STATE_REMOVE);
    }
    else
    {
        UpdateWorldState(BG_AV_NodeWorldStates[node][GetWorldStateType(m_Nodes[node].PrevState, m_Nodes[node].PrevOwner)], WORLD_STATE_REMOVE);
    }
}

/**
 * @brief Sends mine ownership world state updates to all clients.
 *
 * Updates the world state to reflect which team currently owns a mine (North or South).
 * Removes the previous owner's world state and adds the new owner's state.
 *
 * @param mine The mine index (BG_AV_NORTH_MINE or BG_AV_SOUTH_MINE).
 */
void BattleGroundAV::SendMineWorldStates(uint32 mine)
{
    MANGOS_ASSERT(mine == BG_AV_NORTH_MINE || mine == BG_AV_SOUTH_MINE);

    UpdateWorldState(BG_AV_MineWorldStates[mine][m_Mine_Owner[mine]], WORLD_STATE_ADD);
    if (m_Mine_Owner[mine] != m_Mine_PrevOwner[mine])
    {
        UpdateWorldState(BG_AV_MineWorldStates[mine][m_Mine_PrevOwner[mine]], WORLD_STATE_REMOVE);
    }
}

/**
 * @brief Finds the closest valid graveyard for a player.
 *
 * Searches all controlled graveyards for the player's team and returns the nearest
 * available location. If no controlled graveyard is available, the team cave spawn
 * is used as a fallback.
 *
 * @param plr The player requesting a graveyard location.
 * @return Pointer to the closest valid graveyard entry.
 */
WorldSafeLocsEntry const* BattleGroundAV::GetClosestGraveYard(Player* plr)
{
    float x = plr->GetPositionX();
    float y = plr->GetPositionY();
    BattleGroundAVTeamIndex teamIdx = GetAVTeamIndexByTeamId(plr->GetTeam());
    WorldSafeLocsEntry const* good_entry = NULL;
    if (GetStatus() == STATUS_IN_PROGRESS)
    {
        // Is there any occupied node for this team?
        float mindist = 9999999.0f;
        for (uint8 i = BG_AV_NODES_FIRSTAID_STATION; i <= BG_AV_NODES_FROSTWOLF_HUT; ++i)
        {
            if (m_Nodes[i].Owner != teamIdx || m_Nodes[i].State != POINT_CONTROLLED)
            {
                continue;
            }
            WorldSafeLocsEntry const* entry = sWorldSafeLocsStore.LookupEntry(BG_AV_GraveyardIds[i]);
            if (!entry)
            {
                continue;
            }
            float dist = (entry->x - x) * (entry->x - x) + (entry->y - y) * (entry->y - y);
            if (mindist > dist)
            {
                mindist = dist;
                good_entry = entry;
            }
        }
    }
    // If not, place ghost in the starting-cave
    if (!good_entry)
    {
        good_entry = sWorldSafeLocsStore.LookupEntry(BG_AV_GraveyardIds[teamIdx + 7]);
    }

    return good_entry;
}

/**
 * @brief Gets the language string ID for a node name.
 *
 * Returns the appropriate language entry for the given node's display name.
 *
 * @param node The node index.
 * @return The language entry ID for the node name.
 */
uint32 BattleGroundAV::GetNodeName(BG_AV_Nodes node) const
{
    switch (node)
    {
        case BG_AV_NODES_FIRSTAID_STATION:  return LANG_BG_AV_NODE_GRAVE_STORM_AID;
        case BG_AV_NODES_DUNBALDAR_SOUTH:   return LANG_BG_AV_NODE_TOWER_DUN_S;
        case BG_AV_NODES_DUNBALDAR_NORTH:   return LANG_BG_AV_NODE_TOWER_DUN_N;
        case BG_AV_NODES_STORMPIKE_GRAVE:   return LANG_BG_AV_NODE_GRAVE_STORMPIKE;
        case BG_AV_NODES_ICEWING_BUNKER:    return LANG_BG_AV_NODE_TOWER_ICEWING;
        case BG_AV_NODES_STONEHEART_GRAVE:  return LANG_BG_AV_NODE_GRAVE_STONE;
        case BG_AV_NODES_STONEHEART_BUNKER: return LANG_BG_AV_NODE_TOWER_STONE;
        case BG_AV_NODES_SNOWFALL_GRAVE:    return LANG_BG_AV_NODE_GRAVE_SNOW;
        case BG_AV_NODES_ICEBLOOD_TOWER:    return LANG_BG_AV_NODE_TOWER_ICE;
        case BG_AV_NODES_ICEBLOOD_GRAVE:    return LANG_BG_AV_NODE_GRAVE_ICE;
        case BG_AV_NODES_TOWER_POINT:       return LANG_BG_AV_NODE_TOWER_POINT;
        case BG_AV_NODES_FROSTWOLF_GRAVE:   return LANG_BG_AV_NODE_GRAVE_FROST;
        case BG_AV_NODES_FROSTWOLF_ETOWER:  return LANG_BG_AV_NODE_TOWER_FROST_E;
        case BG_AV_NODES_FROSTWOLF_WTOWER:  return LANG_BG_AV_NODE_TOWER_FROST_W;
        case BG_AV_NODES_FROSTWOLF_HUT:     return LANG_BG_AV_NODE_GRAVE_FROST_HUT;
        default:
            return 0;
            break;
    }
}

/**
 * @brief Handles assault of a node in Alterac Valley.
 *
 * Updates node state to reflect ongoing assault. Sets appropriate capture timers based on
 * previous ownership (neutral vs. previously owned).
 *
 * @param node The node index being assaulted.
 * @param teamIdx The team assaulting the node.
 */
void BattleGroundAV::AssaultNode(BG_AV_Nodes node, PvpTeamIndex teamIdx)
{
    MANGOS_ASSERT(m_Nodes[node].TotalOwner != BattleGroundAVTeamIndex(teamIdx));
    MANGOS_ASSERT(m_Nodes[node].Owner != BattleGroundAVTeamIndex(teamIdx));
    // only assault an assaulted node if no totalowner exists:
    MANGOS_ASSERT(m_Nodes[node].State != POINT_ASSAULTED || m_Nodes[node].TotalOwner == BG_AV_TEAM_NEUTRAL);
    // the timer gets another time, if the previous owner was 0 == Neutral
    m_Nodes[node].Timer      = (m_Nodes[node].PrevOwner != BG_AV_TEAM_NEUTRAL) ? BG_AV_CAPTIME : BG_AV_SNOWFALL_FIRSTCAP;
    m_Nodes[node].PrevOwner  = m_Nodes[node].Owner;
    m_Nodes[node].Owner      = BattleGroundAVTeamIndex(teamIdx);
    m_Nodes[node].PrevState  = m_Nodes[node].State;
    m_Nodes[node].State      = POINT_ASSAULTED;
}

/**
 * @brief Destroys a node in Alterac Valley.
 *
 * Removes controlled nodes, despawning associated creatures and objects.
 * Used when a node is captured or contested.
 *
 * @param node The node index to destroy.
 */
void BattleGroundAV::DestroyNode(BG_AV_Nodes node)
{
    MANGOS_ASSERT(m_Nodes[node].State == POINT_ASSAULTED);

    m_Nodes[node].TotalOwner = m_Nodes[node].Owner;
    m_Nodes[node].PrevOwner  = m_Nodes[node].Owner;
    m_Nodes[node].PrevState  = m_Nodes[node].State;
    m_Nodes[node].State      = POINT_CONTROLLED;
    m_Nodes[node].Timer      = 0;
}

/**
 * @brief Initializes a node to its starting ownership and state.
 *
 * Sets the initial owner, previous owner, control state, timer, and active event
 * data for a node when Alterac Valley is reset or created.
 *
 * @param node The node to initialize.
 * @param teamIdx The starting owner of the node.
 * @param tower true if the node is a tower; otherwise, false.
 */
void BattleGroundAV::InitNode(BG_AV_Nodes node, BattleGroundAVTeamIndex teamIdx, bool tower)
{
    m_Nodes[node].TotalOwner = teamIdx;
    m_Nodes[node].Owner      = teamIdx;
    m_Nodes[node].PrevOwner  = teamIdx;
    m_Nodes[node].State      = POINT_CONTROLLED;
    m_Nodes[node].PrevState  = m_Nodes[node].State;
    m_Nodes[node].State      = POINT_CONTROLLED;
    m_Nodes[node].Timer      = 0;
    m_Nodes[node].Tower      = tower;
    m_ActiveEvents[node] = teamIdx * BG_AV_MAX_STATES + m_Nodes[node].State;
    if (IsGrave(node))                                      // grave-creatures are special cause of a quest
    {
        m_ActiveEvents[node + BG_AV_NODES_MAX]  = teamIdx * BG_AV_MAX_GRAVETYPES;
    }
}

/**
 * @brief Defends a node in Alterac Valley.
 *
 * Updates node state when a previously contested node is successfully defended
 * and returns to full team control.
 *
 * @param node The node index being defended.
 * @param teamIdx The team defending the node.
 */
void BattleGroundAV::DefendNode(BG_AV_Nodes node, PvpTeamIndex teamIdx)
{
    MANGOS_ASSERT(m_Nodes[node].TotalOwner == BattleGroundAVTeamIndex(teamIdx));
    MANGOS_ASSERT(m_Nodes[node].Owner != BattleGroundAVTeamIndex(teamIdx));
    MANGOS_ASSERT(m_Nodes[node].State != POINT_CONTROLLED);
    m_Nodes[node].PrevOwner  = m_Nodes[node].Owner;
    m_Nodes[node].Owner      = BattleGroundAVTeamIndex(teamIdx);
    m_Nodes[node].PrevState  = m_Nodes[node].State;
    m_Nodes[node].State      = POINT_CONTROLLED;
    m_Nodes[node].Timer      = 0;
}

/**
 * @brief Resets Alterac Valley to initial state.
 *
 * Resets all node states, mine ownership, team scores, quest status, and reputation/honor
 * values. Accounts for weekend event bonuses when calculating rewards.
 */
void BattleGroundAV::Reset()
{
    BattleGround::Reset();
    // set the reputation and honor variables:
    bool isBGWeekend = BattleGroundMgr::IsBGWeekend(GetTypeID());

    m_HonorMapComplete    = (isBGWeekend) ? BG_AV_KILL_MAP_COMPLETE_HOLIDAY : BG_AV_KILL_MAP_COMPLETE;
    m_RepTowerDestruction = (isBGWeekend) ? BG_AV_REP_TOWER_HOLIDAY         : BG_AV_REP_TOWER;
    m_RepCaptain          = (isBGWeekend) ? BG_AV_REP_CAPTAIN_HOLIDAY       : BG_AV_REP_CAPTAIN;
    m_RepBoss             = (isBGWeekend) ? BG_AV_REP_BOSS_HOLIDAY          : BG_AV_REP_BOSS;
    m_RepOwnedGrave       = (isBGWeekend) ? BG_AV_REP_OWNED_GRAVE_HOLIDAY   : BG_AV_REP_OWNED_GRAVE;
    m_RepSurviveCaptain   = (isBGWeekend) ? BG_AV_REP_SURVIVING_CAPTAIN_HOLIDAY : BG_AV_REP_SURVIVING_CAPTAIN;
    m_RepSurviveTower     = (isBGWeekend) ? BG_AV_REP_SURVIVING_TOWER_HOLIDAY : BG_AV_REP_SURVIVING_TOWER;
    m_RepOwnedMine        = (isBGWeekend) ? BG_AV_REP_OWNED_MINE_HOLIDAY    : BG_AV_REP_OWNED_MINE;

    for (uint8 i = 0; i < PVP_TEAM_COUNT; ++i)
    {
        for (uint8 j = 0; j < 9; ++j)                       // 9 quests getting tracked
        {
            m_Team_QuestStatus[i][j] = 0;
        }
        m_TeamScores[i]         = BG_AV_SCORE_INITIAL_POINTS;
        m_IsInformedNearLose[i] = false;
        m_ActiveEvents[BG_AV_NodeEventCaptainDead_A + i] = BG_EVENT_NONE;
    }

    for (uint8 i = 0; i < BG_AV_MAX_MINES; ++i)
    {
        m_Mine_Owner[i] = BG_AV_TEAM_NEUTRAL;
        m_Mine_PrevOwner[i] = m_Mine_Owner[i];
        m_ActiveEvents[BG_AV_MINE_BOSSES + i] = BG_AV_TEAM_NEUTRAL;
        m_ActiveEvents[BG_AV_MINE_EVENT + i] = BG_AV_TEAM_NEUTRAL;
        m_Mine_Timer[i] = BG_AV_MINE_TICK_TIMER;
    }

    m_ActiveEvents[BG_AV_CAPTAIN_A] = 0;
    m_ActiveEvents[BG_AV_CAPTAIN_H] = 0;
    m_ActiveEvents[BG_AV_HERALD] = 0;
    m_ActiveEvents[BG_AV_BOSS_A] = 0;
    m_ActiveEvents[BG_AV_BOSS_H] = 0;
    for (BG_AV_Nodes i = BG_AV_NODES_DUNBALDAR_SOUTH; i <= BG_AV_NODES_FROSTWOLF_WTOWER; ++i)  // towers
    {
        m_ActiveEvents[BG_AV_MARSHAL_A_SOUTH + i - BG_AV_NODES_DUNBALDAR_SOUTH] = 0;
    }

    for (BG_AV_Nodes i = BG_AV_NODES_FIRSTAID_STATION; i <= BG_AV_NODES_STONEHEART_GRAVE; ++i)  // alliance graves
    {
        InitNode(i, BG_AV_TEAM_ALLIANCE, false);
    }
    for (BG_AV_Nodes i = BG_AV_NODES_DUNBALDAR_SOUTH; i <= BG_AV_NODES_STONEHEART_BUNKER; ++i)  // alliance towers
    {
        InitNode(i, BG_AV_TEAM_ALLIANCE, true);
    }

    for (BG_AV_Nodes i = BG_AV_NODES_ICEBLOOD_GRAVE; i <= BG_AV_NODES_FROSTWOLF_HUT; ++i)       // horde graves
    {
        InitNode(i, BG_AV_TEAM_HORDE, false);
    }
    for (BG_AV_Nodes i = BG_AV_NODES_ICEBLOOD_TOWER; i <= BG_AV_NODES_FROSTWOLF_WTOWER; ++i)    // horde towers
    {
        InitNode(i, BG_AV_TEAM_HORDE, true);
    }

    InitNode(BG_AV_NODES_SNOWFALL_GRAVE, BG_AV_TEAM_NEUTRAL, false);                            // give snowfall neutral owner
}

/// <summary>
/// Gets the premature finish winning team.
/// </summary>
Team BattleGroundAV::GetPrematureWinner()
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
