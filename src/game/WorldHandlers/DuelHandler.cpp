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
 * @file DuelHandler.cpp
 * @brief Player duel request handling
 *
 * This file implements handlers for duel-related opcodes:
 * - CMSG_DUEL_ACCEPTED: Target player accepts the duel
 * - CMSG_DUEL_CANCELLED: Player cancels or forfeits the duel
 *
 * Duel lifecycle:
 * 1. Challenger sends duel request (handled elsewhere)
 * 2. Target accepts (HandleDuelAcceptedOpcode)
 * 3. 3-second countdown begins
 * 4. Duel starts (players can attack each other)
 * 5. Duel ends by forfeit, death, or distance
 */

#include "Common.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Log.h"
#include "Player.h"

/**
 * @brief Handle duel acceptance from the challenged player
 * @param recvPacket World packet containing opponent GUID
 *
 * Validates the duel request and initiates the countdown if accepted.
 * Only the player who was challenged can accept (not the initiator).
 *
 * On success, both players receive a 3-second countdown before the
 * duel officially begins.
 */
void WorldSession::HandleDuelAcceptedOpcode(WorldPacket& recvPacket)
{
    ObjectGuid guid;
    recvPacket >> guid;

    if (!GetPlayer()->duel)                                 // ignore accept from duel-sender
    {
        return;
    }

    Player* pl       = GetPlayer();
    Player* plTarget = pl->duel->opponent;

    if (pl == pl->duel->initiator || !plTarget || pl == plTarget || pl->duel->startTime != 0 || plTarget->duel->startTime != 0)
    {
        return;
    }

    DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "WORLD: received CMSG_DUEL_ACCEPTED");
    DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "Player 1 is: %u (%s)", pl->GetGUIDLow(), pl->GetName());
    DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "Player 2 is: %u (%s)", plTarget->GetGUIDLow(), plTarget->GetName());

    time_t now = time(NULL);
    pl->duel->startTimer = now;
    plTarget->duel->startTimer = now;

    pl->SendDuelCountdown(3000);
    plTarget->SendDuelCountdown(3000);
}

/**
 * @brief Handle duel cancellation or forfeit
 * @param recvPacket World packet (may contain opponent GUID)
 *
 * Handles two scenarios:
 * 1. Active duel forfeit: If duel has started, caster surrenders
 *    and casts "Beg" emote (spell 7267)
 * 2. Request cancellation: If duel hasn't started, simply cancels the request
 *
 * @note /forfeit command also triggers this handler
 */
void WorldSession::HandleDuelCancelledOpcode(WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_DUEL_CANCELLED");

    // no duel requested
    if (!GetPlayer()->duel)
    {
        return;
    }

    // player surrendered in a duel using /forfeit
    if (GetPlayer()->duel->startTime != 0)
    {
        GetPlayer()->CombatStopWithPets(true);
        if (GetPlayer()->duel->opponent)
        {
            GetPlayer()->duel->opponent->CombatStopWithPets(true);
        }

        GetPlayer()->CastSpell(GetPlayer(), 7267, true);    // beg
        GetPlayer()->DuelComplete(DUEL_WON);
        return;
    }

    // player either discarded the duel using the "discard button"
    // or used "/forfeit" before countdown reached 0
    ObjectGuid guid;
    recvPacket >> guid;

    GetPlayer()->DuelComplete(DUEL_INTERRUPTED);
}
