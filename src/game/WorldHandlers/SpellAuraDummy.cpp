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
 * @file SpellAuras.cpp
 * @brief Spell aura implementation
 *
 * This file implements the SpellAura class which handles spell auras:
 * - Aura application and removal
 * - Aura effect processing (stat modifiers, DoTs, HoTs, etc.)
 * - Aura stacking rules
 * - Aura dispelling mechanics
 * - Aura periodic effects
 * - Aura duration management
 * - Aura visual effects
 *
 * Auras are persistent effects applied by spells that modify
 * unit stats, deal damage over time, or provide other benefits.
 *
 * @see SpellAura for the aura class
 * @see Spell for spell casting
 */



#include "SpellAuras.h"
#include "Common.h"
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "WorldSession.h"
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
#include "Group.h"
#include "UpdateData.h"
#include "ObjectAccessor.h"
#include "Policies/Singleton.h"
#include "Totem.h"
#include "Creature.h"
#include "Formulas.h"
#include "BattleGround/BattleGround.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "CreatureAI.h"
#include "ScriptMgr.h"
#include "Util.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "Language.h"
#include "TemporarySummon.h"
#include "MapManager.h"

void Aura::HandleAuraDummy(bool apply, bool Real)
{
    // spells required only Real aura add/remove
    if (!Real)
    {
        return;
    }

    Unit* target = GetTarget();

    if (!target || !target->IsAlive())
    {
        return;
    }

    // AT APPLY
    if (apply)
    {
        switch (GetSpellProto()->SpellClassSet)
        {
            case SPELLFAMILY_GENERIC:
            {
                switch (GetId())
                {
                    case 7057:                              // Haunting Spirits
                        // expected to tick with 30 sec period (tick part see in Aura::PeriodicTick)
                        m_isPeriodic = true;
                        m_modifier.periodictime = 30 * IN_MILLISECONDS;
                        m_periodicTimer = m_modifier.periodictime;
                        return;
                    case 10255:                             // Stoned
                    {
                        if (Unit* caster = GetCaster())
                        {
                            if (caster->GetTypeId() != TYPEID_UNIT)
                            {
                                return;
                            }

                            caster->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
                            caster->addUnitState(UNIT_STAT_ROOT);
                        }
                        return;
                    }
                    case 13139:                             // net-o-matic
                        // root to self part of (root_target->charge->root_self sequence
                        if (Unit* caster = GetCaster())
                        {
                            caster->CastSpell(caster, 13138, true, NULL, this);
                        }
                        return;
                    case 23183:                             // Mark of Frost
                    {
                        if (Unit* target = GetTarget())
                        {
                            if (target->HasAura(23182))
                            {
                                target->CastSpell(target, 23186, true, NULL, NULL, GetCaster()->GetObjectGuid());
                            }
                        }
                        return;
                    }
                    case 25042:                             // Mark of Nature
                    {
                        if (Unit* target = GetTarget())
                        {
                            if (target->HasAura(25040))
                            {
                                target->CastSpell(target, 25043, true, NULL, NULL, GetCaster()->GetObjectGuid());
                            }
                        }
                        return;
                    }
                    case 28832:                             // Mark of Korth'azz
                    case 28833:                             // Mark of Blaumeux
                    case 28834:                             // Mark of Rivendare
                    case 28835:                             // Mark of Zeliek
                    {
                        int32 damage;
                        switch (GetStackAmount())
                        {
                            case 1:
                                return;
                            case 2: damage =   500; break;
                            case 3: damage =  1500; break;
                            case 4: damage =  4000; break;
                            case 5: damage = 12500; break;
                            default:
                                damage = 14000 + 1000 * GetStackAmount();
                                break;
                        }

                        if (Unit* caster = GetCaster())
                        {
                            caster->CastCustomSpell(target, 28836, &damage, NULL, NULL, true, NULL, this);
                        }
                        return;
                    }
                }
                break;
            }
        }
    }
    // AT REMOVE
    else
    {
        if (IsQuestTameSpell(GetId()) && (GetAuraDuration() == 0))
        {
            Unit* caster = GetCaster();
            if (!caster || !caster->IsAlive())
            {
                return;
            }

            uint32 finalSpellId = 0;
            switch (GetId())
            {
                case 19548: finalSpellId = 19597; break;
                case 19674: finalSpellId = 19677; break;
                case 19687: finalSpellId = 19676; break;
                case 19688: finalSpellId = 19678; break;
                case 19689: finalSpellId = 19679; break;
                case 19692: finalSpellId = 19680; break;
                case 19693: finalSpellId = 19684; break;
                case 19694: finalSpellId = 19681; break;
                case 19696: finalSpellId = 19682; break;
                case 19697: finalSpellId = 19683; break;
                case 19699: finalSpellId = 19685; break;
                case 19700: finalSpellId = 19686; break;
            }

            if (finalSpellId)
            {
                caster->CastSpell(target, finalSpellId, true, NULL, this);
            }

            return;
        }

        switch (GetId())
        {
            case 126:                                       // Eye of Killrog
            {
                Unit* caster = GetCaster();
                if (!caster || caster->GetTypeId() != TYPEID_PLAYER)
                {
                    return;
                }
                TemporarySummon* eye = static_cast<TemporarySummon*>(caster->GetCharm());
                if (eye)
                {
                    if (eye->GetUInt32Value(UNIT_CREATED_BY_SPELL) == GetId())
                    {
                        eye->UnSummon();
                    }
                }
                return;
            }
            case 10255:                                     // Stoned
            {
                if (Unit* caster = GetCaster())
                {
                    if (caster->GetTypeId() != TYPEID_UNIT)
                    {
                        return;
                    }

                    // see dummy effect of spell 10254 for removal of flags etc
                    caster->CastSpell(caster, 10254, true);
                }
                return;
            }
            case 11826:                                     // Electromagnetic Gigaflux Reactivator
                if (m_removeMode != AURA_REMOVE_BY_EXPIRE)
                {
                    return;
                }

                if (Unit* caster = GetCaster())
                {
                    if (caster->GetTypeId() == TYPEID_PLAYER)
                    {
                        caster->CastSpell(target, 11828, true, ((Player*) caster)->GetItemByGuid(this->GetCastItemGuid()), this);
                    }
                }
                return;
            case 12479:                                     // Hex of Jammal'an
                target->CastSpell(target, 12480, true, NULL, this);
                return;
            case 12774:                                     // (DND) Belnistrasz Idol Shutdown Visual
            {
                if (m_removeMode == AURA_REMOVE_BY_DEATH)
                {
                    return;
                }

                // Idom Rool Camera Shake <- wtf, don't drink while making spellnames?
                if (Unit* caster = GetCaster())
                {
                    caster->CastSpell(caster, 12816, true);
                }

                return;
            }
            case 28169:                                     // Mutating Injection
            {
                // Mutagen Explosion
                target->CastSpell(target, 28206, true, NULL, this);
                // Poison Cloud
                target->CastSpell(target, 28240, true, NULL, this);
                return;
            }
        }

        if (m_removeMode == AURA_REMOVE_BY_DEATH)
        {
            // Stop caster Arcane Missle chanelling on death
            if (GetSpellProto()->SpellClassSet == SPELLFAMILY_MAGE && (GetSpellProto()->SpellClassMask & UI64LIT(0x0000000000000800)))
            {
                if (Unit* caster = GetCaster())
                {
                    caster->InterruptSpell(CURRENT_CHANNELED_SPELL);
                }

                return;
            }
        }
    }

    // AT APPLY & REMOVE
    switch (GetSpellProto()->SpellClassSet)
    {
        case SPELLFAMILY_GENERIC:
        {
            switch (GetId())
            {
                case 6606:                                  // Self Visual - Sleep Until Cancelled (DND)
                {
                    if (apply)
                    {
                        target->SetStandState(UNIT_STAND_STATE_SLEEP);
                        target->addUnitState(UNIT_STAT_ROOT);
                    }
                    else
                    {
                        target->clearUnitState(UNIT_STAT_ROOT);
                        target->SetStandState(UNIT_STAND_STATE_STAND);
                    }

                    return;
                }
                case 24658:                                 // Unstable Power
                {
                    if (apply)
                    {
                        Unit* caster = GetCaster();
                        if (!caster)
                        {
                            return;
                        }

                        caster->CastSpell(target, 24659, true, NULL, NULL, GetCasterGuid());
                    }
                    else
                    {
                        target->RemoveAurasDueToSpell(24659);
                    }
                    return;
                }
                case 24661:                                 // Restless Strength
                {
                    if (apply)
                    {
                        Unit* caster = GetCaster();
                        if (!caster)
                        {
                            return;
                        }

                        caster->CastSpell(target, 24662, true, NULL, NULL, GetCasterGuid());
                    }
                    else
                    {
                        target->RemoveAurasDueToSpell(24662);
                    }
                    return;
                }
                case 29266:                                 // Permanent Feign Death
                {
                    // Unclear what the difference really is between them.
                    // Some has effect1 that makes the difference, however not all.
                    // Some appear to be used depending on creature location, in water, at solid ground, in air/suspended, etc
                    // For now, just handle all the same way
                    if (target->GetTypeId() == TYPEID_UNIT)
                    {
                        target->SetFeignDeath(apply);
                    }

                    return;
                }
                case 27978:
                    if (apply)
                    {
                        target->m_AuraFlags |= UNIT_AURAFLAG_ALIVE_INVISIBLE;
                    }
                    else
                    {
                        target->m_AuraFlags &= ~UNIT_AURAFLAG_ALIVE_INVISIBLE;
                    }
                    return;
            }
            break;
        }
        case SPELLFAMILY_DRUID:
        {
            // Predatory Strikes
            if (target->GetTypeId() == TYPEID_PLAYER && GetSpellProto()->SpellIconID == 1563)
            {
                ((Player*)target)->UpdateAttackPowerAndDamage();
                return;
            }
            break;
        }
        case SPELLFAMILY_ROGUE:
        {
            break;
        }
        case SPELLFAMILY_HUNTER:
        {
            break;
        }
        case SPELLFAMILY_SHAMAN:
        {
            switch (GetId())
            {
                case 6495:                                  // Sentry Totem
                {
                    if (target->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    Totem* totem = target->GetTotem(TOTEM_SLOT_AIR);

                    if (totem && apply)
                    {
                        ((Player*)target)->GetCamera().SetView(totem);
                    }
                    else
                    {
                        ((Player*)target)->GetCamera().ResetView();
                    }

                    return;
                }
            }
            break;
        }
        case SPELLFAMILY_PALADIN:
        {
            // Seal of the Crusader deals less damage with each attack. -28% damage,multiple tests.
            if (GetSpellProto()->SpellIconID == 237 && GetSpellProto()->SpellClassMask & UI64LIT(0x00000200))
            {
                target->HandleStatModifier(UNIT_MOD_DAMAGE_MAINHAND, TOTAL_PCT, -28.0f, apply);
            }

            break;
        }
    }

    // pet auras
    if (PetAura const* petSpell = sSpellMgr.GetPetAura(GetId()))
    {
        if (apply)
        {
            target->AddPetAura(petSpell);
        }
        else
        {
            target->RemovePetAura(petSpell);
        }
        return;
    }

    if (target->GetTypeId() == TYPEID_PLAYER)
    {
        SpellAreaForAreaMapBounds saBounds = sSpellMgr.GetSpellAreaForAuraMapBounds(GetId());
        if (saBounds.first != saBounds.second)
        {
            uint32 zone, area;
            target->GetZoneAndAreaId(zone, area);

            for (SpellAreaForAreaMap::const_iterator itr = saBounds.first; itr != saBounds.second; ++itr)
            {
                itr->second->ApplyOrRemoveSpellIfCan((Player*)target, zone, area, false);
            }
        }
    }

    // script has to "handle with care", only use where data are not ok to use in the above code.
    if (target->GetTypeId() == TYPEID_UNIT)
    {
        sScriptMgr.OnAuraDummy(this, apply);
    }
}
