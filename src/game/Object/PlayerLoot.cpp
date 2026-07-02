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
 * @brief Converts the player's battleground corpse into lootable bones after insignia removal.
 *
 * @param looterPlr The player removing the insignia.
 */
void Player::RemovedInsignia(Player* looterPlr)
{
    if (!GetBattleGroundId())
    {
        return;
    }

    // If not released spirit, do it !
    if (m_deathTimer > 0)
    {
        m_deathTimer = 0;
        BuildPlayerRepop();
        RepopAtGraveyard();
    }

    Corpse* corpse = GetCorpse();
    if (!corpse)
    {
        return;
    }

    // We have to convert player corpse to bones, not to be able to resurrect there
    // SpawnCorpseBones isn't handy, 'cos it saves player while he in BG
    Corpse* bones = sObjectAccessor.ConvertCorpseForPlayer(GetObjectGuid(), true);
    if (!bones)
    {
        return;
    }

    // Now we must make bones lootable, and send player loot
    bones->SetFlag(CORPSE_FIELD_DYNAMIC_FLAGS, CORPSE_DYNFLAG_LOOTABLE);

    // We store the level of our player in the gold field
    // We retrieve this information at Player::SendLoot()
    bones->loot.gold = getLevel();
    bones->lootRecipient = looterPlr;
    looterPlr->SendLoot(bones->GetObjectGuid(), LOOT_INSIGNIA);
}

/**
 * @brief Sends a loot release response for a loot object.
 *
 * @param guid The GUID of the released loot source.
 */
void Player::SendLootRelease(ObjectGuid guid)
{
    WorldPacket data(SMSG_LOOT_RELEASE_RESPONSE, (8 + 1));
    data << guid;
    data << uint8(1);
    SendDirectMessage(&data);
}

/**
 * @brief Opens and sends loot contents for a supported loot source.
 *
 * @param guid The GUID of the loot source.
 * @param loot_type The requested loot interaction type.
 */
void Player::SendLoot(ObjectGuid guid, LootType loot_type)
{
    if (ObjectGuid lootGuid = GetLootGuid())
    {
        m_session->DoLootRelease(lootGuid);
    }

    Loot* loot = NULL;
    PermissionTypes permission = ALL_PERMISSION;

    DEBUG_LOG("Player::SendLoot");
    switch (guid.GetHigh())
    {
        case HIGHGUID_GAMEOBJECT:
        {
            DEBUG_LOG("       IS_GAMEOBJECT_GUID(guid)");
            GameObject* go = GetMap()->GetGameObject(guid);

            // not check distance for GO in case owned GO (fishing bobber case, for example)
            // And permit out of range GO with no owner in case fishing hole
            if (!go || (loot_type != LOOT_FISHINGHOLE && ((loot_type != LOOT_FISHING && loot_type != LOOT_FISHING_FAIL) || go->GetOwnerGuid() != GetObjectGuid()) && !go->IsWithinDistInMap(this, INTERACTION_DISTANCE)))
            {
                SendLootRelease(guid);
                return;
            }

            GameObjectInfo const* goInfo = go->GetGOInfo();

            loot = &go->loot;

            Player* recipient = go->GetLootRecipient();
            if (!recipient)
            {
                go->SetLootRecipient(this);
                recipient = this;
            }

            // generate loot only if ready for open and spawned in world and not already looted once.
            if (go->getLootState() == GO_READY && go->isSpawned())
            {
                uint32 lootid = goInfo->GetLootId();
                if ((go->GetEntry() == BG_AV_OBJECTID_MINE_N || go->GetEntry() == BG_AV_OBJECTID_MINE_S))
                {
                    if (BattleGround* bg = GetBattleGround())
                    {
                        if (bg->GetTypeID() == BATTLEGROUND_AV)
                        {
                            if (!(((BattleGroundAV*)bg)->PlayerCanDoMineQuest(go->GetEntry(), GetTeam())))
                            {
                                SendLootRelease(guid);
                                return;
                            }
                        }
                    }
                }

                loot->clear();
                switch (loot_type)
                {
                    // Entry 0 in fishing loot template used for store junk fish loot at fishing fail it junk allowed by config option
                    // this is overwrite fishinghole loot for example
                    case LOOT_FISHING_FAIL:
                        loot->FillLoot(0, LootTemplates_Fishing, this, true);
                        break;
                    case LOOT_FISHING:
                        uint32 zone, subzone;
                        go->GetZoneAndAreaId(zone, subzone);
                        // if subzone loot exist use it
                        if (!loot->FillLoot(subzone, LootTemplates_Fishing, this, true, (subzone != zone)) && subzone != zone)
                            // else use zone loot (if zone diff. from subzone, must exist in like case)
                        {
                            loot->FillLoot(zone, LootTemplates_Fishing, this, true);
                        }
                        break;
                    default:
                        if (!lootid)
                        {
                            break;
                        }
                        DEBUG_LOG("       send normal GO loot");

                        loot->FillLoot(lootid, LootTemplates_Gameobject, this, false);
                        loot->generateMoneyLoot(goInfo->MinMoneyLoot, goInfo->MaxMoneyLoot);

                        if (go->GetGoType() == GAMEOBJECT_TYPE_CHEST && goInfo->chest.groupLootRules)
                        {
                            if (Group* group = go->GetGroupLootRecipient())
                            {
                                switch (group->GetLootMethod())
                                {
                                    case GROUP_LOOT:
                                        group->GroupLoot(go, loot);
                                        permission = ALL_PERMISSION;
                                        break;
                                    case NEED_BEFORE_GREED:
                                        group->NeedBeforeGreed(go, loot);
                                        permission = ALL_PERMISSION;
                                        break;
                                    case MASTER_LOOT:
                                        group->MasterLoot(go, loot);
                                        permission = MASTER_PERMISSION;
                                        break;
                                    default:
                                        break;
                                }
                            }
                        }
                        break;
                }
                go->SetLootState(GO_ACTIVATED);
            }

            if (go->getLootState() == GO_ACTIVATED && go->GetGoType() == GAMEOBJECT_TYPE_CHEST && go->GetGOInfo()->chest.groupLootRules)
            {
                if (Group* group = go->GetGroupLootRecipient())
                {
                    if (group == GetGroup())
                    {
                        if (group->GetLootMethod() == MASTER_LOOT)
                        {
                            permission = MASTER_PERMISSION;
                        }
                        else
                        {
                            permission = ALL_PERMISSION;
                        }
                    }
                }
            }

            go->SetGoState(GO_STATE_ACTIVE);
            SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_LOOTING);

            break;
        }
        case HIGHGUID_ITEM:
        {
            Item* item = GetItemByGuid(guid);

            if (!item)
            {
                SendLootRelease(guid);
                return;
            }

            permission = OWNER_PERMISSION;

            loot = &item->loot;

            if (!item->HasGeneratedLoot())
            {
                item->loot.clear();
                ItemPrototype const* itemProto = item->GetProto();

                switch (loot_type)
                {
                    case LOOT_DISENCHANTING:
                        loot->FillLoot(itemProto->DisenchantID, LootTemplates_Disenchant, this, true);
                        item->SetLootState(ITEM_LOOT_TEMPORARY);
                        break;
                    default:
                        loot->FillLoot(item->GetEntry(), LootTemplates_Item, this, true, itemProto->MaxMoneyLoot == 0);
                        loot->generateMoneyLoot(itemProto->MinMoneyLoot, itemProto->MaxMoneyLoot);
                        item->SetLootState(ITEM_LOOT_CHANGED);
                        break;
                }
            }
            break;
        }
        case HIGHGUID_CORPSE:                               // remove insignia
        {
            Corpse* bones = GetMap()->GetCorpse(guid);

            if (!bones || !((loot_type == LOOT_CORPSE) || (loot_type == LOOT_INSIGNIA)) || (bones->GetType() != CORPSE_BONES))
            {
                SendLootRelease(guid);
                return;
            }

            loot = &bones->loot;

            if (!bones->lootForBody)
            {
                bones->lootForBody = true;
                uint32 pLevel = bones->loot.gold;
                bones->loot.clear();
                if (GetBattleGround()->GetTypeID() == BATTLEGROUND_AV)
                {
                    loot->FillLoot(0, LootTemplates_Creature, this, false);
                }
                // It may need a better formula
                // Now it works like this: lvl10: ~6copper, lvl70: ~9silver
                bones->loot.gold = (uint32)(urand(50, 150) * 0.016f * pow(((float)pLevel) / 5.76f, 2.5f) * sWorld.getConfig(CONFIG_FLOAT_RATE_DROP_MONEY));
            }

            if (bones->lootRecipient != this)
            {
                permission = NONE_PERMISSION;
            }
            else
            {
                permission = OWNER_PERMISSION;
            }
            break;
        }
        case HIGHGUID_UNIT:
        {
            Creature* creature = GetMap()->GetCreature(guid);

            // must be in range and creature must be alive for pickpocket and must be dead for another loot
            if (!creature || creature->IsAlive() != (loot_type == LOOT_PICKPOCKETING) || !creature->IsWithinDistInMap(this, INTERACTION_DISTANCE))
            {
                SendLootRelease(guid);
                return;
            }

            if (loot_type == LOOT_PICKPOCKETING && IsFriendlyTo(creature))
            {
                SendLootRelease(guid);
                return;
            }

            loot = &creature->loot;
            CreatureInfo const* creatureInfo = creature->GetCreatureInfo();

            if (loot_type == LOOT_PICKPOCKETING)
            {
                if (!creature->lootForPickPocketed)
                {
                    creature->lootForPickPocketed = true;
                    loot->clear();

                    if (uint32 lootid = creatureInfo->PickpocketLootId)
                    {
                        loot->FillLoot(lootid, LootTemplates_Pickpocketing, this, false);
                    }

                    // Generate extra money for pick pocket loot
                    const uint32 a = urand(0, creature->getLevel() / 2);
                    const uint32 b = urand(0, getLevel() / 2);
                    loot->gold = uint32(10 * (a + b) * sWorld.getConfig(CONFIG_FLOAT_RATE_DROP_MONEY));
                    permission = OWNER_PERMISSION;
                }
            }
            else
            {
                // the player whose group may loot the corpse
                Player* recipient = creature->GetLootRecipient();
                if (!recipient)
                {
                    creature->SetLootRecipient(this);
                    recipient = this;
                }

                if (creature->lootForPickPocketed)
                {
                    creature->lootForPickPocketed = false;
                    loot->clear();
                }

                if (!creature->lootForBody)
                {
                    creature->lootForBody = true;
                    loot->clear();

                    if (uint32 lootid = creatureInfo->LootId)
                    {
                        loot->FillLoot(lootid, LootTemplates_Creature, recipient, false);
                    }

                    loot->generateMoneyLoot(creatureInfo->MinLootGold, creatureInfo->MaxLootGold);

                    if (Group* group = creature->GetGroupLootRecipient())
                    {
                        group->UpdateLooterGuid(creature, true);

                        switch (group->GetLootMethod())
                        {
                            case GROUP_LOOT:
                                // GroupLoot delete items over threshold (threshold even not implemented), and roll them. Items with quality<threshold, round robin
                                group->GroupLoot(creature, loot);
                                break;
                            case NEED_BEFORE_GREED:
                                group->NeedBeforeGreed(creature, loot);
                                break;
                            case MASTER_LOOT:
                                group->MasterLoot(creature, loot);
                                break;
                            default:
                                break;
                        }
                    }
                }

                //if loot for skin is true loot type must be skinning, so loot_type skinning needs to be set in case of reopening the loot window, after bags full or loot not taken
                if (creature->lootForSkin)
                {
                    loot_type = LOOT_SKINNING;
                }

                // possible only if creature->lootForBody && loot->empty() at spell cast check
                if (loot_type == LOOT_SKINNING)
                {
                    if (!creature->lootForSkin)
                    {
                        creature->lootForSkin = true;
                        loot->clear();
                        loot->FillLoot(creatureInfo->SkinningLootId, LootTemplates_Skinning, this, false);

                        // let reopen skinning loot if will closed.
                        if (!loot->empty())
                        {
                            creature->SetUInt32Value(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_LOOTABLE);
                        }

                        permission = OWNER_PERMISSION;

                        // Inform Instance Data, may be scripts related to OnSkinning like The Beast in UBRS
                        if (InstanceData* mapInstance = creature->GetInstanceData())
                        {
                            mapInstance->OnCreatureLooted(creature, LOOT_SKINNING);
                        }
                    }
                }
                // set group rights only for loot_type != LOOT_SKINNING
                else
                {
                    if (Group* group = creature->GetGroupLootRecipient())
                    {
                        if (group == GetGroup())
                        {
                            switch (group->GetLootMethod())
                            {
                                case FREE_FOR_ALL:
                                case ROUND_ROBIN:
                                    permission = ALL_PERMISSION;
                                    break;
                                case MASTER_LOOT:
                                    permission = MASTER_PERMISSION;
                                    break;
                                case GROUP_LOOT:
                                case NEED_BEFORE_GREED:
                                    permission = GROUP_PERMISSION;
                                    break;
                            }
                        }
                        else
                        {
                            permission = NONE_PERMISSION;
                        }
                    }
                    else if (recipient == this)
                    {
                        permission = OWNER_PERMISSION;
                    }
                    else
                    {
                        permission = NONE_PERMISSION;
                    }
                }
            }
            break;
        }
        default:
        {
            sLog.outError("%s is unsupported for looting.", guid.GetString().c_str());
            return;
        }
    }

    SetLootGuid(guid);

    // need know for proper finish item loots (internal pre-switch loot type set in different from 3.x code version)
    // in fact this meaning that it send same loot types for interesting cases like 3.x version code (skip pre-3.x client loot type limitaitons)
    loot->loot_type = loot_type;

    // LOOT_SKINNING, LOOT_PROSPECTING, LOOT_INSIGNIA and LOOT_FISHINGHOLE unsupported by client
    switch (loot_type)
    {
        case LOOT_SKINNING:     loot_type = LOOT_PICKPOCKETING; break;
        case LOOT_INSIGNIA:     loot_type = LOOT_PICKPOCKETING; break;
        case LOOT_FISHING_FAIL: loot_type = LOOT_FISHING;       break;
        case LOOT_FISHINGHOLE:  loot_type = LOOT_FISHING;       break;
        default: break;
    }

    WorldPacket data(SMSG_LOOT_RESPONSE, (9 + 50));         // we guess size
    data << ObjectGuid(guid);
    data << uint8(loot_type);
    data << LootView(*loot, this, permission);
    SendDirectMessage(&data);

    // add 'this' player as one of the players that are looting 'loot'
    if (permission != NONE_PERMISSION)
    {
        loot->AddLooter(GetObjectGuid());
    }

    if (loot_type == LOOT_CORPSE && !guid.IsItem())
    {
        SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_LOOTING);
    }
}

/**
 * @brief Notifies the client that money was removed from the current loot.
 */
void Player::SendNotifyLootMoneyRemoved()
{
    WorldPacket data(SMSG_LOOT_CLEAR_MONEY, 0);
    GetSession()->SendPacket(&data);
}

/**
 * @brief Notifies the client that a loot slot was removed.
 *
 * @param lootSlot The loot slot index that was removed.
 */
void Player::SendNotifyLootItemRemoved(uint8 lootSlot)
{
    WorldPacket data(SMSG_LOOT_REMOVED, 1);
    data << uint8(lootSlot);
    GetSession()->SendPacket(&data);
}
