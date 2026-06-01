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
 * @file SkillHandler.cpp
 * @brief Character talent and skill management handlers
 *
 * This file handles player-initiated talent and skill operations:
 * - LearnTalent: Spending talent points to acquire talents
 * - TalentWipeConfirm: Resetting all talents (via trainer)
 * - UnlearnSkill: Abandoning a profession skill
 *
 * These are distinct from automatic skill gains from crafting/usage,
 * which are handled elsewhere.
 *
 * @note Talent wipes require interaction with a class trainer NPC
 */

#include "Common.h"
#include "Opcodes.h"
#include "Log.h"
#include "Player.h"
#include "WorldPacket.h"
#include "WorldSession.h"

/**
 * @brief Handle talent learning (CMSG_LEARN_TALENT)
 * @param recv_data World packet containing talent_id and requested_rank
 *
 * Player spends talent points to learn or upgrade a talent.
 * Packet data:
 * - talent_id: ID from Talent.dbc
 * - requested_rank: Rank to learn (0-based)
 *
 * Validation and point deduction handled by Player::LearnTalent().
 * If player has an active pet, owner talent auras are recast on it.
 */
void WorldSession::HandleLearnTalentOpcode(WorldPacket& recv_data)
{
    uint32 talent_id, requested_rank;
    recv_data >> talent_id >> requested_rank;

    _player->LearnTalent(talent_id, requested_rank);

    // if player has a pet, update owner talent auras
    if (_player->GetPet())
    {
        _player->GetPet()->CastOwnerTalentAuras();
    }
}

/**
 * @brief Handle talent wipe confirmation (MSG_TALENT_WIPE_CONFIRM)
 * @param recv_data World packet containing trainer GUID
 *
 * Player confirms talent reset at a class trainer. Requirements:
 * - Target must be a trainer NPC
 * - NPC can train and reset talents for player's class
 * - Costs money (handled by resetTalents())
 *
 * Visual effect (spell 14867) is cast by the trainer on the player.
 * Pet talent auras are recast if player has an active pet.
 *
 * @note Player cannot be feign death during the interaction
 */
void WorldSession::HandleTalentWipeConfirmOpcode(WorldPacket& recv_data)
{
    DETAIL_LOG("MSG_TALENT_WIPE_CONFIRM");
    ObjectGuid guid;
    recv_data >> guid;

    Creature* unit = GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_TRAINER);
    if (!unit)
    {
        DEBUG_LOG("WORLD: HandleTalentWipeConfirmOpcode - %s not found or you can't interact with him.", guid.GetString().c_str());
        return;
    }

    if (!unit->CanTrainAndResetTalentsOf(_player))
    {
        return;
    }

    // Remove fake death to allow interaction
    if (GetPlayer()->hasUnitState(UNIT_STAT_DIED))
    {
        GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);
    }

    if (!(_player->resetTalents()))
    {
        WorldPacket data(MSG_TALENT_WIPE_CONFIRM, 8 + 4);   // No talents to reset
        data << uint64(0);
        data << uint32(0);
        SendPacket(&data);
        return;
    }

    // Visual effect: "Untalent Visual Effect"
    unit->CastSpell(_player, 14867, true);

    // Recast owner talent auras on pet if present
    if (_player->GetPet())
    {
        _player->GetPet()->CastOwnerTalentAuras();
    }
}

/**
 * @brief Handle skill unlearning (CMSG_UNLEARN_SKILL)
 * @param recv_data World packet containing skill_id
 *
 * Player abandons a profession or secondary skill.
 * Sets skill level and maximum to 0, effectively removing it.
 *
 * @warning This action is permanent and removes all skill progress
 * @note Does not refund any costs or recipe purchases
 */
void WorldSession::HandleUnlearnSkillOpcode(WorldPacket& recv_data)
{
    uint32 skill_id;
    recv_data >> skill_id;
    GetPlayer()->SetSkill(skill_id, 0, 0);
}
