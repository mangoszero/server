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



#include "GameObject.h"
#include "QuestDef.h"
#include "ObjectMgr.h"
#include "PoolManager.h"
#include "SpellMgr.h"
#include "Spell.h"
#include "UpdateMask.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "World.h"
#include "Database/DatabaseEnv.h"
#include "LootMgr.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "InstanceData.h"
#include "MapManager.h"
#include "MapPersistentStateMgr.h"
#include "BattleGround/BattleGround.h"
#include "BattleGround/BattleGroundAV.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "Util.h"
#include "ScriptMgr.h"
#include "vmap/GameObjectModel.h"
#include "CreatureAISelector.h"
#include "SQLStorages.h"
#include "GameObjectAI.h"
#include "G3D/Quat.h"
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */

/**
 * @brief Handles use interaction for this game object.
 *
 * @param user The unit using the object.
 */
void GameObject::Use(Unit* user)
{
    // user must be provided
    MANGOS_ASSERT(user || PrintEntryError("GameObject::Use (without user)"));

    // by default spell caster is user
    Unit* spellCaster = user;
    uint32 spellId = 0;
    bool triggered = false;

    // test only for exist cooldown data (cooldown timer used for door/buttons reset that not have use cooldown)
    if (uint32 cooldown = GetGOInfo()->GetCooldown())
    {
        if (m_cooldownTime > sWorld.GetGameTime())
        {
            return;
        }

        m_cooldownTime = sWorld.GetGameTime() + cooldown;
    }

    bool scriptReturnValue = user->GetTypeId() == TYPEID_PLAYER && sScriptMgr.OnGameObjectUse((Player*)user, this);

    if (!scriptReturnValue)
    {
        GetMap()->ScriptsStart(DBS_ON_GOT_USE, GetEntry(), spellCaster, this);
    }

    switch (GetGoType())
    {
        case GAMEOBJECT_TYPE_DOOR:                          // 0
        {
            // doors never really despawn, only reset to default state/flags
            UseDoorOrButton();

            // activate script
            if (!scriptReturnValue)
            {
                GetMap()->ScriptsStart(DBS_ON_GO_USE, GetGUIDLow(), spellCaster, this);
            }
            return;
        }
        case GAMEOBJECT_TYPE_BUTTON:                        // 1
        {
            // buttons never really despawn, only reset to default state/flags
            UseDoorOrButton();

            TriggerLinkedGameObject(user);

            // activate script
            if (!scriptReturnValue)
            {
                GetMap()->ScriptsStart(DBS_ON_GO_USE, GetGUIDLow(), spellCaster, this);
            }

            return;
        }
        case GAMEOBJECT_TYPE_QUESTGIVER:                    // 2
        {
            if (user->GetTypeId() != TYPEID_PLAYER)
            {
                return;
            }

            Player* player = (Player*)user;

            if (!sScriptMgr.OnGossipHello(player, this))
            {
                player->PrepareGossipMenu(this, GetGOInfo()->questgiver.gossipID);
                player->SendPreparedGossip(this);
            }

            return;
        }
        case GAMEOBJECT_TYPE_CHEST:                         // 3
        {
            if (user->GetTypeId() != TYPEID_PLAYER)
            {
                return;
            }

            TriggerLinkedGameObject(user);

            // TODO: possible must be moved to loot release (in different from linked triggering)
            if (GetGOInfo()->chest.eventId)
            {
                DEBUG_LOG("Chest ScriptStart id %u for %s (opened by %s)", GetGOInfo()->chest.eventId, GetGuidStr().c_str(), user->GetGuidStr().c_str());
                StartEvents_Event(GetMap(), GetGOInfo()->chest.eventId, user, this);
            }

            return;
        }
        case GAMEOBJECT_TYPE_GENERIC:                       // 5
        {
            if (scriptReturnValue)
            {
                return;
            }

            // No known way to exclude some - only different approach is to select despawnable GOs by Entry
            SetLootState(GO_JUST_DEACTIVATED);
            return;
        }
        case GAMEOBJECT_TYPE_TRAP:                          // 6
        {
            if (scriptReturnValue)
            {
                return;
            }

            Unit* owner = GetOwner();
            Unit* caster = owner ? owner : user;

            GameObjectInfo const* goInfo = GetGOInfo();
            float radius = float(goInfo->trap.radius);
            bool IsBattleGroundTrap = !radius && goInfo->trap.cooldown == 3 && m_respawnTime == 0;

            // FIXME: when GO casting will be implemented trap must cast spell to target
            if (goInfo->trap.spellId)
            {
                caster->CastSpell(user, goInfo->trap.spellId, true, NULL, NULL, GetObjectGuid());
            }
            // use template cooldown if provided
            m_cooldownTime = time(NULL) + (goInfo->trap.cooldown ? goInfo->trap.cooldown : uint32(4));

            // count charges
            if (goInfo->trap.charges > 0)
            {
                AddUse();
            }

            if (IsBattleGroundTrap && user->GetTypeId() == TYPEID_PLAYER)
            {
                // BattleGround gameobjects case
                if (BattleGround* bg = ((Player*)user)->GetBattleGround())
                {
                    bg->HandleTriggerBuff(GetObjectGuid());
                }
            }

            // TODO: all traps can be activated, also those without spell.
            // Some may have have animation and/or are expected to despawn.

            // TODO: Improve this when more information is available, currently these traps are known that must send the anim (Onyxia/ Heigan Fissures/ Trap in DireMaul)
            if (GetDisplayId() == 4392 || GetDisplayId() == 4472 || GetDisplayId() == 4491 || GetDisplayId() == 6785 || GetDisplayId() == 3073)
            {
                SendGameObjectCustomAnim();
            }

            if (!scriptReturnValue && user->GetTypeId() == TYPEID_UNIT)
            {
                sScriptMgr.OnGameObjectUse(user, this);
            }

            // TODO: Despawning of traps? (Also related to code in ::Update)
            return;
        }
        case GAMEOBJECT_TYPE_CHAIR:                         // 7 Sitting: Wooden bench, chairs
        {
            GameObjectInfo const* info = GetGOInfo();
            if (!info)
            {
                return;
            }

            if (user->GetTypeId() != TYPEID_PLAYER)
            {
                return;
            }

            Player* player = (Player*)user;

            // a chair may have n slots. we have to calculate their positions and teleport the player to the nearest one

            // check if the db is sane
            if (info->chair.slots > 0)
            {
                float lowestDist = DEFAULT_VISIBILITY_DISTANCE;

                float x_lowest = GetPositionX();
                float y_lowest = GetPositionY();

                // the object orientation + 1/2 pi
                // every slot will be on that straight line
                float orthogonalOrientation = GetOrientation() + M_PI_F * 0.5f;
                // find nearest slot
                for (uint32 i = 0; i < info->chair.slots; ++i)
                {
                    // the distance between this slot and the center of the go - imagine a 1D space
                    float relativeDistance = (info->size * i) - (info->size * (info->chair.slots - 1) / 2.0f);

                    float x_i = GetPositionX() + relativeDistance * cos(orthogonalOrientation);
                    float y_i = GetPositionY() + relativeDistance * sin(orthogonalOrientation);

                    // calculate the distance between the player and this slot
                    float thisDistance = player->GetDistance2d(x_i, y_i);

                    /* debug code. It will spawn a npc on each slot to visualize them.
                    Creature* helper = player->SummonCreature(14496, x_i, y_i, GetPositionZ(), GetOrientation(), TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, 10000);
                    std::ostringstream output;
                    output << i << ": thisDist: " << thisDistance;
                    helper->MonsterSay(output.str().c_str(), LANG_UNIVERSAL);
                    */

                    if (thisDistance <= lowestDist)
                    {
                        lowestDist = thisDistance;
                        x_lowest = x_i;
                        y_lowest = y_i;
                    }
                }
                player->TeleportTo(GetMapId(), x_lowest, y_lowest, GetPositionZ(), GetOrientation(), TELE_TO_NOT_LEAVE_TRANSPORT | TELE_TO_NOT_LEAVE_COMBAT | TELE_TO_NOT_UNSUMMON_PET);
            }
            else
            {
                // fallback, will always work
                player->TeleportTo(GetMapId(), GetPositionX(), GetPositionY(), GetPositionZ(), GetOrientation(), TELE_TO_NOT_LEAVE_TRANSPORT | TELE_TO_NOT_LEAVE_COMBAT | TELE_TO_NOT_UNSUMMON_PET);
            }
            player->SetStandState(UNIT_STAND_STATE_SIT_LOW_CHAIR + info->chair.height);
            return;
        }
        case GAMEOBJECT_TYPE_SPELL_FOCUS:                   // 8
        {
            TriggerLinkedGameObject(user);

            // some may be activated in addition? Conditions for this? (ex: entry 181616)
            return;
        }
        case GAMEOBJECT_TYPE_GOOBER:                        // 10
        {
            // Handle OutdoorPvP use cases
            // Note: this may be also handled by DB spell scripts in the future, when the world state manager is implemented
            if (user->GetTypeId() == TYPEID_PLAYER)
            {
                Player* player = (Player*)user;
                if (OutdoorPvP* outdoorPvP = sOutdoorPvPMgr.GetScript(player->GetCachedZoneId()))
                {
                    outdoorPvP->HandleGameObjectUse(player, this);
                }
            }

            GameObjectInfo const* info = GetGOInfo();

            TriggerLinkedGameObject(user);

            SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_IN_USE);
            SetLootState(GO_ACTIVATED);

            // this appear to be ok, however others exist in addition to this that should have custom (ex: 190510, 188692, 187389)
            if (info->goober.customAnim)
            {
                SendGameObjectCustomAnim();
            }
            else
            {
                SetGoState(GO_STATE_ACTIVE);
            }

            m_cooldownTime = time(NULL) + info->GetAutoCloseTime();

            if (user->GetTypeId() == TYPEID_PLAYER)
            {
                Player* player = (Player*)user;

                if (info->goober.pageId)                    // show page...
                {
                    WorldPacket data(SMSG_GAMEOBJECT_PAGETEXT, 8);
                    data << ObjectGuid(GetObjectGuid());
                    player->GetSession()->SendPacket(&data);
                }
                else if (info->goober.gossipID)             // ...or gossip, if page does not exist
                {
                    if (!sScriptMgr.OnGossipHello(player, this))
                    {
                        player->PrepareGossipMenu(this, info->goober.gossipID);
                        player->SendPreparedGossip(this);
                    }
                }

                if (info->goober.eventId)
                {
                    DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "Goober ScriptStart id %u for %s (Used by %s).", info->goober.eventId, GetGuidStr().c_str(), player->GetGuidStr().c_str());
                    StartEvents_Event(GetMap(), info->goober.eventId, player, this);
                }

                // possible quest objective for active quests
                if (info->goober.questId && sObjectMgr.GetQuestTemplate(info->goober.questId))
                {
                    // Quest require to be active for GO using
                    if (player->GetQuestStatus(info->goober.questId) != QUEST_STATUS_INCOMPLETE)
                    {
                        break;
                    }
                }

                player->RewardPlayerAndGroupAtCast(this);
            }

            // activate script
            if (!scriptReturnValue)
            {
                GetMap()->ScriptsStart(DBS_ON_GO_USE, GetGUIDLow(), spellCaster, this);
            }
            else
            {
                return;
            }

            // cast this spell later if provided
            spellId = info->goober.spellId;

            break;
        }
        case GAMEOBJECT_TYPE_CAMERA:                        // 13
        {
            GameObjectInfo const* info = GetGOInfo();
            if (!info)
            {
                return;
            }

            if (user->GetTypeId() != TYPEID_PLAYER)
            {
                return;
            }

            Player* player = (Player*)user;

            if (info->camera.cinematicId)
            {
                player->SendCinematicStart(info->camera.cinematicId);
            }

            if (info->camera.eventID)
            {
                StartEvents_Event(GetMap(), info->camera.eventID, player, this);
            }

            return;
        }
        case GAMEOBJECT_TYPE_FISHINGNODE:                   // 17 fishing bobber
        {
            if (user->GetTypeId() != TYPEID_PLAYER)
            {
                return;
            }

            Player* player = (Player*)user;

            if (player->GetObjectGuid() != GetOwnerGuid())
            {
                return;
            }

            switch (getLootState())
            {
                case GO_READY:                              // ready for loot
                {
                    // 1) skill must be >= base_zone_skill
                    // 2) if skill == base_zone_skill => 5% chance
                    // 3) chance is linear dependence from (base_zone_skill-skill)

                    uint32 zone, subzone;
                    GetZoneAndAreaId(zone, subzone);

                    int32 zone_skill = sObjectMgr.GetFishingBaseSkillLevel(subzone);
                    if (!zone_skill)
                    {
                        zone_skill = sObjectMgr.GetFishingBaseSkillLevel(zone);
                    }

                    // provide error, no fishable zone or area should be 0
                    if (!zone_skill)
                    {
                        sLog.outErrorDb("Fishable areaId %u are not properly defined in `skill_fishing_base_level`.", subzone);
                    }

                    int32 skill = player->GetSkillValue(SKILL_FISHING);
                    int32 chance = skill - zone_skill + 5;
                    int32 roll = irand(1, 100);

                    DEBUG_LOG("Fishing check (skill: %i zone min skill: %i chance %i roll: %i", skill, zone_skill, chance, roll);

                    // normal chance
                    bool success = skill >= zone_skill && chance >= roll;
                    GameObject* fishingHole = NULL;

                    // overwrite fail in case fishhole if allowed (after 3.3.0)
                    if (!success)
                    {
                        if (!sWorld.getConfig(CONFIG_BOOL_SKILL_FAIL_POSSIBLE_FISHINGPOOL))
                        {
                            // TODO: find reasonable value for fishing hole search
                            fishingHole = LookupFishingHoleAround(20.0f + CONTACT_DISTANCE);
                            if (fishingHole)
                            {
                                success = true;
                            }
                        }
                    }
                    // just search fishhole for success case
                    else
                        // TODO: find reasonable value for fishing hole search
                    {
                        fishingHole = LookupFishingHoleAround(20.0f + CONTACT_DISTANCE);
                    }

                    if (success || sWorld.getConfig(CONFIG_BOOL_SKILL_FAIL_GAIN_FISHING))
                    {
                        player->UpdateFishingSkill();
                    }

                    // fish catch or fail and junk allowed (after 3.1.0)
                    if (success || sWorld.getConfig(CONFIG_BOOL_SKILL_FAIL_LOOT_FISHING))
                    {
                        // prevent removing GO at spell cancel
                        player->RemoveGameObject(this, false);
                        SetOwnerGuid(player->GetObjectGuid());

                        if (fishingHole)                    // will set at success only
                        {
                            fishingHole->Use(player);
                            SetLootState(GO_JUST_DEACTIVATED);
                        }
                        else
                        {
                            player->SendLoot(GetObjectGuid(), success ? LOOT_FISHING : LOOT_FISHING_FAIL);
                        }
                    }
                    else
                    {
                        // fish escaped, can be deleted now
                        SetLootState(GO_JUST_DEACTIVATED);

                        WorldPacket data(SMSG_FISH_ESCAPED, 0);
                        player->GetSession()->SendPacket(&data);
                    }
                    break;
                }
                case GO_JUST_DEACTIVATED:                   // nothing to do, will be deleted at next update
                    break;
                default:
                {
                    SetLootState(GO_JUST_DEACTIVATED);

                    WorldPacket data(SMSG_FISH_NOT_HOOKED, 0);
                    player->GetSession()->SendPacket(&data);
                    break;
                }
            }

            player->FinishSpell(CURRENT_CHANNELED_SPELL);
            return;
        }
        case GAMEOBJECT_TYPE_SUMMONING_RITUAL:              // 18
        {
            if (user->GetTypeId() != TYPEID_PLAYER)
            {
                return;
            }

            Player* player = (Player*)user;

            Unit* owner = GetOwner();

            GameObjectInfo const* info = GetGOInfo();

            if (owner)
            {
                if (owner->GetTypeId() != TYPEID_PLAYER)
                {
                    return;
                }

                // accept only use by player from same group as owner, excluding owner itself (unique use already added in spell effect)
                if (player == (Player*)owner || (info->summoningRitual.castersGrouped && !player->IsInSameRaidWith(((Player*)owner))))
                {
                    return;
                }

                // expect owner to already be channeling, so if not...
                if (!owner->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
                {
                    return;
                }

                // in case summoning ritual caster is GO creator
                spellCaster = owner;
            }
            else
            {
                if (m_firstUser && player->GetObjectGuid() != m_firstUser && info->summoningRitual.castersGrouped)
                {
                    if (Group* group = player->GetGroup())
                    {
                        if (!group->IsMember(m_firstUser))
                        {
                            return;
                        }
                    }
                    else
                    {
                        return;
                    }
                }

                spellCaster = player;
            }

            AddUniqueUse(player);

            if (info->summoningRitual.animSpell)
            {
                player->CastSpell(player, info->summoningRitual.animSpell, true);

                // for this case, summoningRitual.spellId is always triggered
                triggered = true;
            }

            // full amount unique participants including original summoner, need more
            if (GetUniqueUseCount() < info->summoningRitual.reqParticipants)
            {
                return;
            }

            // owner is first user for non-wild GO objects, if it offline value already set to current user
            if (!GetOwnerGuid())
            {
                if (Player* firstUser = GetMap()->GetPlayer(m_firstUser))
                {
                    spellCaster = firstUser;
                }
            }

            spellId = info->summoningRitual.spellId;

            // spell have reagent and mana cost but it not expected use its
            // it triggered spell in fact casted at currently channeled GO
            triggered = true;

            // finish owners spell
            if (owner)
            {
                owner->FinishSpell(CURRENT_CHANNELED_SPELL);
            }

            // can be deleted now, if
            if (!info->summoningRitual.ritualPersistent)
            {
                SetLootState(GO_JUST_DEACTIVATED);
            }
            // reset ritual for this GO
            else
            {
                ClearAllUsesData();
            }

            // go to end function to spell casting
            break;
        }
        case GAMEOBJECT_TYPE_SPELLCASTER:                   // 22
        {
            SetUInt32Value(GAMEOBJECT_FLAGS, GO_FLAG_LOCKED);

            GameObjectInfo const* info = GetGOInfo();
            if (!info)
            {
                return;
            }

            if (info->spellcaster.partyOnly)
            {
                Unit* caster = GetOwner();
                if (!caster || caster->GetTypeId() != TYPEID_PLAYER)
                {
                    return;
                }

                if (user->GetTypeId() != TYPEID_PLAYER || !((Player*)user)->IsInSameRaidWith((Player*)caster))
                {
                    return;
                }
            }

            spellId = info->spellcaster.spellId;

            AddUse();
            break;
        }
        case GAMEOBJECT_TYPE_FLAGSTAND:                     // 24
        {
            if (user->GetTypeId() != TYPEID_PLAYER)
            {
                return;
            }

            Player* player = (Player*)user;

            if (player->CanUseBattleGroundObject())
            {
                // in battleground check
                BattleGround* bg = player->GetBattleGround();
                if (!bg)
                {
                    return;
                }
                // BG flag click
                // AB:
                // 15001
                // 15002
                // 15003
                // 15004
                // 15005
                bg->EventPlayerClickedOnFlag(player, this);
                return;                                     // we don't need to delete flag ... it is despawned!
            }
            break;
        }
        case GAMEOBJECT_TYPE_FISHINGHOLE:                   // 25
        {
            if (user->GetTypeId() != TYPEID_PLAYER)
            {
                return;
            }

            Player* player = (Player*)user;

            player->SendLoot(GetObjectGuid(), LOOT_FISHINGHOLE);
            return;
        }
        case GAMEOBJECT_TYPE_FLAGDROP:                      // 26
        {
            if (user->GetTypeId() != TYPEID_PLAYER)
            {
                return;
            }

            Player* player = (Player*)user;

            if (player->CanUseBattleGroundObject())
            {
                // in battleground check
                BattleGround* bg = player->GetBattleGround();
                if (!bg)
                {
                    return;
                }
                // BG flag dropped
                // WS:
                // 179785 - Silverwing Flag
                // 179786 - Warsong Flag
                GameObjectInfo const* info = GetGOInfo();
                if (info)
                {
                    switch (info->id)
                    {
                        case 179785:                        // Silverwing Flag
                        case 179786:                        // Warsong Flag
                            // check if it's correct bg
                            if (bg->GetTypeID() == BATTLEGROUND_WS)
                            {
                                bg->EventPlayerClickedOnFlag(player, this);
                            }
                            break;
                    }
                }
                // this cause to call return, all flags must be deleted here!!
                spellId = 0;
                Delete();
            }
            break;
        }
        default:
            sLog.outError("GameObject::Use unhandled GameObject type %u (entry %u).", GetGoType(), GetEntry());
            return;
    }

    if (!spellId)
    {
        return;
    }

    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellId);
    if (!spellInfo)
    {
        sLog.outError("WORLD: unknown spell id %u at use action for gameobject (Entry: %u GoType: %u )", spellId, GetEntry(), GetGoType());
        return;
    }

    Spell* spell = new Spell(spellCaster, spellInfo, triggered, GetObjectGuid());

    // spell target is user of GO
    SpellCastTargets targets;
    targets.setUnitTarget(user);

    spell->prepare(&targets);
}
