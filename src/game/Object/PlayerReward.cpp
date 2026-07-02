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

// Used in triggers for check "Only to targets that grant experience or honor" req
bool Player::isHonorOrXPTarget(Unit* pVictim) const
{
    uint32 v_level = pVictim->getLevel();
    uint32 k_grey  = MaNGOS::XP::GetGrayLevel(getLevel());

    // Victim level less gray level
    if (v_level <= k_grey)
    {
        return false;
    }

    if (pVictim->GetTypeId() == TYPEID_UNIT)
    {
        if (((Creature*)pVictim)->IsTotem() ||
            ((Creature*)pVictim)->IsPet() ||
            ((Creature*)pVictim)->GetCreatureInfo()->ExtraFlags & CREATURE_FLAG_EXTRA_NO_XP_AT_KILL)
        {
            return false;
        }
    }
    return true;
}

/**
 * @brief Rewards the player for killing a unit outside group reward distribution.
 *
 * @param pVictim The killed unit.
 */
void Player::RewardSinglePlayerAtKill(Unit* pVictim)
{
    bool PvP = pVictim->IsCharmedOwnedByPlayerOrPlayer();

    uint32 xp = PvP ? 0 : MaNGOS::XP::Gain(this, pVictim);

    // honor can be in PvP and !PvP (racial leader) cases
    RewardHonor(pVictim, 1);

    // xp and reputation only in !PvP case
    if (!PvP)
    {
        RewardReputation(pVictim, 1);
        GiveXP(xp, pVictim);

        if (Pet* pet = GetPet())
        {
            pet->GivePetXP(xp);
        }

        // normal creature (not pet/etc) can be only in !PvP case
        if (pVictim->GetTypeId() == TYPEID_UNIT)
        {
            KilledMonster(((Creature*)pVictim)->GetCreatureInfo(), pVictim->GetObjectGuid());
        }
    }
}

/**
 * @brief Grants event kill credit to the player or nearby group members.
 *
 * @param creature_id The credited creature entry identifier.
 * @param pRewardSource The world object used for distance checks.
 */
void Player::RewardPlayerAndGroupAtEvent(uint32 creature_id, WorldObject* pRewardSource)
{
    MANGOS_ASSERT((!GetGroup() || pRewardSource));              // Player::RewardPlayerAndGroupAtEvent called for Group-Case but no source for range searching provided

    ObjectGuid creature_guid = pRewardSource && pRewardSource->GetTypeId() == TYPEID_UNIT ? pRewardSource->GetObjectGuid() : ObjectGuid();

    // prepare data for near group iteration
    if (Group* pGroup = GetGroup())
    {
        for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
        {
            Player* pGroupGuy = itr->getSource();
            if (!pGroupGuy)
            {
                continue;
            }

            if (!pGroupGuy->IsAtGroupRewardDistance(pRewardSource))
            {
                continue; // member (alive or dead) or his corpse at req. distance
            }

            // quest objectives updated only for alive group member or dead but with not released body
            if (pGroupGuy->IsAlive() || !pGroupGuy->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST))
            {
                pGroupGuy->KilledMonsterCredit(creature_id, creature_guid);
            }
        }
    }
    else                                                    // if (!pGroup)
    {
        KilledMonsterCredit(creature_id, creature_guid);
    }
}

/**
 * @brief Grants quest cast credit to the player or nearby group members.
 *
 * @param pRewardSource The credited creature or gameobject.
 * @param spellid The spell that granted the credit.
 */
void Player::RewardPlayerAndGroupAtCast(WorldObject* pRewardSource, uint32 spellid)
{
    // prepare data for near group iteration
    if (Group* pGroup = GetGroup())
    {
        for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
        {
            Player* pGroupGuy = itr->getSource();
            if (!pGroupGuy)
            {
                continue;
            }

            if (!pGroupGuy->IsAtGroupRewardDistance(pRewardSource))
            {
                continue; // member (alive or dead) or his corpse at req. distance
            }

            // quest objectives updated only for alive group member or dead but with not released body
            if (pGroupGuy->IsAlive() || !pGroupGuy->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST))
            {
                pGroupGuy->CastedCreatureOrGO(pRewardSource->GetEntry(), pRewardSource->GetObjectGuid(), spellid, pGroupGuy == this);
            }
        }
    }
    else                                                    // if (!pGroup)
    {
        CastedCreatureOrGO(pRewardSource->GetEntry(), pRewardSource->GetObjectGuid(), spellid);
    }
}

/**
 * @brief Checks whether the player or corpse is close enough for shared rewards.
 *
 * @param pRewardSource The source object used for the distance check.
 * @return True if the player qualifies for group reward range; otherwise, false.
 */
bool Player::IsAtGroupRewardDistance(WorldObject const* pRewardSource) const
{
    if (pRewardSource->IsWithinDistInMap(this, sWorld.getConfig(CONFIG_FLOAT_GROUP_XP_DISTANCE)))
    {
        return true;
    }

    if (IsAlive())
    {
        return false;
    }

    Corpse* corpse = GetCorpse();
    if (!corpse)
    {
        return false;
    }

    return pRewardSource->IsWithinDistInMap(corpse, sWorld.getConfig(CONFIG_FLOAT_GROUP_XP_DISTANCE));
}
