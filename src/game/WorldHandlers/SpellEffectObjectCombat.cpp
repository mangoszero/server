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



#include "Common.h"
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "Log.h"
#include "UpdateMask.h"
#include "World.h"
#include "ObjectMgr.h"
#include "SpellMgr.h"
#include "Player.h"
#include "Unit.h"
#include "Spell.h"
#include "DynamicObject.h"
#include "SpellAuras.h"
#include "Group.h"
#include "UpdateData.h"
#include "MapManager.h"
#include "ObjectAccessor.h"
#include "SharedDefines.h"
#include "Pet.h"
#include "GameObject.h"
#include "GossipDef.h"
#include "Creature.h"
#include "Totem.h"
#include "CreatureAI.h"
#include "BattleGround/BattleGroundMgr.h"
#include "BattleGround/BattleGround.h"
#include "BattleGround/BattleGroundWS.h"
#include "Language.h"
#include "SocialMgr.h"
#include "VMapFactory.h"
#include "Util.h"
#include "TemporarySummon.h"
#include "ScriptMgr.h"
#include "Formulas.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "G3D/Vector3.h"
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */

/**
 * @brief Adds flat threat from the caster to the unit target.
 *
 * @param eff_idx Unused effect index.
 */
void Spell::EffectThreat(SpellEffectIndex /*eff_idx*/)
{
    if (!unitTarget || !unitTarget->IsAlive() || !m_caster->IsAlive())
    {
        return;
    }

    if (!unitTarget->CanHaveThreatList())
    {
        return;
    }

    unitTarget->AddThreat(m_caster, float(damage), false, GetSpellSchoolMask(m_spellInfo), m_spellInfo);
}

/**
 * @brief Heals the target for an amount equal to the caster's maximum health.
 *
 * @param eff_idx Unused effect index.
 */
void Spell::EffectHealMaxHealth(SpellEffectIndex /*eff_idx*/)
{
    if (!unitTarget)
    {
        return;
    }
    if (!unitTarget->IsAlive())
    {
        return;
    }

    uint32 heal = m_caster->GetMaxHealth();

    m_healing += heal;
}

/**
 * @brief Interrupts interruptible non-melee spells on the unit target.
 *
 * @param eff_idx Unused effect index.
 */
void Spell::EffectInterruptCast(SpellEffectIndex /*eff_idx*/)
{
    if (!unitTarget)
    {
        return;
    }
    if (!unitTarget->IsAlive())
    {
        return;
    }

    // TODO: not all spells that used this effect apply cooldown at school spells
    // also exist case: apply cooldown to interrupted cast only and to all spells
    for (uint32 i = CURRENT_FIRST_NON_MELEE_SPELL; i < CURRENT_MAX_SPELL; ++i)
    {
        if (Spell* spell = unitTarget->GetCurrentSpell(CurrentSpellTypes(i)))
        {
            SpellEntry const* curSpellInfo = spell->m_spellInfo;
            // check if we can interrupt spell
            if ((curSpellInfo->InterruptFlags & SPELL_INTERRUPT_FLAG_INTERRUPT) && curSpellInfo->PreventionType == SPELL_PREVENTION_TYPE_SILENCE)
            {
                unitTarget->ProhibitSpellSchool(GetSpellSchoolMask(curSpellInfo), GetSpellDuration(m_spellInfo));
                unitTarget->InterruptSpell(CurrentSpellTypes(i), false);
            }
        }
    }
}

/**
 * @brief Summons a wild game object at the destination or near the caster.
 *
 * @param eff_idx The summon object effect index.
 */
void Spell::EffectSummonObjectWild(SpellEffectIndex eff_idx)
{
    uint32 gameobject_id = m_spellInfo->EffectMiscValue[eff_idx];

    GameObject* pGameObj = new GameObject;

    WorldObject* target = focusObject;
    if (!target)
    {
        target = m_caster;
    }

    float x, y, z;
    if (m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION)
    {
        m_targets.getDestination(x, y, z);
    }
    else
    {
        m_caster->GetClosePoint(x, y, z, DEFAULT_WORLD_OBJECT_SIZE);
    }

    Map* map = target->GetMap();

    if (!pGameObj->Create(map->GenerateLocalLowGuid(HIGHGUID_GAMEOBJECT), gameobject_id, map,
        x, y, z, target->GetOrientation()))
    {
        delete pGameObj;
        return;
    }

    int32 duration = GetSpellDuration(m_spellInfo);

    pGameObj->SetRespawnTime(duration > 0 ? duration / IN_MILLISECONDS : 0);
    pGameObj->SetSpellId(m_spellInfo->ID);

    // Wild object not have owner and check clickable by players
    map->Add(pGameObj);
    pGameObj->AIM_Initialize();

    if (pGameObj->GetGoType() == GAMEOBJECT_TYPE_FLAGDROP && m_caster->GetTypeId() == TYPEID_PLAYER)
    {
        Player* pl = (Player*)m_caster;
        BattleGround* bg = ((Player*)m_caster)->GetBattleGround();

        switch (pGameObj->GetMapId())
        {
            case 489:                                       // WS
            {
                if (bg && bg->GetTypeID() == BATTLEGROUND_WS && bg->GetStatus() == STATUS_IN_PROGRESS)
                {
                    Team team = pl->GetTeam() == ALLIANCE ? HORDE : ALLIANCE;

                    ((BattleGroundWS*)bg)->SetDroppedFlagGuid(pGameObj->GetObjectGuid(), team);
                }
                break;
            }
        }
    }

    pGameObj->SummonLinkedTrapIfAny();

    if (m_caster->GetTypeId() == TYPEID_UNIT && ((Creature*)m_caster)->AI())
    {
        ((Creature*)m_caster)->AI()->JustSummoned(pGameObj);
    }
    if (m_originalCaster && m_originalCaster != m_caster && m_originalCaster->GetTypeId() == TYPEID_UNIT && ((Creature*)m_originalCaster)->AI())
    {
        ((Creature*)m_originalCaster)->AI()->JustSummoned(pGameObj);
    }
}

/**
 * @brief Clears combat and threat state for the target.
 *
 * @param eff_idx Unused effect index.
 */
void Spell::EffectSanctuary(SpellEffectIndex /*eff_idx*/)
{
    if (!unitTarget)
    {
        return;
    }
    // unitTarget->CombatStop();

    unitTarget->CombatStop();
    unitTarget->GetHostileRefManager().deleteReferences();  // stop all fighting

    // Vanish allows to remove all threat and cast regular stealth so other spells can be used
    if (m_spellInfo->IsFitToFamily(SPELLFAMILY_ROGUE, UI64LIT(0x0000000000000800)))
    {
        ((Player*)m_caster)->RemoveSpellsCausingAura(SPELL_AURA_MOD_ROOT);
    }

    // Improved Sap: a hacky way
    if (m_triggeredByAuraSpell && m_spellInfo->ID == 14093 && unitTarget->GetTypeId() == TYPEID_PLAYER)
    {
        // find highest rank Stealth spell cooldown
        uint32 stealth_id = 0;
        SpellCooldowns const scm = ((Player*)unitTarget)->GetSpellCooldownMap();
        for (SpellCooldowns::const_reverse_iterator it = scm.rbegin(); it != scm.rend(); ++it)
        {
            if (it->first >= 1784 && it->first <= 1787)
            {
                stealth_id = it->first;
                break;
            }
        }
        if (!stealth_id)
        {
            return;
        }
        // drop cooldown and prepare to recast the aura
        ((Player*)unitTarget)->RemoveSpellCooldown(stealth_id);
        unitTarget->CastSpell(unitTarget, stealth_id, true);
    }
}

/**
 * @brief Adds combo points to the caster for the unit target.
 *
 * @param eff_idx Unused effect index.
 */
void Spell::EffectAddComboPoints(SpellEffectIndex /*eff_idx*/)
{
    if (!unitTarget)
    {
        return;
    }

    if (m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }

    if (damage <= 0)
    {
        return;
    }

    ((Player*)m_caster)->AddComboPoints(unitTarget, damage);
}

/**
 * @brief Creates a duel flag object and starts a duel request between two players.
 *
 * @param eff_idx The effect index containing the duel flag game object id.
 */
void Spell::EffectDuel(SpellEffectIndex eff_idx)
{
    if (!m_caster || !unitTarget || m_caster->GetTypeId() != TYPEID_PLAYER || unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }

    Player* caster = (Player*)m_caster;
    Player* target = (Player*)unitTarget;

    // caster or target already have requested duel
    if (caster->duel || target->duel || !target->GetSocial() || target->GetSocial()->HasIgnore(caster->GetObjectGuid()))
    {
        return;
    }

    // Players can only fight a duel with each other outside (=not inside dungeons and not in capital cities)
    AreaTableEntry const* casterAreaEntry = GetAreaEntryByAreaID(caster->GetAreaId());
    if (casterAreaEntry && !(casterAreaEntry->Flags & AREA_FLAG_DUEL))
    {
        SendCastResult(SPELL_FAILED_NO_DUELING);            // Dueling isn't allowed here
        return;
    }

    AreaTableEntry const* targetAreaEntry = GetAreaEntryByAreaID(target->GetAreaId());
    if (targetAreaEntry && !(targetAreaEntry->Flags & AREA_FLAG_DUEL))
    {
        SendCastResult(SPELL_FAILED_NO_DUELING);            // Dueling isn't allowed here
        return;
    }

    // CREATE DUEL FLAG OBJECT
    GameObject* pGameObj = new GameObject;

    uint32 gameobject_id = m_spellInfo->EffectMiscValue[eff_idx];

    Map* map = m_caster->GetMap();
    float x = (m_caster->GetPositionX() + unitTarget->GetPositionX()) * 0.5f;
    float y = (m_caster->GetPositionY() + unitTarget->GetPositionY()) * 0.5f;
    float z = m_caster->GetPositionZ();
    m_caster->UpdateAllowedPositionZ(x, y, z);
    if (!pGameObj->Create(map->GenerateLocalLowGuid(HIGHGUID_GAMEOBJECT), gameobject_id, map, x, y, z, m_caster->GetOrientation()))
    {
        delete pGameObj;
        return;
    }

    pGameObj->SetUInt32Value(GAMEOBJECT_FACTION, m_caster->getFaction());
    pGameObj->SetUInt32Value(GAMEOBJECT_LEVEL, m_caster->getLevel() + 1);
    int32 duration = GetSpellDuration(m_spellInfo);
    pGameObj->SetRespawnTime(duration > 0 ? duration / IN_MILLISECONDS : 0);
    pGameObj->SetSpellId(m_spellInfo->ID);

    m_caster->AddGameObject(pGameObj);
    map->Add(pGameObj);
    pGameObj->AIM_Initialize();
    // END

    // Send request
    WorldPacket data(SMSG_DUEL_REQUESTED, 8 + 8);
    data << pGameObj->GetObjectGuid();
    data << caster->GetObjectGuid();
    caster->GetSession()->SendPacket(&data);
    target->GetSession()->SendPacket(&data);

    // create duel-info
    DuelInfo* duel   = new DuelInfo;
    duel->initiator  = caster;
    duel->opponent   = target;
    duel->startTime  = 0;
    duel->startTimer = 0;
    caster->duel     = duel;

    DuelInfo* duel2   = new DuelInfo;
    duel2->initiator  = caster;
    duel2->opponent   = caster;
    duel2->startTime  = 0;
    duel2->startTimer = 0;
    target->duel      = duel2;

    caster->SetGuidValue(PLAYER_DUEL_ARBITER, pGameObj->GetObjectGuid());
    target->SetGuidValue(PLAYER_DUEL_ARBITER, pGameObj->GetObjectGuid());

    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (Eluna* e = caster->GetEluna())
    {
        e->OnDuelRequest(target, caster);
    }
#endif /* ENABLE_ELUNA */
}

/**
 * @brief Teleports a player target to its homebind as an unstuck action.
 *
 * @param eff_idx Unused effect index.
 */
void Spell::EffectStuck(SpellEffectIndex /*eff_idx*/)
{
    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }

    if (!sWorld.getConfig(CONFIG_BOOL_CAST_UNSTUCK))
    {
        return;
    }

    Player* pTarget = (Player*)unitTarget;

    DEBUG_LOG("Spell Effect: Stuck");
    DETAIL_LOG("Player %s (guid %u) used auto-unstuck future at map %u (%f, %f, %f)", pTarget->GetName(), pTarget->GetGUIDLow(), m_caster->GetMapId(), m_caster->GetPositionX(), pTarget->GetPositionY(), pTarget->GetPositionZ());

    if (pTarget->IsTaxiFlying())
    {
        return;
    }

    // homebind location is loaded always
    pTarget->TeleportToHomebind(unitTarget == m_caster ? TELE_TO_SPELL : 0);

    // Stuck spell trigger Hearthstone cooldown
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(8690);
    if (!spellInfo)
    {
        return;
    }
    Spell spell(pTarget, spellInfo, true);
    spell.SendSpellCooldown();
}

/**
 * @brief Sends a summon request to a player target.
 *
 * @param eff_idx Unused effect index.
 */
void Spell::EffectSummonPlayer(SpellEffectIndex /*eff_idx*/)
{
    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }

    // Evil Twin (ignore player summon, but hide this for summoner)
    if (unitTarget->GetDummyAura(23445))
    {
        return;
    }

    float x, y, z;
    m_caster->GetClosePoint(x, y, z, unitTarget->GetObjectBoundingRadius());

    ((Player*)unitTarget)->SetSummonPoint(m_caster->GetMapId(), x, y, z);

    WorldPacket data(SMSG_SUMMON_REQUEST, 8 + 4 + 4);
    data << m_caster->GetObjectGuid();                      // summoner guid
    data << uint32(m_caster->GetZoneId());                  // summoner zone
    data << uint32(MAX_PLAYER_SUMMON_DELAY * IN_MILLISECONDS); // auto decline after msecs
    ((Player*)unitTarget)->GetSession()->SendPacket(&data);
}

/**
 * @brief Builds the default script command used to activate a game object.
 *
 * @return ScriptInfo Preconfigured activation command data.
 */
static ScriptInfo generateActivateCommand()
{
    ScriptInfo si;
    si.command = SCRIPT_COMMAND_ACTIVATE_OBJECT;
    si.id = 0;
    si.buddyEntry = 0;
    si.searchRadiusOrGuid = 0;
    si.data_flags = 0x00;
    return si;
}

/**
 * @brief Activates or manipulates the targeted game object based on the effect misc value.
 *
 * @param eff_idx The activation effect index.
 */
void Spell::EffectActivateObject(SpellEffectIndex eff_idx)
{
    if (!gameObjTarget)
    {
        return;
    }

    uint32 misc_value = m_spellInfo->EffectMiscValue[eff_idx];

    switch (misc_value)
    {
        case 1:                     // GO simple use
        case 2:                     // unk - 2 spells
        case 4:                     // unk - 1 spell
        case 5:                     // GO trap usage
        case 7:                     // unk - 2 spells
        case 8:                     // GO usage with TargetB = none or random
        case 10:                    // unk - 2 spells
        {
            static ScriptInfo activateCommand = generateActivateCommand();

            int32 delay_secs = m_spellInfo->CalculateSimpleValue(eff_idx);

            gameObjTarget->GetMap()->ScriptCommandStart(activateCommand, delay_secs, m_caster, gameObjTarget);
            break;
        }
        case 3:                     // GO custom anim - found mostly in Lunar Fireworks spells
            gameObjTarget->SendGameObjectCustomAnim();
            break;
        case 12:                    // GO state active alternative - found mostly in Simon Game spells
            gameObjTarget->UseDoorOrButton(0, true);
            break;
        case 15:                    // GO destroy
            gameObjTarget->SetLootState(GO_JUST_DEACTIVATED);
            break;
        case 16:                    // GO custom use - found mostly in Wind Stones spells, Simon Game spells and other GO target summoning spells
        {
            switch (m_spellInfo->ID)
            {
                case 24734:         // Summon Templar Random
                case 24744:         // Summon Templar (fire)
                case 24756:         // Summon Templar (air)
                case 24758:         // Summon Templar (earth)
                case 24760:         // Summon Templar (water)
                case 24763:         // Summon Duke Random
                case 24765:         // Summon Duke (fire)
                case 24768:         // Summon Duke (air)
                case 24770:         // Summon Duke (earth)
                case 24772:         // Summon Duke (water)
                case 24784:         // Summon Royal Random
                case 24786:         // Summon Royal (fire)
                case 24788:         // Summon Royal (air)
                case 24789:         // Summon Royal (earth)
                case 24790:         // Summon Royal (water)
                {
                    uint32 npcEntry = 0;
                    uint32 templars[] = {15209, 15211, 15212, 15307};
                    uint32 dukes[] = {15206, 15207, 15208, 15220};
                    uint32 royals[] = {15203, 15204, 15205, 15305};

                    switch (m_spellInfo->ID)
                    {
                        case 24734: npcEntry = templars[urand(0, 3)]; break;
                        case 24763: npcEntry = dukes[urand(0, 3)];    break;
                        case 24784: npcEntry = royals[urand(0, 3)];   break;
                        case 24744: npcEntry = 15209;                 break;
                        case 24756: npcEntry = 15212;                 break;
                        case 24758: npcEntry = 15307;                 break;
                        case 24760: npcEntry = 15211;                 break;
                        case 24765: npcEntry = 15206;                 break;
                        case 24768: npcEntry = 15220;                 break;
                        case 24770: npcEntry = 15208;                 break;
                        case 24772: npcEntry = 15207;                 break;
                        case 24786: npcEntry = 15203;                 break;
                        case 24788: npcEntry = 15204;                 break;
                        case 24789: npcEntry = 15205;                 break;
                        case 24790: npcEntry = 15305;                 break;
                    }

                    gameObjTarget->SummonCreature(npcEntry, gameObjTarget->GetPositionX(), gameObjTarget->GetPositionY(), gameObjTarget->GetPositionZ(), gameObjTarget->GetAngle(m_caster), TEMPSPAWN_TIMED_OOC_OR_DEAD_DESPAWN, MINUTE * IN_MILLISECONDS);
                    gameObjTarget->SetLootState(GO_JUST_DEACTIVATED);
                    break;
                }
            }
            break;
        }
        default:
            sLog.outError("Spell::EffectActivateObject called with unknown misc value. Spell Id %u", m_spellInfo->ID);
            break;
    }
}

/**
 * @brief Applies a temporary enchantment to the main-hand item of the player target.
 *
 * @param eff_idx The enchant effect index.
 */
void Spell::EffectEnchantHeldItem(SpellEffectIndex eff_idx)
{
    // this is only item spell effect applied to main-hand weapon of target player (players in area)
    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }

    Player* item_owner = (Player*)unitTarget;
    Item* item = item_owner->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);

    if (!item)
    {
        return;
    }

    // must be equipped
    if (!item ->IsEquipped())
    {
        return;
    }

    if (m_spellInfo->EffectMiscValue[eff_idx])
    {
        uint32 enchant_id = m_spellInfo->EffectMiscValue[eff_idx];
        int32 duration = GetSpellDuration(m_spellInfo);     // Try duration index first...
        if (!duration)
        {
            duration = m_currentBasePoints[eff_idx];         // Base points after...
        }
        if (!duration)
        {
            duration = 10 * IN_MILLISECONDS;                 // 10 seconds for enchants which don't have listed duration
        }

        SpellItemEnchantmentEntry const* pEnchant = sSpellItemEnchantmentStore.LookupEntry(enchant_id);
        if (!pEnchant)
        {
            return;
        }

        // Always go to temp enchantment slot
        EnchantmentSlot slot = TEMP_ENCHANTMENT_SLOT;

        // Enchantment will not be applied if a different one already exists
        if (item->GetEnchantmentId(slot) && item->GetEnchantmentId(slot) != enchant_id)
        {
            return;
        }

        // Apply the temporary enchantment
        item->SetEnchantment(slot, enchant_id, duration, 0);
        item_owner->ApplyEnchantment(item, slot, true);
    }
}

/**
 * @brief Starts disenchanting loot generation for the target item.
 *
 * @param eff_idx Unused effect index.
 */
void Spell::EffectDisEnchant(SpellEffectIndex /*eff_idx*/)
{
    if (m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }

    Player* p_caster = (Player*)m_caster;
    if (!itemTarget || !itemTarget->GetProto()->DisenchantID)
    {
        return;
    }

    p_caster->UpdateCraftSkill(m_spellInfo->ID);

    ((Player*)m_caster)->SendLoot(itemTarget->GetObjectGuid(), LOOT_DISENCHANTING);

    // item will be removed at disenchanting end
}

/**
 * @brief Increases the drunk state of a player target.
 *
 * @param eff_idx Unused effect index.
 */
void Spell::EffectInebriate(SpellEffectIndex /*eff_idx*/)
{
    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }

    Player* player = (Player*)unitTarget;
    uint16 currentDrunk = player->GetDrunkValue();
    uint16 drunkMod = damage * 256;
    if (currentDrunk + drunkMod > 0xFFFF)
    {
        currentDrunk = 0xFFFF;
    }
    else
    {
        currentDrunk += drunkMod;
    }
    player->SetDrunkValue(currentDrunk, m_CastItem ? m_CastItem->GetEntry() : 0);
}

/**
 * @brief Feeds the caster's pet and triggers the associated benefit spell.
 *
 * @param eff_idx The effect index containing the triggered spell id.
 */
void Spell::EffectFeedPet(SpellEffectIndex eff_idx)
{
    if (m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }

    Player* _player = (Player*)m_caster;

    Item* foodItem = itemTarget;
    if (!foodItem)
    {
        return;
    }

    Pet* pet = _player->GetPet();
    if (!pet)
    {
        return;
    }

    if (!pet->IsAlive())
    {
        return;
    }

    int32 benefit = pet->GetCurrentFoodBenefitLevel(foodItem->GetProto()->ItemLevel);
    if (benefit <= 0)
    {
        return;
    }

    uint32 count = 1;
    _player->DestroyItemCount(foodItem, count, true);
    // TODO: fix crash when a spell has two effects, both pointed at the same item target

    m_caster->CastCustomSpell(m_caster, m_spellInfo->EffectTriggerSpell[eff_idx], &benefit, NULL, NULL, true);
}

/**
 * @brief Dismisses the caster's living pet.
 *
 * @param eff_idx Unused effect index.
 */
void Spell::EffectDismissPet(SpellEffectIndex /*eff_idx*/)
{
    if (m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }

    Pet* pet = m_caster->GetPet();

    // not let dismiss dead pet
    if (!pet || !pet->IsAlive())
    {
        return;
    }

    pet->Unsummon(PET_SAVE_NOT_IN_SLOT, m_caster);
}

/**
 * @brief Summons a persistent object into one of the caster's object slots.
 *
 * @param eff_idx The summon object effect index.
 */
void Spell::EffectSummonObject(SpellEffectIndex eff_idx)
{
    uint32 go_id = m_spellInfo->EffectMiscValue[eff_idx];

    uint8 slot = 0;
    switch (m_spellInfo->Effect[eff_idx])
    {
        case SPELL_EFFECT_SUMMON_OBJECT_SLOT1: slot = 0; break;
        case SPELL_EFFECT_SUMMON_OBJECT_SLOT2: slot = 1; break;
        case SPELL_EFFECT_SUMMON_OBJECT_SLOT3: slot = 2; break;
        case SPELL_EFFECT_SUMMON_OBJECT_SLOT4: slot = 3; break;
        default: return;
    }

    if (ObjectGuid guid = m_caster->m_ObjectSlotGuid[slot])
    {
        if (GameObject* obj = m_caster->GetMap()->GetGameObject(guid))
        {
            obj->SetLootState(GO_JUST_DEACTIVATED);
        }
        m_caster->m_ObjectSlotGuid[slot].Clear();
    }

    GameObject* pGameObj = new GameObject;

    float x, y, z;
    // If dest location if present
    if (m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION)
    {
        m_targets.getDestination(x, y, z);
    }
    // Summon in random point all other units if location present
    else
    {
        m_caster->GetClosePoint(x, y, z, DEFAULT_WORLD_OBJECT_SIZE);
    }

    Map* map = m_caster->GetMap();
    if (!pGameObj->Create(map->GenerateLocalLowGuid(HIGHGUID_GAMEOBJECT), go_id, map,
        x, y, z, m_caster->GetOrientation(), 0.0f, 0.0f, 0.0f, 0.0f, GO_ANIMPROGRESS_DEFAULT, GO_STATE_READY))
    {
        delete pGameObj;
        return;
    }

    pGameObj->SetUInt32Value(GAMEOBJECT_LEVEL, m_caster->getLevel());
    int32 duration = GetSpellDuration(m_spellInfo);
    pGameObj->SetRespawnTime(duration > 0 ? duration / IN_MILLISECONDS : 0);
    pGameObj->SetSpellId(m_spellInfo->ID);
    m_caster->AddGameObject(pGameObj);

    map->Add(pGameObj);
    pGameObj->AIM_Initialize();
    WorldPacket data(SMSG_GAMEOBJECT_SPAWN_ANIM_OBSOLETE, 8);
    data << ObjectGuid(pGameObj->GetObjectGuid());
    m_caster->SendMessageToSet(&data, true);

    m_caster->m_ObjectSlotGuid[slot] = pGameObj->GetObjectGuid();

    pGameObj->SummonLinkedTrapIfAny();

    if (m_caster->GetTypeId() == TYPEID_UNIT && ((Creature*)m_caster)->AI())
    {
        ((Creature*)m_caster)->AI()->JustSummoned(pGameObj);
    }
    if (m_originalCaster && m_originalCaster != m_caster && m_originalCaster->GetTypeId() == TYPEID_UNIT && ((Creature*)m_originalCaster)->AI())
    {
        ((Creature*)m_originalCaster)->AI()->JustSummoned(pGameObj);
    }
}

/**
 * @brief Sends a resurrection request with percentage-based health and mana restoration.
 *
 * @param eff_idx Unused effect index.
 */
void Spell::EffectResurrect(SpellEffectIndex /*eff_idx*/)
{
    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }

    if (unitTarget->IsAlive() || !unitTarget->IsInWorld())
    {
        return;
    }

    switch (m_spellInfo->ID)
    {
        case 8342:                                          // Defibrillate (Goblin Jumper Cables) has 33% chance on success
        case 22999:                                         // Defibrillate (Goblin Jumper Cables XL) has 50% chance on success
        {
            uint32 failChance = 0;
            uint32 failSpellId = 0;
            switch (m_spellInfo->ID)
            {
                case 8342:  failChance = 67; failSpellId = 8338;  break;
                case 22999: failChance = 50; failSpellId = 23055; break;
            }

            if (roll_chance_i(failChance))
            {
                if (failSpellId)
                {
                    m_caster->CastSpell(m_caster, failSpellId, true, m_CastItem);
                }
                return;
            }
            break;
        }
        default:
            break;
    }

    Player* pTarget = ((Player*)unitTarget);

    if (pTarget->isRessurectRequested())      // already have one active request
    {
        return;
    }

    uint32 health = pTarget->GetMaxHealth() * damage / 100;
    uint32 mana   = pTarget->GetMaxPower(POWER_MANA) * damage / 100;

    pTarget->setResurrectRequestData(m_caster->GetObjectGuid(), m_caster->GetMapId(), m_caster->GetPositionX(), m_caster->GetPositionY(), m_caster->GetPositionZ(), health, mana);
    SendResurrectRequest(pTarget);
}

/**
 * @brief Adds queued extra attacks to the target unit.
 *
 * @param eff_idx Unused effect index.
 */
void Spell::EffectAddExtraAttacks(SpellEffectIndex /*eff_idx*/)
{
    if (!unitTarget || !unitTarget->IsAlive())
    {
        return;
    }

    if (unitTarget->m_extraAttacks)
    {
        if (m_spellInfo->ID == 20178 && unitTarget->m_extraAttacks < 4)
        {
            ++unitTarget->m_extraAttacks;   // += damage would be more logical
        }
    }
    else
    {
        unitTarget->m_extraAttacks = damage;
    }
}

/**
 * @brief Grants the ability to parry to a player target.
 *
 * @param eff_idx Unused effect index.
 */
void Spell::EffectParry(SpellEffectIndex /*eff_idx*/)
{
    if (unitTarget && unitTarget->GetTypeId() == TYPEID_PLAYER)
    {
        ((Player*)unitTarget)->SetCanParry(true);
    }
}

/**
 * @brief Grants the ability to block to a player target.
 *
 * @param eff_idx Unused effect index.
 */
void Spell::EffectBlock(SpellEffectIndex /*eff_idx*/)
{
    if (unitTarget && unitTarget->GetTypeId() == TYPEID_PLAYER)
    {
        ((Player*)unitTarget)->SetCanBlock(true);
    }
}

/**
 * @brief Teleports the target forward while avoiding steep terrain, water edges, and obstacles.
 *
 * @param eff_idx The effect index providing the leap distance.
 */
void Spell::EffectLeapForward(SpellEffectIndex eff_idx)
{
    float dist = GetSpellRadius(sSpellRadiusStore.LookupEntry(m_spellInfo->EffectRadiusIndex[eff_idx]));
    const float IN_OR_UNDER_LIQUID_RANGE = 0.8f;                // range to make player under liquid or on liquid surface from liquid level

    G3D::Vector3 prevPos, nextPos;
    float orientation = unitTarget->GetOrientation();

    prevPos.x = unitTarget->GetPositionX();
    prevPos.y = unitTarget->GetPositionY();
    prevPos.z = unitTarget->GetPositionZ();

    float groundZ = prevPos.z;
    bool isPrevInLiquid = false;

    // falling case
    if (!unitTarget->GetMap()->GetHeightInRange(prevPos.x, prevPos.y, groundZ, 3.0f) && unitTarget->m_movementInfo.HasMovementFlag(MOVEFLAG_FALLING))
    {
        nextPos.x = prevPos.x + dist * cos(orientation);
        nextPos.y = prevPos.y + dist * sin(orientation);
        nextPos.z = prevPos.z - 2.0f; // little hack to avoid the impression to go up when teleporting instead of continue to fall. This value may need some tweak

        //
        GridMapLiquidData liquidData;
        if (unitTarget->GetMap()->GetTerrain()->IsInWater(nextPos.x, nextPos.y, nextPos.z, &liquidData))
        {
            if (fabs(nextPos.z - liquidData.level) < 10.0f)
            {
                nextPos.z = liquidData.level - IN_OR_UNDER_LIQUID_RANGE;
            }
        }
        else
        {
            // fix z to ground if near of it
            unitTarget->GetMap()->GetHeightInRange(nextPos.x, nextPos.y, nextPos.z, 10.0f);
        }

        // check any obstacle and fix coords
        unitTarget->GetMap()->GetHitPosition(prevPos.x, prevPos.y, prevPos.z + 0.5f, nextPos.x, nextPos.y, nextPos.z, -0.5f);

        // teleport
        unitTarget->NearTeleportTo(nextPos.x, nextPos.y, nextPos.z, orientation, unitTarget == m_caster);

        //sLog.outString("Falling BLINK!");
        return;
    }

    // fix origin position if player was jumping and near of the ground but not in ground
    if (fabs(prevPos.z - groundZ) > 0.5f)
    {
        prevPos.z = groundZ;
    }

    //check if in liquid
    isPrevInLiquid = unitTarget->GetMap()->GetTerrain()->IsInWater(prevPos.x, prevPos.y, prevPos.z);

    const float step = 2.0f;                                    // step length before next check slope/edge/water
    const float maxSlope = 50.0f;                               // 50(degree) max seem best value for walkable slope
    const float MAX_SLOPE_IN_RADIAN = maxSlope / 180.0f * M_PI_F;
    float nextZPointEstimation = 1.0f;
    float destx = prevPos.x + dist * cos(orientation);
    float desty = prevPos.y + dist * sin(orientation);
    const uint32 numChecks = ceil(fabs(dist / step));
    const float DELTA_X = (destx - prevPos.x) / numChecks;
    const float DELTA_Y = (desty - prevPos.y) / numChecks;

    for (uint32 i = 1; i < numChecks + 1; ++i)
    {
        // compute next point average position
        nextPos.x = prevPos.x + DELTA_X;
        nextPos.y = prevPos.y + DELTA_Y;
        nextPos.z = prevPos.z + nextZPointEstimation;

        bool isInLiquid = false;
        bool isInLiquidTested = false;
        bool isOnGround = false;
        GridMapLiquidData liquidData;

        // try fix height for next position
        if (!unitTarget->GetMap()->GetHeightInRange(nextPos.x, nextPos.y, nextPos.z))
        {
            // we cant so test if we are on water
            if (!unitTarget->GetMap()->GetTerrain()->IsInWater(nextPos.x, nextPos.y, nextPos.z, &liquidData))
            {
                // not in water and cannot get correct height, maybe flying?
                //sLog.outString("Can't get height of point %u, point value %s", i, nextPos.toString().c_str());
                nextPos = prevPos;
                break;
            }
            else
            {
                isInLiquid = true;
                isInLiquidTested = true;
            }
        }
        else
        {
            isOnGround = true;                                  // player is on ground
        }

        if (isInLiquid || (!isInLiquidTested && unitTarget->GetMap()->GetTerrain()->IsInWater(nextPos.x, nextPos.y, nextPos.z, &liquidData)))
        {
            if (!isPrevInLiquid && fabs(liquidData.level - prevPos.z) > 2.0f)
            {
                // on edge of water with difference a bit to high to continue
                //sLog.outString("Ground vs liquid edge detected!");
                nextPos = prevPos;
                break;
            }

            if ((liquidData.level - IN_OR_UNDER_LIQUID_RANGE) > nextPos.z)
            {
                nextPos.z = prevPos.z;                                      // we are under water so next z equal prev z
            }
            else
            {
                nextPos.z = liquidData.level - IN_OR_UNDER_LIQUID_RANGE;    // we are on water surface, so next z equal liquid level
            }

            isInLiquid = true;

            float ground = nextPos.z;
            if (unitTarget->GetMap()->GetHeightInRange(nextPos.x, nextPos.y, ground))
            {
                if (nextPos.z < ground)
                {
                    nextPos.z = ground;
                    isOnGround = true;                          // player is on ground of the water
                }
            }
        }

        //unitTarget->SummonCreature(VISUAL_WAYPOINT, nextPos.x, nextPos.y, nextPos.z, 0, TEMPSUMMON_TIMED_DESPAWN, 15000);
        float hitZ = nextPos.z + 1.5f;
        if (unitTarget->GetMap()->GetHitPosition(prevPos.x, prevPos.y, prevPos.z + 1.5f, nextPos.x, nextPos.y, hitZ, -1.0f))
        {
            //sLog.outString("Blink collision detected!");
            nextPos = prevPos;
            break;
        }

        if (isOnGround)
        {
            // project vector to get only positive value
            float ac = fabs(prevPos.z - nextPos.z);

            // compute slope (in radian)
            float slope = atan(ac / step);

            // check slope value
            if (slope > MAX_SLOPE_IN_RADIAN)
            {
                //sLog.outString("bad slope detected! %4.2f max %4.2f, ac(%4.2f)", slope * 180 / M_PI_F, maxSlope, ac);
                nextPos = prevPos;
                break;
            }
            //sLog.outString("slope is ok! %4.2f max %4.2f, ac(%4.2f)", slope * 180 / M_PI_F, maxSlope, ac);
        }

        //sLog.outString("point %u is ok, coords %s", i, nextPos.toString().c_str());
        nextZPointEstimation = (nextPos.z - prevPos.z) / 2.0f;
        isPrevInLiquid = isInLiquid;
        prevPos = nextPos;
    }

    unitTarget->NearTeleportTo(nextPos.x, nextPos.y, nextPos.z, orientation, unitTarget == m_caster);
}

/**
 * @brief Modifies player reputation for the faction referenced by the effect.
 *
 * @param eff_idx The effect index containing faction and reputation values.
 */
void Spell::EffectReputation(SpellEffectIndex eff_idx)
{
    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }

    Player* _player = (Player*)unitTarget;

    int32  rep_change = m_currentBasePoints[eff_idx];
    uint32 faction_id = m_spellInfo->EffectMiscValue[eff_idx];

    FactionEntry const* factionEntry = sFactionStore.LookupEntry(faction_id);

    if (!factionEntry)
    {
        return;
    }

    rep_change = _player->CalculateReputationGain(REPUTATION_SOURCE_SPELL, rep_change, faction_id);

    _player->GetReputationMgr().ModifyReputation(factionEntry, rep_change);
}

/**
 * @brief Marks the referenced quest objective as completed for the player target.
 *
 * @param eff_idx The effect index containing the quest id.
 */
void Spell::EffectQuestComplete(SpellEffectIndex eff_idx)
{
    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }

    uint32 quest_id = m_spellInfo->EffectMiscValue[eff_idx];
    ((Player*)unitTarget)->AreaExploredOrEventHappens(quest_id);
}

/**
 * @brief Resurrects the target player with flat or percentage-based health and mana.
 *
 * @param eff_idx The effect index containing resurrection resource data.
 */
void Spell::EffectSelfResurrect(SpellEffectIndex eff_idx)
{
    if (!unitTarget || unitTarget->IsAlive())
    {
        return;
    }
    if (unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }
    if (!unitTarget->IsInWorld())
    {
        return;
    }

    uint32 health = 0;
    uint32 mana = 0;

    // flat case
    if (damage < 0)
    {
        health = uint32(-damage);
        mana = m_spellInfo->EffectMiscValue[eff_idx];
    }
    // percent case
    else
    {
        health = uint32(damage / 100.0f * unitTarget->GetMaxHealth());
        if (unitTarget->GetMaxPower(POWER_MANA) > 0)
        {
            mana = uint32(damage / 100.0f * unitTarget->GetMaxPower(POWER_MANA));
        }
    }

    Player* plr = ((Player*)unitTarget);
    plr->ResurrectPlayer(0.0f);

    plr->SetHealth(health);
    plr->SetPower(POWER_MANA, mana);
    plr->SetPower(POWER_RAGE, 0);
    plr->SetPower(POWER_ENERGY, plr->GetMaxPower(POWER_ENERGY));

    plr->SpawnCorpseBones();
}

/**
 * @brief Opens skinning loot for a creature and updates the player's gathering skill.
 *
 * @param eff_idx Unused effect index.
 */
void Spell::EffectSkinning(SpellEffectIndex /*eff_idx*/)
{
    if (unitTarget->GetTypeId() != TYPEID_UNIT)
    {
        return;
    }
    if (!m_caster || m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }

    Creature* creature = (Creature*) unitTarget;
    int32 targetLevel = creature->getLevel();

    uint32 skill = creature->GetCreatureInfo()->GetRequiredLootSkill();

    ((Player*)m_caster)->SendLoot(creature->GetObjectGuid(), LOOT_SKINNING);
    creature->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SKINNABLE);
    creature->SetLootRecipient(m_caster);

    int32 reqValue = targetLevel < 10 ? 0 : targetLevel < 20 ? (targetLevel - 10) * 10 : targetLevel * 5;

    int32 skillValue = ((Player*)m_caster)->GetPureSkillValue(skill);

    // Double chances for elites
    ((Player*)m_caster)->UpdateGatherSkill(skill, skillValue, reqValue, creature->IsElite() ? 2 : 1);
}

/**
 * @brief Moves the caster into melee contact with the target and starts attacking if appropriate.
 *
 * @param eff_idx Unused effect index.
 */
void Spell::EffectCharge(SpellEffectIndex /*eff_idx*/)
{
    if (!unitTarget)
    {
        return;
    }

    // TODO: research more ContactPoint/attack distance.
    // 3.666666 instead of ATTACK_DISTANCE(5.0f) in below seem to give more accurate result.
    float x, y, z;
    unitTarget->GetContactPoint(m_caster, x, y, z, 3.666666f);

    if (unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        ((Creature*)unitTarget)->StopMoving();
    }

    // Only send MOVEMENTFLAG_WALK_MODE, client has strange issues with other move flags
    m_caster->MonsterMoveWithSpeed(x, y, z, 24.f, true, true);

    // not all charge effects used in negative spells
    if (unitTarget != m_caster && !IsPositiveSpell(m_spellInfo->ID))
    {
        m_caster->Attack(unitTarget, true);
    }
}

/**
 * @brief Applies a knockback to a player target.
 *
 * @param eff_idx The effect index containing horizontal speed data.
 */
void Spell::EffectKnockBack(SpellEffectIndex eff_idx)
{
    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }

    ((Player*)unitTarget)->KnockBackFrom(m_caster, float(m_spellInfo->EffectMiscValue[eff_idx]) / 10, float(damage) / 10);
}

/**
 * @brief Starts the taxi path referenced by the spell effect for a player target.
 *
 * @param eff_idx The effect index containing the taxi path id.
 */
void Spell::EffectSendTaxi(SpellEffectIndex eff_idx)
{
    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }

    ((Player*)unitTarget)->ActivateTaxiPathTo(m_spellInfo->EffectMiscValue[eff_idx], m_spellInfo->ID);
}

/**
 * @brief Pulls a player target toward the caster using reverse knockback.
 *
 * @param eff_idx The effect index containing vertical speed data.
 */
void Spell::EffectPlayerPull(SpellEffectIndex eff_idx)
{
    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }

    float dist = unitTarget->GetDistance2d(m_caster);
    if (damage && dist > damage)
    {
        dist = float(damage);
    }

    ((Player*)unitTarget)->KnockBackFrom(m_caster, -dist, float(m_spellInfo->EffectMiscValue[eff_idx]) / 10);
}
