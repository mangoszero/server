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

/**
 * @brief Checks whether this player should be visible to another player in grid range.
 *
 * @param pl The observing player.
 * @return True if this player should be visible; otherwise, false.
 */
bool Player::IsVisibleInGridForPlayer(Player* pl) const
{
    // gamemaster in GM mode see all, including ghosts
    if (pl->isGameMaster() && GetSession()->GetSecurity() <= pl->GetSession()->GetSecurity())
    {
        return true;
    }

    // player see dead player/ghost from own group/raid
    if (IsInSameRaidWith(pl))
    {
        return true;
    }

    // Live player see live player or dead player with not realized corpse
    if (pl->IsAlive() || pl->m_deathTimer > 0)
    {
        return IsAlive() || m_deathTimer > 0;
    }

    // Ghost see other friendly ghosts, that's for sure
    if (!(IsAlive() || m_deathTimer > 0) && IsFriendlyTo(pl))
    {
        return true;
    }

    // Dead player see live players near own corpse
    if (IsAlive())
    {
        if (Corpse* corpse = pl->GetCorpse())
        {
            // 20 - aggro distance for same level, 25 - max additional distance if player level less that creature level
            if (corpse->IsWithinDistInMap(this, (20 + 25) * sWorld.getConfig(CONFIG_FLOAT_RATE_CREATURE_AGGRO)))
            {
                return true;
            }
        }
    }

    // and not see any other
    return false;
}

/**
 * @brief Checks whether this player should appear in global player visibility contexts.
 *
 * @param u The player attempting to see this player.
 * @return True if this player is globally visible; otherwise, false.
 */
bool Player::IsVisibleGloballyFor(Player* u) const
{
    if (!u)
    {
        return false;
    }

    // Always can see self
    if (u == this)
    {
        return true;
    }

    // Visible units, always are visible for all players
    if (GetVisibility() == VISIBILITY_ON)
    {
        return true;
    }

    // GMs are visible for higher gms (or players are visible for gms)
    if (u->GetSession()->GetSecurity() > SEC_PLAYER)
    {
        return GetSession()->GetSecurity() <= u->GetSession()->GetSecurity();
    }

    // non faction visibility non-breakable for non-GMs
    if (GetVisibility() == VISIBILITY_OFF)
    {
        return false;
    }

    // non-gm stealth/invisibility not hide from global player lists
    return true;
}

/**
 * @brief Performs cleanup before an object is removed from a player's visibility.
 *
 * @param o The world object about to be hidden.
 * @param p The player losing visibility of the object.
 */
inline void BeforeVisibilityDestroy(WorldObject* o, Player* p)
{
    if (Creature* t = o->ToCreature())
    {
        if (p->GetPetGuid() == t->GetObjectGuid() && t->IsPet())
        {
            ((Pet*)t)->Unsummon(PET_SAVE_REAGENTS);
        }
    }
}

/**
 * @brief Updates visibility of a single world object for the player.
 *
 * @param viewPoint The viewpoint used for visibility checks.
 * @param target The target object whose visibility is being updated.
 */
void Player::UpdateVisibilityOf(WorldObject const* viewPoint, WorldObject* target)
{
    if (HaveAtClient(target))
    {
        if (!target->IsVisibleForInState(this, viewPoint, true))
        {
            if (m_transport)
            {
                Transport* targetTransport = nullptr;
                if (target->GetTypeId() == TYPEID_PLAYER)
                {
                    targetTransport = ((Player*)target)->GetTransport();
                }
                else if (target->GetTypeId() == TYPEID_UNIT &&
                         target->ToCreature()->IsPet())
                {
                    targetTransport = ((Pet*)target->ToCreature())->GetTransport();
                }
                if (targetTransport && targetTransport == m_transport)
                {
                    return;
                }
            }

            ObjectGuid t_guid = target->GetObjectGuid();

            if (target->GetTypeId() == TYPEID_UNIT)
            {
                Creature* c = target->ToCreature();
                if (c->IsPet() && GetPetGuid() == c->GetObjectGuid() && ((Pet*)c)->GetTransport())
                {
                    return;
                }
                BeforeVisibilityDestroy(target, this);
            }

            target->DestroyForPlayer(this);
            m_clientGUIDs.erase(t_guid);

            DEBUG_FILTER_LOG(LOG_FILTER_VISIBILITY_CHANGES, "UpdateVisibilityOf(2p): %s out of range for player %u. Distance = %f", t_guid.GetString().c_str(), GetGUIDLow(), GetDistance(target));
        }
    }
    else
    {
        if (target->IsVisibleForInState(this, viewPoint, false))
        {
            target->SendCreateUpdateToPlayer(this);
            if (target->GetTypeId() != TYPEID_GAMEOBJECT || !((GameObject*)target)->IsTransport())
            {
                m_clientGUIDs.insert(target->GetObjectGuid());
            }

            DEBUG_FILTER_LOG(LOG_FILTER_VISIBILITY_CHANGES, "UpdateVisibilityOf(2p): %s is visible now for player %u. Distance = %f", target->GetGuidStr().c_str(), GetGUIDLow(), GetDistance(target));

            // target aura duration for caster show only if target exist at caster client
            // send data at target visibility change (adding to client)
            if (target != this && target->isType(TYPEMASK_UNIT))
            {
                SendAuraDurationsForTarget((Unit*)target);
            }
        }
    }
}

//4 params version (4p)
void Player::UpdateVisibilityOf(WorldObject const* viewPoint, WorldObject* target, UpdateData& data, std::set<WorldObject*>& visibleNow)
{
    if (HaveAtClient(target))
    {
        if (!target->IsVisibleForInState(this, viewPoint, true))
        {
            if (m_transport)
            {
                Transport* targetTransport = nullptr;
                if (target->GetTypeId() == TYPEID_PLAYER)
                {
                    targetTransport = ((Player*)target)->GetTransport();
                }
                else if (target->GetTypeId() == TYPEID_UNIT &&
                         target->ToCreature()->IsPet())
                {
                    targetTransport = ((Pet*)target->ToCreature())->GetTransport();
                }
                if (targetTransport && targetTransport == m_transport)
                {
                    return;
                }
            }

            if (target->GetTypeId() == TYPEID_UNIT)
            {
                Creature* c = target->ToCreature();
                if (c->IsPet() && GetPetGuid() == c->GetObjectGuid() && ((Pet*)c)->GetTransport())
                {
                    return;
                }
            }

            BeforeVisibilityDestroy(target, this);

            ObjectGuid t_guid = target->GetObjectGuid();

            target->BuildOutOfRangeUpdateBlock(&data);
            m_clientGUIDs.erase(t_guid);

            DEBUG_FILTER_LOG(LOG_FILTER_VISIBILITY_CHANGES, "UpdateVisibilityOf(4p): %s is out of range for %s. Distance = %f", t_guid.GetString().c_str(), GetGuidStr().c_str(), GetDistance(target));
        }
    }
    else
    {
        if (target->IsVisibleForInState(this, viewPoint, false))
        {
            visibleNow.insert(target);
            target->BuildCreateUpdateBlockForPlayer(&data, this);
            if (GameObject* g = target->ToGameObject())
            {
                if (!g->IsTransport())
                {
                    m_clientGUIDs.insert(g->GetObjectGuid());
                }
            }
            else
            {
                m_clientGUIDs.insert(target->GetObjectGuid());
            }
            DEBUG_FILTER_LOG(LOG_FILTER_VISIBILITY_CHANGES, "UpdateVisibilityOf(4p): %s is visible now for %s. Distance = %f", target->GetGuidStr().c_str(), GetGuidStr().c_str(), GetDistance(target));
        }
    }
}
