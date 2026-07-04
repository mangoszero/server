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

/// \addtogroup shared
/// @{
/// \file

#ifndef MANGOS_H_GDB_EVENTS
#define MANGOS_H_GDB_EVENTS

#include <cstdint>

/*
 * Catalogue of game-level breakpoint events for the GDB-server debug
 * endpoint. This enum lives in the shared layer so both the lower-level
 * shared subsystems (e.g. the database) and the game library refer to the
 * same event ids. Keep Count <= 64 so the armed set fits in a single
 * bitmask word, and keep the GdbBp::kEventNames table (game) in lockstep.
 */
enum class GdbEvent : std::uint32_t
{
    // --- core gameplay ----------------------------------------------------
    Opcode,          ///< received client opcode          (filter: opcode)
    Login,           ///< player login                    (filter: account id)
    Logout,          ///< player logout                   (filter: account id)
    MapEnter,        ///< object added to a map           (filter: map id)
    MapLeave,        ///< object removed from a map       (filter: map id)
    SpellCast,       ///< spell cast                      (filter: spell id)
    SpellPrepare,    ///< spell prepared                  (filter: spell id)
    Death,           ///< unit died                       (filter: entry)
    Damage,          ///< damage dealt                    (filter: victim entry)
    LevelUp,         ///< player gained a level           (filter: new level)
    Loot,            ///< loot opened                     (filter: loot type)
    QuestAccept,     ///< quest accepted                  (filter: quest id)
    QuestComplete,   ///< quest completed                 (filter: quest id)
    QuestReward,     ///< quest rewarded                  (filter: quest id)
    Chat,            ///< chat message handled            (filter: 0)
    ItemUse,         ///< item used                       (filter: 0)
    GossipSelect,    ///< gossip option selected          (filter: gossip id)
    CreatureCreate,  ///< creature created                (filter: entry)
    GameObjectUse,   ///< game object used                (filter: entry)
    GmCommand,       ///< chat/console command parsed     (filter: 0)
    WorldTick,       ///< world heartbeat (single-step)   (filter: 0)

    // --- netcode / auth ---------------------------------------------------
    NetAccept,       ///< world socket accepted           (filter: 0)
    NetClose,        ///< world socket closed             (filter: 0)
    AuthSession,     ///< CMSG_AUTH_SESSION handled       (filter: 0)
    PacketRecv,      ///< packet read from a socket       (filter: opcode)
    PacketSend,      ///< packet queued to a socket       (filter: opcode)

    // --- database (shared layer) ------------------------------------------
    DbQuery,         ///< SQL query executed              (filter: 0)
    DbExecute,       ///< SQL statement executed          (filter: 0)
    DbAsyncQuery,    ///< async SQL query queued          (filter: 0)

    // --- warden anti-cheat ------------------------------------------------
    WardenCheck,     ///< a warden check is sent          (filter: 0)
    WardenViolation, ///< a warden check failed           (filter: 0)

    // --- scripting --------------------------------------------------------
    ScriptAI,        ///< a creature AI is instantiated   (filter: entry)
    ElunaHook,       ///< an Eluna hook fires             (filter: 0)
    Sd3Hook,         ///< a ScriptDev3 script runs        (filter: 0)

    // --- creature AI ------------------------------------------------------
    AiCombat,        ///< AI enters combat                (filter: entry)
    AiCombatEnd,     ///< AI leaves combat                (filter: entry)
    AiUpdate,        ///< AI update tick                  (filter: entry)
    AiSpawn,         ///< creature (re)spawns             (filter: entry)

    // --- maps / grids / instances -----------------------------------------
    MapCreate,       ///< a map is created                (filter: map id)
    GridLoad,        ///< a grid is loaded                (filter: map id)
    InstanceCreate,  ///< an instance is created          (filter: map id)
    InstanceReset,   ///< an instance is reset            (filter: map id)

    // --- mail / auction / trade -------------------------------------------
    MailSend,        ///< mail is sent                    (filter: 0)
    MailReceive,     ///< mail is taken                   (filter: 0)
    AuctionAdd,      ///< auction listed                  (filter: 0)
    AuctionBuy,      ///< auction bought                  (filter: 0)
    Trade,           ///< trade completed                 (filter: 0)

    // --- battleground / pet / item / group / pvp / movement ---------------
    BgStart,         ///< battleground starts             (filter: type id)
    BgEnd,           ///< battleground ends               (filter: type id)
    PetSummon,       ///< a pet is summoned               (filter: entry)
    ItemEquip,       ///< an item is equipped             (filter: entry)
    ItemDestroy,     ///< an item is destroyed            (filter: entry)
    GroupJoin,       ///< a player joins a group          (filter: 0)
    PvpKill,         ///< an honorable kill is credited   (filter: 0)
    MovementInform,  ///< a movement generator reports    (filter: 0)

    Count
};

#endif
/// @}
