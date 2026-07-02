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

void Player::_ApplyItemMods(Item* item, uint8 slot, bool apply)
{
    if (slot >= INVENTORY_SLOT_BAG_END || !item)
    {
        return;
    }

    // not apply/remove mods for broken item
    if (item->IsBroken())
    {
        return;
    }

    ItemPrototype const* proto = item->GetProto();

    if (!proto)
    {
        return;
    }

    DETAIL_LOG("applying mods for item %u ", item->GetGUIDLow());

    uint32 attacktype = Player::GetAttackBySlot(slot);
    if (attacktype < MAX_ATTACK)
    {
        _ApplyWeaponDependentAuraMods(item, WeaponAttackType(attacktype), apply);
    }

    _ApplyItemBonuses(proto, slot, apply);

    if (slot == EQUIPMENT_SLOT_RANGED)
    {
        _ApplyAmmoBonuses();
    }

    ApplyItemEquipSpell(item, apply);
    ApplyEnchantment(item, apply);

    DEBUG_LOG("_ApplyItemMods complete.");
}

/**
 * @brief Applies or removes stat bonuses from an equipped item prototype.
 *
 * @param proto The item prototype supplying the bonuses.
 * @param slot The equipment slot receiving the bonuses.
 * @param apply True to apply the bonuses; false to remove them.
 */
void Player::_ApplyItemBonuses(ItemPrototype const* proto, uint8 slot, bool apply)
{
    if (slot >= INVENTORY_SLOT_BAG_END || !proto)
    {
        return;
    }

    for (uint32 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
    {
        float val = float(proto->ItemStat[i].ItemStatValue);

        if (val == 0)
        {
            break; // no point continuing through the loop, as this will signify no more stats of this variety (mana, strength, etc.) are to be added to the item
        }

        switch (proto->ItemStat[i].ItemStatType)
        {
            case ITEM_MOD_MANA:
                HandleStatModifier(UNIT_MOD_MANA, BASE_VALUE, float(val), apply);
                break;
            case ITEM_MOD_HEALTH:                           // modify HP
                HandleStatModifier(UNIT_MOD_HEALTH, BASE_VALUE, float(val), apply);
                break;
            case ITEM_MOD_AGILITY:                          // modify agility
                HandleStatModifier(UNIT_MOD_STAT_AGILITY, BASE_VALUE, float(val), apply);
                ApplyStatBuffMod(STAT_AGILITY, float(val), apply);
                break;
            case ITEM_MOD_STRENGTH:                         // modify strength
                HandleStatModifier(UNIT_MOD_STAT_STRENGTH, BASE_VALUE, float(val), apply);
                ApplyStatBuffMod(STAT_STRENGTH, float(val), apply);
                break;
            case ITEM_MOD_INTELLECT:                        // modify intellect
                HandleStatModifier(UNIT_MOD_STAT_INTELLECT, BASE_VALUE, float(val), apply);
                ApplyStatBuffMod(STAT_INTELLECT, float(val), apply);
                break;
            case ITEM_MOD_SPIRIT:                           // modify spirit
                HandleStatModifier(UNIT_MOD_STAT_SPIRIT, BASE_VALUE, float(val), apply);
                ApplyStatBuffMod(STAT_SPIRIT, float(val), apply);
                break;
            case ITEM_MOD_STAMINA:                          // modify stamina
                HandleStatModifier(UNIT_MOD_STAT_STAMINA, BASE_VALUE, float(val), apply);
                ApplyStatBuffMod(STAT_STAMINA, float(val), apply);
                break;
        }
    }

    if (proto->Armor)
    {
        HandleStatModifier(UNIT_MOD_ARMOR, BASE_VALUE, float(proto->Armor), apply);
    }

    if (proto->Block)
    {
        HandleBaseModValue(SHIELD_BLOCK_VALUE, FLAT_MOD, float(proto->Block), apply);
    }

    if (proto->HolyRes)
    {
        HandleStatModifier(UNIT_MOD_RESISTANCE_HOLY, BASE_VALUE, float(proto->HolyRes), apply);
    }

    if (proto->FireRes)
    {
        HandleStatModifier(UNIT_MOD_RESISTANCE_FIRE, BASE_VALUE, float(proto->FireRes), apply);
    }

    if (proto->NatureRes)
    {
        HandleStatModifier(UNIT_MOD_RESISTANCE_NATURE, BASE_VALUE, float(proto->NatureRes), apply);
    }

    if (proto->FrostRes)
    {
        HandleStatModifier(UNIT_MOD_RESISTANCE_FROST, BASE_VALUE, float(proto->FrostRes), apply);
    }

    if (proto->ShadowRes)
    {
        HandleStatModifier(UNIT_MOD_RESISTANCE_SHADOW, BASE_VALUE, float(proto->ShadowRes), apply);
    }

    if (proto->ArcaneRes)
    {
        HandleStatModifier(UNIT_MOD_RESISTANCE_ARCANE, BASE_VALUE, float(proto->ArcaneRes), apply);
    }

    WeaponAttackType attType = BASE_ATTACK;
    float damage = 0.0f;

    if (slot == EQUIPMENT_SLOT_RANGED &&
        (proto->InventoryType == INVTYPE_RANGED || proto->InventoryType == INVTYPE_THROWN ||
        proto->InventoryType == INVTYPE_RANGEDRIGHT))
    {
        attType = RANGED_ATTACK;
    }
    else if (slot == EQUIPMENT_SLOT_OFFHAND)
    {
        attType = OFF_ATTACK;
    }

    if (proto->Damage[0].DamageMin > 0)
    {
        damage = apply ? proto->Damage[0].DamageMin : BASE_MINDAMAGE;
        SetBaseWeaponDamage(attType, MINDAMAGE, damage);
        // sLog.outError("applying mindam: assigning %f to weapon mindamage, now is: %f", damage, GetWeaponDamageRange(attType, MINDAMAGE));
    }

    if (proto->Damage[0].DamageMax  > 0)
    {
        damage = apply ? proto->Damage[0].DamageMax : BASE_MAXDAMAGE;
        SetBaseWeaponDamage(attType, MAXDAMAGE, damage);
    }

    if (!CanUseEquippedWeapon(attType))
    {
        return;
    }

    if (proto->Delay)
    {
        if (slot == EQUIPMENT_SLOT_RANGED)
        {
            SetAttackTime(RANGED_ATTACK, apply ? proto->Delay : BASE_ATTACK_TIME);
        }
        else if (slot == EQUIPMENT_SLOT_MAINHAND)
        {
            SetAttackTime(BASE_ATTACK, apply ? proto->Delay : BASE_ATTACK_TIME);
        }
        else if (slot == EQUIPMENT_SLOT_OFFHAND)
        {
            SetAttackTime(OFF_ATTACK, apply ? proto->Delay : BASE_ATTACK_TIME);
        }
    }

    if (CanModifyStats() && (damage || proto->Delay))
    {
        UpdateDamagePhysical(attType);
    }
}

/**
 * @brief Applies or removes weapon-specific aura modifiers for an equipped weapon.
 *
 * @param item The equipped weapon item.
 * @param attackType The attack type affected by the weapon.
 * @param apply True to apply modifiers; false to remove them.
 */
void Player::_ApplyWeaponDependentAuraMods(Item* item, WeaponAttackType attackType, bool apply)
{
    AuraList const& auraCritList = GetAurasByType(SPELL_AURA_MOD_CRIT_PERCENT);
    for (AuraList::const_iterator itr = auraCritList.begin(); itr != auraCritList.end(); ++itr)
    {
        _ApplyWeaponDependentAuraCritMod(item, attackType, *itr, apply);
    }

    AuraList const& auraDamageFlatList = GetAurasByType(SPELL_AURA_MOD_DAMAGE_DONE);
    for (AuraList::const_iterator itr = auraDamageFlatList.begin(); itr != auraDamageFlatList.end(); ++itr)
    {
        _ApplyWeaponDependentAuraDamageMod(item, attackType, *itr, apply);
    }

    AuraList const& auraDamagePCTList = GetAurasByType(SPELL_AURA_MOD_DAMAGE_PERCENT_DONE);
    for (AuraList::const_iterator itr = auraDamagePCTList.begin(); itr != auraDamagePCTList.end(); ++itr)
    {
        _ApplyWeaponDependentAuraDamageMod(item, attackType, *itr, apply);
    }
}

/**
 * @brief Applies or removes a weapon-dependent critical strike aura modifier.
 *
 * @param item The equipped weapon item.
 * @param attackType The affected attack type.
 * @param aura The aura providing the modifier.
 * @param apply True to apply the modifier; false to remove it.
 */
void Player::_ApplyWeaponDependentAuraCritMod(Item* item, WeaponAttackType attackType, Aura* aura, bool apply)
{
    // generic not weapon specific case processes in aura code
    if (aura->GetSpellProto()->EquippedItemClass == -1)
    {
        return;
    }

    BaseModGroup mod = BASEMOD_END;
    switch (attackType)
    {
        case BASE_ATTACK:   mod = CRIT_PERCENTAGE;        break;
        case OFF_ATTACK:    mod = OFFHAND_CRIT_PERCENTAGE; break;
        case RANGED_ATTACK: mod = RANGED_CRIT_PERCENTAGE; break;
        default: return;
    }

    if (item->IsFitToSpellRequirements(aura->GetSpellProto()))
    {
        HandleBaseModValue(mod, FLAT_MOD, float(aura->GetModifier()->m_amount), apply);
    }
}

/**
 * @brief Applies or removes a weapon-dependent damage aura modifier.
 *
 * @param item The equipped weapon item.
 * @param attackType The affected attack type.
 * @param aura The aura providing the modifier.
 * @param apply True to apply the modifier; false to remove it.
 */
void Player::_ApplyWeaponDependentAuraDamageMod(Item* item, WeaponAttackType attackType, Aura* aura, bool apply)
{
    // ignore spell mods for not wands
    Modifier const* modifier = aura->GetModifier();
    if ((modifier->m_miscvalue & SPELL_SCHOOL_MASK_NORMAL) == 0 && (getClassMask() & CLASSMASK_WAND_USERS) == 0)
    {
        return;
    }

    // generic not weapon specific case processes in aura code
    if (aura->GetSpellProto()->EquippedItemClass == -1)
    {
        return;
    }

    UnitMods unitMod = UNIT_MOD_END;
    switch (attackType)
    {
        case BASE_ATTACK:   unitMod = UNIT_MOD_DAMAGE_MAINHAND; break;
        case OFF_ATTACK:    unitMod = UNIT_MOD_DAMAGE_OFFHAND;  break;
        case RANGED_ATTACK: unitMod = UNIT_MOD_DAMAGE_RANGED;   break;
        default: return;
    }

    UnitModifierType unitModType = TOTAL_VALUE;
    switch (modifier->m_auraname)
    {
        case SPELL_AURA_MOD_DAMAGE_DONE:         unitModType = TOTAL_VALUE; break;
        case SPELL_AURA_MOD_DAMAGE_PERCENT_DONE: unitModType = TOTAL_PCT;   break;
        default: return;
    }

    if (item->IsFitToSpellRequirements(aura->GetSpellProto()))
    {
        HandleStatModifier(unitMod, unitModType, float(modifier->m_amount), apply);
    }
}

/**
 * @brief Applies or removes all item spells associated with an equipped item.
 *
 * @param item The equipped item to process.
 * @param apply True to apply item equip effects; false to remove them.
 * @param form_change True if the update is caused by a shapeshift form change.
 */
void Player::ApplyItemEquipSpell(Item* item, bool apply, bool form_change)
{
    if (!item)
    {
        return;
    }

    ItemPrototype const* proto = item->GetProto();
    if (!proto)
    {
        return;
    }

    for (int i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
    {
        _Spell const& spellData = proto->Spells[i];

        // no spell
        if (!spellData.SpellId)
        {
            continue;
        }

        if (apply)
        {
            // apply only at-equip spells
            if (spellData.SpellTrigger != ITEM_SPELLTRIGGER_ON_EQUIP)
            {
                continue;
            }
        }
        else
        {
            // at un-apply remove all spells (not only at-apply, so any at-use active affects from item and etc)
            // except on form change and with at-use with negative charges, so allow consuming item spells (including with extra flag that prevent consume really)
            // applied to player after item remove from equip slot
            if (spellData.SpellTrigger == ITEM_SPELLTRIGGER_ON_USE && (form_change || spellData.SpellCharges < 0))
            {
                continue;
            }
        }

        // check if it is valid spell
        SpellEntry const* spellproto = sSpellStore.LookupEntry(spellData.SpellId);
        if (!spellproto)
        {
            continue;
        }

        ApplyEquipSpell(spellproto, item, apply, form_change);
    }
}

/**
 * @brief Applies or removes a single equipment-derived spell effect.
 *
 * @param spellInfo The spell entry to process.
 * @param item The source item, or null for item set bonuses.
 * @param apply True to apply the spell effect; false to remove it.
 * @param form_change True if the update is caused by a shapeshift form change.
 */
void Player::ApplyEquipSpell(SpellEntry const* spellInfo, Item* item, bool apply, bool form_change)
{
    if (apply)
    {
        // Can not be used in this stance/form
        if (GetErrorAtShapeshiftedCast(spellInfo, GetShapeshiftForm()) != SPELL_CAST_OK)
        {
            return;
        }

        if (form_change)                                    // check aura active state from other form
        {
            bool found = false;
            for (int k = 0; k < MAX_EFFECT_INDEX; ++k)
            {
                SpellAuraHolderBounds spair = GetSpellAuraHolderBounds(spellInfo->Id);
                for (SpellAuraHolderMap::const_iterator iter = spair.first; iter != spair.second; ++iter)
                {
                    if (!item || iter->second->GetCastItemGuid() == item->GetObjectGuid())
                    {
                        found = true;
                        break;
                    }
                }
                if (found)
                {
                    break;
                }
            }

            if (found)                                      // and skip re-cast already active aura at form change
            {
                return;
            }
        }

        DEBUG_LOG("WORLD: cast %s Equip spellId - %i", (item ? "item" : "itemset"), spellInfo->Id);

        CastSpell(this, spellInfo, true, item);
    }
    else
    {
        if (form_change)                                    // check aura compatibility
        {
            // Can not be used in this stance/form
            if (GetErrorAtShapeshiftedCast(spellInfo, GetShapeshiftForm()) == SPELL_CAST_OK)
            {
                return; // and remove only not compatible at form change
            }
        }

        if (item)
        {
            RemoveAurasDueToItemSpell(item, spellInfo->Id); // un-apply all spells , not only at-equipped
        }
        else
        {
            RemoveAurasDueToSpell(spellInfo->Id); // un-apply spell (item set case)
        }
    }
}

/**
 * @brief Re-evaluates equipment and item set spells after a form change.
 */
void Player::UpdateEquipSpellsAtFormChange()
{
    for (int i = 0; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (m_items[i] && !m_items[i]->IsBroken())
        {
            ApplyItemEquipSpell(m_items[i], false, true);   // remove spells that not fit to form
            ApplyItemEquipSpell(m_items[i], true, true);    // add spells that fit form but not active
        }
    }

    // item set bonuses not dependent from item broken state
    for (size_t setindex = 0; setindex < ItemSetEff.size(); ++setindex)
    {
        ItemSetEffect* eff = ItemSetEff[setindex];
        if (!eff)
        {
            continue;
        }

        for (uint32 y = 0; y < 8; ++y)
        {
            SpellEntry const* spellInfo = eff->spells[y];
            if (!spellInfo)
            {
                continue;
            }

            ApplyEquipSpell(spellInfo, NULL, false, true);  // remove spells that not fit to form
            ApplyEquipSpell(spellInfo, NULL, true, true);   // add spells that fit form but not active
        }
    }
}

/**
 * @brief Triggers item and enchantment combat procs for an attack.
 *
 * @param Target The unit struck by the attack.
 * @param attType The attack type that caused the proc check.
 */
void Player::CastItemCombatSpell(Unit* Target, WeaponAttackType attType)
{
    Item* item = GetWeaponForAttack(attType, true, true);
    if (!item)
    {
        return;
    }

    ItemPrototype const* proto = item->GetProto();
    if (!proto)
    {
        return;
    }

    if (!Target || Target == this)
    {
        return;
    }

    for (int i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
    {
        _Spell const& spellData = proto->Spells[i];

        // no spell
        if (!spellData.SpellId)
        {
            continue;
        }

        // wrong triggering type
        if (spellData.SpellTrigger != ITEM_SPELLTRIGGER_CHANCE_ON_HIT)
        {
            continue;
        }

        SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellData.SpellId);
        if (!spellInfo)
        {
            sLog.outError("WORLD: unknown Item spellid %i", spellData.SpellId);
            continue;
        }

        // not allow proc extra attack spell at extra attack
        if (m_extraAttacks && spellInfo->HasSpellEffect(SPELL_EFFECT_ADD_EXTRA_ATTACKS))
        {
            return;
        }

        float chance = (float)spellInfo->procChance;

        if (spellData.SpellPPMRate)
        {
            uint32 WeaponSpeed = proto->Delay;
            chance = GetPPMProcChance(WeaponSpeed, spellData.SpellPPMRate);
        }
        else if (chance > 100.0f)
        {
            chance = GetWeaponProcChance();
        }

        if (roll_chance_f(chance))
        {
            CastSpell(Target, spellInfo->Id, true, item);
        }
    }

    // item combat enchantments
    for (int e_slot = 0; e_slot < MAX_ENCHANTMENT_SLOT; ++e_slot)
    {
        uint32 enchant_id = item->GetEnchantmentId(EnchantmentSlot(e_slot));
        SpellItemEnchantmentEntry const* pEnchant = sSpellItemEnchantmentStore.LookupEntry(enchant_id);
        if (!pEnchant)
        {
            continue;
        }
        for (int s = 0; s < 3; ++s)
        {
            uint32 proc_spell_id = pEnchant->spellid[s];

            if (pEnchant->type[s] != ITEM_ENCHANTMENT_TYPE_COMBAT_SPELL)
            {
                continue;
            }

            SpellEntry const* spellInfo = sSpellStore.LookupEntry(proc_spell_id);
            if (!spellInfo)
            {
                sLog.outError("Player::CastItemCombatSpell Enchant %i, cast unknown spell %i", pEnchant->ID, proc_spell_id);
                continue;
            }

            // Use first rank to access spell item enchant procs
            float ppmRate = sSpellMgr.GetItemEnchantProcChance(spellInfo->Id);

            float chance = ppmRate
                ? GetPPMProcChance(proto->Delay, ppmRate)
                : pEnchant->amount[s] != 0 ? float(pEnchant->amount[s]) : GetWeaponProcChance();

            ApplySpellMod(spellInfo->Id, SPELLMOD_CHANCE_OF_SUCCESS, chance);

            if (roll_chance_f(chance))
            {
                if (IsPositiveSpell(spellInfo->Id))
                {
                    CastSpell(this, spellInfo->Id, true, item);
                }
                else
                {
                    CastSpell(Target, spellInfo->Id, true, item);
                }
            }
        }
    }
}

/**
 * @brief Casts all on-use spells provided by an item.
 *
 * @param item The item being used.
 * @param targets The prepared spell targets for the cast.
 */
void Player::CastItemUseSpell(Item* item, SpellCastTargets const& targets)
{
    ItemPrototype const* proto = item->GetProto();

    // use triggered flag only for items with many spell casts and for not first cast
    int count = 0;

    // item spells casted at use
    for (int i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
    {
        _Spell const& spellData = proto->Spells[i];

        // no spell
        if (!spellData.SpellId)
        {
            continue;
        }

        // wrong triggering type
        if (spellData.SpellTrigger != ITEM_SPELLTRIGGER_ON_USE && spellData.SpellTrigger != ITEM_SPELLTRIGGER_ON_NO_DELAY_USE)
        {
            continue;
        }

        SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellData.SpellId);
        if (!spellInfo)
        {
            sLog.outError("Player::CastItemUseSpell: Item (Entry: %u) in have wrong spell id %u, ignoring", proto->ItemId, spellData.SpellId);
            continue;
        }

        Spell* spell = new Spell(this, spellInfo, (count > 0));
        spell->m_CastItem = item;
        spell->prepare(&targets);

        ++count;
    }
}

/**
 * @brief Removes all item-derived modifiers, enchantments, and equip effects.
 */
void Player::_RemoveAllItemMods()
{
    DEBUG_LOG("_RemoveAllItemMods start.");

    for (int i = 0; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (m_items[i])
        {
            ItemPrototype const* proto = m_items[i]->GetProto();
            if (!proto)
            {
                continue;
            }

            // item set bonuses not dependent from item broken state
            if (proto->ItemSet)
            {
                RemoveItemsSetItem(this, proto);
            }

            if (m_items[i]->IsBroken())
            {
                continue;
            }

            ApplyItemEquipSpell(m_items[i], false);
            ApplyEnchantment(m_items[i], false);
        }
    }

    for (int i = 0; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (m_items[i])
        {
            if (m_items[i]->IsBroken())
            {
                continue;
            }
            ItemPrototype const* proto = m_items[i]->GetProto();
            if (!proto)
            {
                continue;
            }

            uint32 attacktype = Player::GetAttackBySlot(i);
            if (attacktype < MAX_ATTACK)
            {
                _ApplyWeaponDependentAuraMods(m_items[i], WeaponAttackType(attacktype), false);
            }

            _ApplyItemBonuses(proto, i, false);

            if (i == EQUIPMENT_SLOT_RANGED)
            {
                _ApplyAmmoBonuses();
            }
        }
    }

    DEBUG_LOG("_RemoveAllItemMods complete.");
}

/**
 * @brief Applies all item-derived modifiers, set bonuses, enchantments, and equip effects.
 */
void Player::_ApplyAllItemMods()
{
    DEBUG_LOG("_ApplyAllItemMods start.");

    for (int i = 0; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (m_items[i])
        {
            if (m_items[i]->IsBroken())
            {
                continue;
            }

            ItemPrototype const* proto = m_items[i]->GetProto();
            if (!proto)
            {
                continue;
            }

            uint32 attacktype = Player::GetAttackBySlot(i);
            if (attacktype < MAX_ATTACK)
            {
                _ApplyWeaponDependentAuraMods(m_items[i], WeaponAttackType(attacktype), true);
            }

            _ApplyItemBonuses(proto, i, true);

            if (i == EQUIPMENT_SLOT_RANGED)
            {
                _ApplyAmmoBonuses();
            }
        }
    }

    for (int i = 0; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (m_items[i])
        {
            ItemPrototype const* proto = m_items[i]->GetProto();
            if (!proto)
            {
                continue;
            }

            // item set bonuses not dependent from item broken state
            if (proto->ItemSet)
            {
                AddItemsSetItem(this, m_items[i]);
            }

            if (m_items[i]->IsBroken())
            {
                continue;
            }

            ApplyItemEquipSpell(m_items[i], true);
            ApplyEnchantment(m_items[i], true);
        }
    }

    DEBUG_LOG("_ApplyAllItemMods complete.");
}

/**
 * @brief Updates ranged damage bonuses from the currently selected ammo.
 */
void Player::_ApplyAmmoBonuses()
{
    // check ammo
    uint32 ammo_id = GetUInt32Value(PLAYER_AMMO_ID);
    if (!ammo_id)
    {
        return;
    }

    float currentAmmoDPSMin;
    float currentAmmoDPSMax;

    ItemPrototype const* ammo_proto = ObjectMgr::GetItemPrototype(ammo_id);
    if (!ammo_proto || ammo_proto->Class != ITEM_CLASS_PROJECTILE || !CheckAmmoCompatibility(ammo_proto))
    {
        currentAmmoDPSMin = 0.f;
        currentAmmoDPSMax = 0.f;
    }
    else
    {
        currentAmmoDPSMin = ammo_proto->Damage[0].DamageMin;
        currentAmmoDPSMax = ammo_proto->Damage[0].DamageMax;
    }

    if (std::make_pair(currentAmmoDPSMin, currentAmmoDPSMax) == GetAmmoDPS())
    {
        return;
    }

    m_ammoDPSMin = currentAmmoDPSMin;
    m_ammoDPSMax = currentAmmoDPSMax;

    if (CanModifyStats())
    {
        UpdateDamagePhysical(RANGED_ATTACK);
    }
}

/**
 * @brief Checks whether a projectile item is compatible with the equipped ranged weapon.
 *
 * @param ammo_proto The ammo item prototype to validate.
 * @return True if the ammo can be used with the equipped ranged weapon; otherwise, false.
 */
bool Player::CheckAmmoCompatibility(const ItemPrototype* ammo_proto) const
{
    if (!ammo_proto)
    {
        return false;
    }

    // check ranged weapon
    Item* weapon = GetWeaponForAttack(RANGED_ATTACK, true, false);
    if (!weapon)
    {
        return false;
    }

    ItemPrototype const* weapon_proto = weapon->GetProto();
    if (!weapon_proto || weapon_proto->Class != ITEM_CLASS_WEAPON)
    {
        return false;
    }

    // check ammo ws. weapon compatibility
    switch (weapon_proto->SubClass)
    {
        case ITEM_SUBCLASS_WEAPON_BOW:
        case ITEM_SUBCLASS_WEAPON_CROSSBOW:
            if (ammo_proto->SubClass != ITEM_SUBCLASS_ARROW)
            {
                return false;
            }
            break;
        case ITEM_SUBCLASS_WEAPON_GUN:
            if (ammo_proto->SubClass != ITEM_SUBCLASS_BULLET)
            {
                return false;
            }
            break;
        default:
            return false;
    }

    return true;
}
