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



#include "Player.h"
#include "Language.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "Opcodes.h"
#include "SpellMgr.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "UpdateMask.h"
#include "QuestDef.h"
#include "GossipDef.h"
#include "UpdateData.h"
#include "Channel.h"
#include "ChannelMgr.h"
#include "MapManager.h"
#include "MapPersistentStateMgr.h"
#include "InstanceData.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "ObjectMgr.h"
#include "ObjectAccessor.h"
#include "CreatureAI.h"
#include "Formulas.h"
#include "Group.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Pet.h"
#include "Util.h"
#include "Transports.h"
#include "Weather.h"
#include "BattleGround/BattleGround.h"
#include "BattleGround/BattleGroundMgr.h"
#include "BattleGround/BattleGroundAV.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "Chat.h"
#include "revision_data.h"
#include "Database/DatabaseImpl.h"
#include "Spell.h"
#include "ScriptMgr.h"
#include "SocialMgr.h"
#include "Mail.h"
#include "SpellAuras.h"
#include "DBCStores.h"
#include "SQLStorages.h"
#include "DisableMgr.h"
#include "CinematicFlyover.h"
#include <cmath>
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */
#ifdef ENABLE_PLAYERBOTS
#include "playerbot.h"
#endif /* ENABLE_PLAYERBOTS */

// If players are too far way of duel flag... then player loose the duel
void Player::CheckDuelDistance(time_t currTime)
{
    if (!duel)
    {
        return;
    }

    uint64 duelFlagGUID = GetUInt64Value(PLAYER_DUEL_ARBITER);
    GameObject* obj = GetMap()->GetGameObject(duelFlagGUID);
    if (!obj)
    {
        return;
    }

    if (duel->outOfBound == 0)
    {
        if (!IsWithinDistInMap(obj, 80))
        {
            duel->outOfBound = currTime;

            WorldPacket data(SMSG_DUEL_OUTOFBOUNDS, 0);
            GetSession()->SendPacket(&data);
        }
    }
    else
    {
        if (IsWithinDistInMap(obj, 70))
        {
            duel->outOfBound = 0;

            WorldPacket data(SMSG_DUEL_INBOUNDS, 0);
            GetSession()->SendPacket(&data);
        }
        else if (currTime >= (duel->outOfBound + 10))
        {
            DuelComplete(DUEL_FLED);
        }
    }
}

/**
 * @brief Finalizes an active duel and cleans up all duel state.
 *
 * @param type The duel completion result.
 */
void Player::DuelComplete(DuelCompleteType type)
{
    // duel not requested
    if (!duel)
    {
        return;
    }

    WorldPacket data(SMSG_DUEL_COMPLETE, (1));
    data << (uint8)((type != DUEL_INTERRUPTED) ? 1 : 0);
    GetSession()->SendPacket(&data);

    if (duel->opponent->GetSession())
    {
        duel->opponent->GetSession()->SendPacket(&data);
    }

    if (type != DUEL_INTERRUPTED)
    {
        data.Initialize(SMSG_DUEL_WINNER, (1 + 20));          // we guess size
        data << (uint8)((type == DUEL_WON) ? 0 : 1);          // 0 = just won; 1 = fled
        data << duel->opponent->GetName();
        data << GetName();
        SendMessageToSet(&data, true);
    }

    switch (type)
    {
        case DUEL_FLED:
            // if initiator and opponent are on the same team
            // or initiator and opponent are not PvP enabled, forcibly stop attacking
            if (duel->initiator->GetTeam() == duel->opponent->GetTeam())
            {
                duel->initiator->AttackStop();
                duel->opponent->AttackStop();
            }
            else
            {
                if (!duel->initiator->IsPvP())
                {
                    duel->initiator->AttackStop();
                }
                if (!duel->opponent->IsPvP())
                {
                    duel->opponent->AttackStop();
                }
            }
        default:
            break;
    }

    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (Eluna* e = GetEluna())
    {
        e->OnDuelEnd(duel->opponent, this, type);
    }
#endif /* ENABLE_ELUNA */

    // Remove Duel Flag object
    GameObject* obj = GetMap()->GetGameObject(GetUInt64Value(PLAYER_DUEL_ARBITER));
    if (obj)
    {
        duel->initiator->RemoveGameObject(obj, true);
    }

    /* remove auras */
    // TODO: Needs a simpler method
    std::vector<uint32> auras2remove;
    SpellAuraHolderMap const& vAuras = duel->opponent->GetSpellAuraHolderMap();
    for (SpellAuraHolderMap::const_iterator i = vAuras.begin(); i != vAuras.end(); ++i)
    {
        SpellAuraHolder const* aura = i->second;
        if (!aura->IsPositive() && aura->GetCasterGuid() == GetObjectGuid() && aura->GetAuraApplyTime() >= duel->startTime)
        {
            auras2remove.push_back(aura->GetId());
        }
    }

    for (size_t i = 0; i < auras2remove.size(); ++i)
    {
        duel->opponent->RemoveAurasDueToSpell(auras2remove[i]);
    }

    auras2remove.clear();
    SpellAuraHolderMap const& auras = GetSpellAuraHolderMap();
    for (SpellAuraHolderMap::const_iterator i = auras.begin(); i != auras.end(); ++i)
    {
        SpellAuraHolder const* aura = i->second;
        if (!aura->IsPositive() && aura->GetCasterGuid() == duel->opponent->GetObjectGuid() && aura->GetAuraApplyTime() >= duel->startTime)
        {
            auras2remove.push_back(aura->GetId());
        }
    }
    for (size_t i = 0; i < auras2remove.size(); ++i)
    {
        RemoveAurasDueToSpell(auras2remove[i]);
    }

    // cleanup combo points
    if (GetComboTargetGuid() == duel->opponent->GetObjectGuid())
    {
        ClearComboPoints();
    }
    else if (GetComboTargetGuid() == duel->opponent->GetPetGuid())
    {
        ClearComboPoints();
    }

    if (duel->opponent->GetComboTargetGuid() == GetObjectGuid())
    {
        duel->opponent->ClearComboPoints();
    }
    else if (duel->opponent->GetComboTargetGuid() == GetPetGuid())
    {
        duel->opponent->ClearComboPoints();
    }

    // cleanups
    SetGuidValue(PLAYER_DUEL_ARBITER, ObjectGuid());
    SetUInt32Value(PLAYER_DUEL_TEAM, 0);
    duel->opponent->SetGuidValue(PLAYER_DUEL_ARBITER, ObjectGuid());
    duel->opponent->SetUInt32Value(PLAYER_DUEL_TEAM, 0);

    delete duel->opponent->duel;
    duel->opponent->duel = NULL;
    delete duel;
    duel = NULL;
}

/**
 * @brief Sends the duel countdown timer to the client.
 *
 * @param counter The countdown value in seconds.
 */
void Player::SendDuelCountdown(uint32 counter)
{
    WorldPacket data(SMSG_DUEL_COUNTDOWN, 4);
    data << uint32(counter);                                // seconds
    GetSession()->SendPacket(&data);
}
