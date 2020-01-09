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
#include "ObjectGuid.h"
#include "Player.h"

void WorldSession::HandleAttackSwingOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;
    recv_data >> guid;

    DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "WORLD: Received opcode CMSG_ATTACKSWING %s", guid.GetString().c_str());

    if (!guid.IsUnit())
    {
        sLog.outError("WORLD: %s isn't unit", guid.GetString().c_str());
        return;
    }

    Unit* pEnemy = _player->GetMap()->GetUnit(guid);

    if (!pEnemy)
    {
        sLog.outError("WORLD: Enemy %s not found", guid.GetString().c_str());

        // stop attack state at client
        SendAttackStop(NULL);
        return;
    }

    if (_player->IsFriendlyTo(pEnemy) || pEnemy->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE))
    {
        sLog.outError("WORLD: Enemy %s is friendly", guid.GetString().c_str());

        // stop attack state at client
        SendAttackStop(pEnemy);
        return;
    }

    if (!pEnemy->IsAlive())
    {
        // client can generate swing to known dead target if autoswitch between autoshot and autohit is enabled in client options
        // stop attack state at client
        SendAttackStop(pEnemy);
        return;
    }

    _player->Attack(pEnemy, true);
}

void WorldSession::HandleAttackStopOpcode(WorldPacket& /*recv_data*/)
{
    GetPlayer()->AttackStop();
}

void WorldSession::HandleSetSheathedOpcode(WorldPacket& recv_data)
{
    uint32 sheathed;
    recv_data >> sheathed;

    DEBUG_LOG("WORLD: Received opcode CMSG_SETSHEATHED for %s - value: %u", GetPlayer()->GetGuidStr().c_str(), sheathed);

    if (sheathed >= MAX_SHEATH_STATE)
    {
        sLog.outError("Unknown sheath state %u ??", sheathed);
        return;
    }

    GetPlayer()->SetSheath(SheathState(sheathed));
}

void WorldSession::SendAttackStop(Unit const* enemy)
{
    WorldPacket data(SMSG_ATTACKSTOP, (4 + 20));            // we guess size
    data << GetPlayer()->GetPackGUID();
    data << (enemy ? enemy->GetPackGUID() : PackedGuid());  // must be packed guid
    data << uint32(0);                                      // unk, can be 1 also
    SendPacket(&data);
}
