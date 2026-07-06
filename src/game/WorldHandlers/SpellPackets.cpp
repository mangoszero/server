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
 * @file Spell.cpp
 * @brief Spell casting and effect implementation
 *
 * This file implements the Spell class which handles spell casting:
 * - Spell validation and casting requirements
 * - Spell effect execution (damage, healing, summon, etc.)
 * - Spell targeting and area effects
 * - Spell cooldowns and resource costs
 * - Spell interruption and pushback
 * - Spell aura application
 * - Spell hit/miss calculations
 *
 * Spells are the primary combat mechanic in WoW, encompassing
 * abilities, talents, and item effects.
 *
 * @see Spell for the spell class
 * @see SpellAura for spell auras
 * @see SpellMgr for spell management
 */



#include "Spell.h"
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Opcodes.h"
#include "Log.h"
#include "UpdateMask.h"
#include "World.h"
#include "ObjectMgr.h"
#include "SpellMgr.h"
#include "Player.h"
#include "Pet.h"
#include "Unit.h"
#include "DynamicObject.h"
#include "Group.h"
#include "UpdateData.h"
#include "MapManager.h"
#include "ObjectAccessor.h"
#include "CellImpl.h"
#include "Policies/Singleton.h"
#include "SharedDefines.h"
#include "LootMgr.h"
#include "VMapFactory.h"
#include "BattleGround/BattleGround.h"
#include "Util.h"
#include "Chat.h"
#include "TemporarySummon.h"
#include "SQLStorages.h"
#include "DisableMgr.h"
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */

/**
 * @brief Sends the cast result for this spell to the appropriate receiver.
 *
 * @param result The cast result code.
 */
void Spell::SendCastResult(SpellCastResult result)
{
    if (m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        if (((Creature*)m_caster)->AI())
        {
            ((Creature*)m_caster)->AI()->OnSpellCastChange(m_spellInfo, result);
        }
        return;
    }

    if (((Player*)m_caster)->GetSession()->PlayerLoading()) // don't send cast results at loading time
    {
        return;
    }

    // Reseting emote state for case not handled by the client.
    if (result == SPELL_FAILED_CHEST_IN_USE)
    {
        SendInterrupted(result);
    }

    SendCastResult((Player*)m_caster, m_spellInfo, result);
}

/**
 * @brief Sends a cast result packet for a specific player and spell.
 *
 * @param caster The player receiving the result.
 * @param spellInfo The spell being reported.
 * @param result The cast result code.
 */
void Spell::SendCastResult(Player* caster, SpellEntry const* spellInfo, SpellCastResult result)
{
    WorldPacket data(SMSG_CAST_FAILED, (4 + 1 + 1));
    data << uint32(spellInfo->ID);

    if (result != SPELL_CAST_OK)
    {
        data << uint8(2); // status = fail
        data << uint8(!IsPassiveSpell(spellInfo) ? result : SPELL_FAILED_DONT_REPORT); // do not report failed passive spells
        switch (result)
        {
            case SPELL_FAILED_REQUIRES_SPELL_FOCUS:
                data << uint32(spellInfo->RequiresSpellFocus);
                break;
            case SPELL_FAILED_REQUIRES_AREA:
                break;
            case SPELL_FAILED_EQUIPPED_ITEM_CLASS:
                data << uint32(spellInfo->EquippedItemClass);
                data << uint32(spellInfo->EquippedItemSubclass);
                data << uint32(spellInfo->EquippedItemInvTypes);
                break;
            default:
                break;
        }
    }
    else
    {
        data << uint8(0);
    }

    caster->GetSession()->SendPacket(&data);
}

/**
 * @brief Sends the spell start packet for visible casts.
 */
void Spell::SendSpellStart()
{
    if (!IsNeedSendToClient())
    {
        return;
    }

    DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "Sending SMSG_SPELL_START id=%u", m_spellInfo->ID);

    uint32 castFlags = CAST_FLAG_UNKNOWN2;
    if (IsRangedSpell())
    {
        castFlags |= CAST_FLAG_AMMO;
    }

    WorldPacket data(SMSG_SPELL_START, (8 + 8 + 4 + 2 + 4));
    if (m_CastItem)
    {
        data << m_CastItem->GetPackGUID();
    }
    else
    {
        data << m_caster->GetPackGUID();
    }

    data << m_caster->GetPackGUID();
    data << uint32(m_spellInfo->ID);                        // spellId
    data << uint16(castFlags);                              // cast flags
    data << uint32(m_timer);                                // delay?

    data << m_targets;

    if (castFlags & CAST_FLAG_AMMO)                         // projectile info
    {
        WriteAmmoToPacket(&data);
    }

    m_caster->SendMessageToSet(&data, true);
}

/**
 * @brief Sends the spell go packet for visible casts.
 */
void Spell::SendSpellGo()
{
    // not send invisible spell casting
    if (!IsNeedSendToClient())
    {
        return;
    }

    DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "Sending SMSG_SPELL_GO id=%u", m_spellInfo->ID);

    uint32 castFlags = CAST_FLAG_UNKNOWN9;
    if (IsRangedSpell())
    {
        castFlags |= CAST_FLAG_AMMO;                         // arrows/bullets visual
    }

    WorldPacket data(SMSG_SPELL_GO, 53);                    // guess size

    if (m_CastItem)
    {
        data << m_CastItem->GetPackGUID();
    }
    else
    {
        data << m_caster->GetPackGUID();
    }

    data << m_caster->GetPackGUID();
    data << uint32(m_spellInfo->ID);                        // spellId
    data << uint16(castFlags);                              // cast flags

    WriteSpellGoTargets(&data);

    data << m_targets;

    if (castFlags & CAST_FLAG_AMMO)                         // projectile info
    {
        WriteAmmoToPacket(&data);
    }

    m_caster->SendMessageToSet(&data, true);
}

/**
 * @brief Writes projectile display and inventory type data into a packet.
 *
 * @param data The packet being populated.
 */
void Spell::WriteAmmoToPacket(WorldPacket* data)
{
    uint32 ammoInventoryType = 0;
    uint32 ammoDisplayID = 0;

    if (m_caster->GetTypeId() == TYPEID_PLAYER)
    {
        Item* pItem = ((Player*)m_caster)->GetWeaponForAttack(RANGED_ATTACK);
        if (pItem)
        {
            ammoInventoryType = pItem->GetProto()->InventoryType;
            if (ammoInventoryType == INVTYPE_THROWN)
            {
                ammoDisplayID = pItem->GetProto()->DisplayInfoID;
            }
            else
            {
                uint32 ammoID = ((Player*)m_caster)->GetUInt32Value(PLAYER_AMMO_ID);
                if (ammoID)
                {
                    ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(ammoID);
                    if (pProto)
                    {
                        ammoDisplayID = pProto->DisplayInfoID;
                        ammoInventoryType = pProto->InventoryType;
                    }
                }
            }
        }
    }
    else
    {
        for (uint8 i = 0; i < MAX_VIRTUAL_ITEM_SLOT; ++i)
        {
            // see Creature::SetVirtualItem for structure data
            if (uint32 item_class = m_caster->GetByteValue(UNIT_VIRTUAL_ITEM_INFO + (i * 2) + 0, VIRTUAL_ITEM_INFO_0_OFFSET_CLASS))
            {
                if (item_class == ITEM_CLASS_WEAPON)
                {
                    switch (m_caster->GetByteValue(UNIT_VIRTUAL_ITEM_INFO + (i * 2) + 0, VIRTUAL_ITEM_INFO_0_OFFSET_SUBCLASS))
                    {
                        case ITEM_SUBCLASS_WEAPON_THROWN:
                            ammoDisplayID = m_caster->GetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_DISPLAY + i);
                            ammoInventoryType = m_caster->GetByteValue(UNIT_VIRTUAL_ITEM_INFO + (i * 2) + 0, VIRTUAL_ITEM_INFO_0_OFFSET_INVENTORYTYPE);
                            break;
                        case ITEM_SUBCLASS_WEAPON_BOW:
                        case ITEM_SUBCLASS_WEAPON_CROSSBOW:
                            ammoDisplayID = 5996;           // is this need fixing?
                            ammoInventoryType = INVTYPE_AMMO;
                            break;
                        case ITEM_SUBCLASS_WEAPON_GUN:
                            ammoDisplayID = 5998;           // is this need fixing?
                            ammoInventoryType = INVTYPE_AMMO;
                            break;
                    }

                    if (ammoDisplayID)
                    {
                        break;
                    }
                }
            }
        }
    }

    *data << uint32(ammoDisplayID);
    *data << uint32(ammoInventoryType);
}

/**
 * @brief Writes spell target guids into the spell-go packet and updates alive-target tracking.
 *
 * @param data The packet being populated.
 */
void Spell::WriteSpellGoTargets(WorldPacket* data)
{
    // This function also fill data for channeled spells:
    // m_needAliveTargetMask req for stop channeling if one target die
    // Always hits on GO and expected all targets for Units
    *data << (uint8)(m_UniqueTargetInfo.size() + m_UniqueGOTargetInfo.size());

    for (TargetList::iterator ihit = m_UniqueTargetInfo.begin(); ihit != m_UniqueTargetInfo.end(); ++ihit)
    {
        *data << ihit->targetGUID;                          // in 1.12.1 expected all targets

        if (ihit->effectMask == 0)                          // No effect apply - all immuned add state
        {
            // possibly SPELL_MISS_IMMUNE2 for this??
            ihit->missCondition = SPELL_MISS_IMMUNE2;
        }
        else if (ihit->missCondition == SPELL_MISS_NONE)    // Add only hits
        {
            m_needAliveTargetMask |= ihit->effectMask;
        }
    }

    for (GOTargetList::const_iterator ighit = m_UniqueGOTargetInfo.begin(); ighit != m_UniqueGOTargetInfo.end(); ++ighit)
    {
        *data << ighit->targetGUID;                          // Always hits
    }

    *data << uint8(0);                                      // unknown, not miss

    // Reset m_needAliveTargetMask for non channeled spell
    if (!IsChanneledSpell(m_spellInfo))
    {
        m_needAliveTargetMask = 0;
    }
}

/**
 * @brief Sends the spell log execute packet for special client-side effect logging.
 */
void Spell::SendLogExecute()
{

    WorldPacket data(SMSG_SPELLLOGEXECUTE, (8 + 4 + 4 + (4 + 4 + 8)));  // estimate size

    data << m_caster->GetPackGUID();
    data << uint32(m_spellInfo->ID);

    size_t efcount_pos = data.wpos();
    int32 effectCount = 0;
    data << uint32(effectCount);                    // count1 (effect count) if <=0, SMSG ignored

    size_t starteff_pos = data.wpos();
    for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        data << uint32(m_spellInfo->Effect[i]);     // spell effect
        data << uint32(1);                          // count2 placeholder (target count)

        bool hasSpecial = true;
        {   // this block may be iterated (target count) times
            switch (m_spellInfo->Effect[i])
            {
                case SPELL_EFFECT_POWER_DRAIN:
                    data << m_targets.getUnitTargetGuid();
                    data << uint32(0);
                    data << uint32(0);
                    data << float(0);
                    break;
                case SPELL_EFFECT_ADD_EXTRA_ATTACKS:
                    data << m_targets.getUnitTargetGuid();
                    data << uint32(0);                      // count?
                    break;
                case SPELL_EFFECT_INTERRUPT_CAST:
                    data << m_targets.getUnitTargetGuid();
                    data << uint32(0);                      // spellid being interrupted
                    break;
                case SPELL_EFFECT_DURABILITY_DAMAGE:
                    data << m_targets.getUnitTargetGuid();
                    data << uint32(0);
                    data << uint32(0);                      // if both -1, a separate handling
                    break;
                case SPELL_EFFECT_CREATE_ITEM:              // here target is not the item but SELF
                    data << uint32(m_spellInfo->EffectItemType[i]);
                    break;
                case SPELL_EFFECT_FEED_PET:                 // here we may get both SELF and item targets
                    data << m_targets.getItemTargetEntry();
                    break;
                case SPELL_EFFECT_RESURRECT:
                case SPELL_EFFECT_DISPEL:
                case SPELL_EFFECT_THREAT:
                case SPELL_EFFECT_DISTRACT:
                case SPELL_EFFECT_SANCTUARY:
                case SPELL_EFFECT_THREAT_ALL:
                case SPELL_EFFECT_DISPEL_MECHANIC:
                case SPELL_EFFECT_RESURRECT_NEW:
                case SPELL_EFFECT_ATTACK_ME:
                case SPELL_EFFECT_SKIN_PLAYER_CORPSE:
                case SPELL_EFFECT_MODIFY_THREAT_PERCENT:
                case SPELL_EFFECT_126:
                case SPELL_EFFECT_DISMISS_PET:              // case is handled separately but has the same input
                case SPELL_EFFECT_OPEN_LOCK:                // 2 cases are handled separately but have the same input
                case SPELL_EFFECT_OPEN_LOCK_ITEM:
                case SPELL_EFFECT_INSTAKILL:                // separate. Also self instakill will not be logged by client
                    if (ObjectGuid guid = GetPrefilledOrUnitTargetGuid(SpellEffectIndex(i)))
                    {
                        data << guid;
                    }
                    else if (m_targets.getItemTargetGuid())
                    {
                        data << m_targets.getItemTargetGuid();
                    }
                    else if (m_targets.getGOTargetGuid())
                    {
                        data << m_targets.getGOTargetGuid();
                    }
                    break;
                case SPELL_EFFECT_DUMMY:
                    break;
                default:                                    // including SPELL_EFFECT_DUMMY w/separate handling
                    hasSpecial = false;                     // prevent duplicate logging for spells logged w/o target
                    break;
            }
        }
        if (hasSpecial)
        {
            ++effectCount;
            starteff_pos = data.wpos();
        }
        else
        {
            data.wpos(starteff_pos);
        }
    }
    if (!effectCount)                                   // no effect with special handling
    {
        effectCount = 1;
        data << uint32(m_spellInfo->Effect[EFFECT_INDEX_0]);
        data << uint32(1);
    }
    data.put<uint32>(efcount_pos, effectCount);

    m_caster->SendMessageToSet(&data, true);
}

/**
 * @brief Sends interruption packets for the current spell cast.
 *
 * @param result The interruption result code.
 */
void Spell::SendInterrupted(SpellCastResult result)
{
    Player *casterPlayer = m_caster->ToPlayer();

    if (casterPlayer)
    {
        WorldPacket data(SMSG_SPELL_FAILURE, (8 + 4 + 1));
        data << m_caster->GetObjectGuid();
        data << m_spellInfo->ID;
        data << uint8(result);
        casterPlayer->SendDirectMessage(&data);
    }

    WorldPacket data(SMSG_SPELL_FAILED_OTHER, (8 + 4));
    data << m_caster->GetObjectGuid();
    data << m_spellInfo->ID;
    if (casterPlayer)
    {
        casterPlayer->SendMessageToSetExcept(&data, casterPlayer);
    }
    else
    {
        m_caster->SendMessageToSet(&data, true);
    }
}

/**
 * @brief Sends channel progress updates and clears channel state when ending.
 *
 * @param time The remaining channel time.
 */
void Spell::SendChannelUpdate(uint32 time)
{
    if (time == 0)
    {
        // Reset farsight for some possessing auras of possessed summoned (as they might work with different aura types)
        if (m_spellInfo->HasAttribute(SPELL_ATTR_EX_FARSIGHT) && m_caster->GetTypeId() == TYPEID_PLAYER && m_caster->GetCharmGuid() &&
            !IsSpellHaveAura(m_spellInfo, SPELL_AURA_MOD_POSSESS) && !IsSpellHaveAura(m_spellInfo, SPELL_AURA_MOD_POSSESS_PET))
        {
            Player* player = (Player*)m_caster;
            // These Auras are applied to self, so get the possessed first
            Unit* possessed = player->GetCharm();

            player->SetCharm(NULL);
            if (possessed)
            {
                player->SetClientControl(possessed, 0);
            }
            player->SetMover(NULL);
            player->GetCamera().ResetView();
            player->RemovePetActionBar();

            if (possessed)
            {
                possessed->clearUnitState(UNIT_STAT_CONTROLLED);
                possessed->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_POSSESSED);
                possessed->SetCharmerGuid(ObjectGuid());
                // TODO - Requires more specials for target?

                // Some possessed might want to despawn?
                if (possessed->GetUInt32Value(UNIT_CREATED_BY_SPELL) == m_spellInfo->ID && possessed->GetTypeId() == TYPEID_UNIT)
                {
                    ((Creature*)possessed)->ForcedDespawn();
                }
            }
        }

        m_caster->RemoveAurasByCasterSpell(m_spellInfo->ID, m_caster->GetObjectGuid());

        ObjectGuid target_guid = m_caster->GetChannelObjectGuid();
        if (target_guid != m_caster->GetObjectGuid() && target_guid.IsUnit())
        {
            if (Unit* target = sObjectAccessor.GetUnit(*m_caster, target_guid))
            {
                target->RemoveAurasByCasterSpell(m_spellInfo->ID, m_caster->GetObjectGuid());
            }
        }

        // Only finish channeling when latest channeled spell finishes
        if (m_caster->GetUInt32Value(UNIT_CHANNEL_SPELL) != m_spellInfo->ID)
        {
            return;
        }

        m_caster->SetChannelObjectGuid(ObjectGuid());
        m_caster->SetUInt32Value(UNIT_CHANNEL_SPELL, 0);
    }

    if (m_caster->GetTypeId() == TYPEID_PLAYER)
    {
        WorldPacket data(MSG_CHANNEL_UPDATE, 4);
        data << uint32(time);
        ((Player*)m_caster)->SendDirectMessage(&data);
    }
}

/**
 * @brief Starts channeling visuals and channel state for the spell.
 *
 * @param duration The channel duration in milliseconds.
 */
void Spell::SendChannelStart(uint32 duration)
{
    WorldObject* target = NULL;

    // select dynobject created by first effect if any
    if (m_spellInfo->Effect[EFFECT_INDEX_0] == SPELL_EFFECT_PERSISTENT_AREA_AURA)
    {
        target = m_caster->GetDynObject(m_spellInfo->ID, EFFECT_INDEX_0);
    }
    // select first not resisted target from target list for _0_ effect
    else if (!m_UniqueTargetInfo.empty())
    {
        for (TargetList::const_iterator itr = m_UniqueTargetInfo.begin(); itr != m_UniqueTargetInfo.end(); ++itr)
        {
            if ((itr->effectMask & (1 << EFFECT_INDEX_0)) && itr->reflectResult == SPELL_MISS_NONE &&
                itr->targetGUID != m_caster->GetObjectGuid())
            {
                target = sObjectAccessor.GetUnit(*m_caster, itr->targetGUID);
                break;
            }
        }
    }
    else if (!m_UniqueGOTargetInfo.empty())
    {
        for (GOTargetList::const_iterator itr = m_UniqueGOTargetInfo.begin(); itr != m_UniqueGOTargetInfo.end(); ++itr)
        {
            if (itr->effectMask & (1 << EFFECT_INDEX_0))
            {
                target = m_caster->GetMap()->GetGameObject(itr->targetGUID);
                break;
            }
        }
    }

    if (m_caster->GetTypeId() == TYPEID_PLAYER)
    {
        WorldPacket data(MSG_CHANNEL_START, (4 + 4));
        data << uint32(m_spellInfo->ID);
        data << uint32(duration);
        ((Player*)m_caster)->SendDirectMessage(&data);
    }

    m_timer = duration;

    if (target)
    {
        m_caster->SetChannelObjectGuid(target->GetObjectGuid());
    }

    m_caster->SetUInt32Value(UNIT_CHANNEL_SPELL, m_spellInfo->ID);
}

/**
 * @brief Sends a resurrection request to the target player.
 *
 * @param target The player being offered resurrection.
 */
void Spell::SendResurrectRequest(Player* target)
{
    // Both players and NPCs can resurrect using spells - have a look at creature 28487 for example
    // However, the packet structure differs slightly

    const char* sentName = m_caster->GetTypeId() == TYPEID_PLAYER ? "" : m_caster->GetNameForLocaleIdx(target->GetSession()->GetSessionDbLocaleIndex());

    WorldPacket data(SMSG_RESURRECT_REQUEST, (8 + 4 + strlen(sentName) + 1 + 1 + 1));
    data << m_caster->GetObjectGuid();
    data << uint32(strlen(sentName) + 1);

    data << sentName;
    data << uint8(m_caster->isSpiritHealer());
    // override delay sent with SMSG_CORPSE_RECLAIM_DELAY, set instant resurrection for spells with this attribute
    data << uint8(!m_spellInfo->HasAttribute(SPELL_ATTR_EX3_IGNORE_RESURRECTION_TIMER));
    target->GetSession()->SendPacket(&data);
}
