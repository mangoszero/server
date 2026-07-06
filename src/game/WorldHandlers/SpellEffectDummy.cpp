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
#include <random>
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */

/**
 * @brief Executes spell-specific dummy effect behavior.
 *
 * @param eff_idx The dummy effect index.
 */
void Spell::EffectDummy(SpellEffectIndex eff_idx)
{
    if (!unitTarget && !gameObjTarget && !itemTarget)
    {
        return;
    }

    // selection by spell family
    switch (m_spellInfo->SpellClassSet)
    {
        case SPELLFAMILY_GENERIC:
        {
            switch (m_spellInfo->ID)
            {
                case 3360:                                  // Curse of the Eye
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    uint32 spell_id = (unitTarget->getGender() == GENDER_MALE) ? 10651 : 10653;

                    m_caster->CastSpell(unitTarget, spell_id, true);
                    return;
                }
                case 7671:                                  // Transformation (human<->worgen)
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    // Transform Visual
                    unitTarget->CastSpell(unitTarget, 24085, true);
                    return;
                }
                case 8063:                                  // Deviate Fish
                {
                    if (m_caster->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    uint32 spell_id = 0;
                    switch (urand(1, 5))
                    {
                        case 1: spell_id = 8064; break;     // Sleepy
                        case 2: spell_id = 8065; break;     // Invigorate
                        case 3: spell_id = 8066; break;     // Shrink
                        case 4: spell_id = 8067; break;     // Party Time!
                        case 5: spell_id = 8068; break;     // Healthy Spirit
                    }
                    m_caster->CastSpell(m_caster, spell_id, true, NULL);
                    return;
                }
                case 8213:                                  // Savory Deviate Delight
                {
                    if (m_caster->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    uint32 spell_id = 0;
                    switch (urand(1, 2))
                    {
                        // Flip Out - ninja
                        case 1: spell_id = (m_caster->getGender() == GENDER_MALE ? 8219 : 8220); break;
                        // Yaaarrrr - pirate
                        case 2: spell_id = (m_caster->getGender() == GENDER_MALE ? 8221 : 8222); break;
                    }

                    m_caster->CastSpell(m_caster, spell_id, true, NULL);
                    return;
                }
                case 8344:                                  // Gnomish Universal Remote (ItemID: 7506)
                {
                    if (m_CastItem && unitTarget)
                    {
                        // 8345 - Control the machine | 8346 = Malfunction the machine (root) | 8347 = Taunt/enrage the machine
                        const uint32 spell_list[3] = { 8345, 8346, 8347 };
                        m_caster->CastSpell(unitTarget, spell_list[urand(0, 2)], true, m_CastItem);
                    }

                    return;
                }
                case 9204:                                  // Hate to Zero
                case 20538:
                case 26569:
                case 26637:
                {
                    m_caster->GetThreatManager().modifyThreatPercent(unitTarget, -100);
                    return;
                }
                case 9976:                                  // Polly Eats the E.C.A.C.
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_UNIT)
                    {
                        return;
                    }

                    // Summon Polly Jr.
                    unitTarget->CastSpell(unitTarget, 9998, true);

                    ((Creature*)unitTarget)->ForcedDespawn(100);
                    return;
                }
                case 10254:                                 // Stone Dwarf Awaken Visual
                {
                    if (m_caster->GetTypeId() != TYPEID_UNIT)
                    {
                        return;
                    }

                    // see spell 10255 (aura dummy)
                    m_caster->clearUnitState(UNIT_STAT_ROOT);
                    m_caster->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
                    return;
                }
                case 12975:                                 // Last Stand
                {
                    int32 healthModSpellBasePoints0 = int32(m_caster->GetMaxHealth() * 0.3);
                    m_caster->CastCustomSpell(m_caster, 12976, &healthModSpellBasePoints0, NULL, NULL, true, NULL);
                    return;
                }
                case 13120:                                 // net-o-matic
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    uint32 spell_id = 0;

                    uint32 roll = urand(0, 99);

                    if (roll < 2)                           // 2% for 30 sec self root (off-like chance unknown)
                    {
                        spell_id = 16566;
                    }
                    else if (roll < 4)                      // 2% for 20 sec root, charge to target (off-like chance unknown)
                    {
                        spell_id = 13119;
                    }
                    else                                    // normal root
                    {
                        spell_id = 13099;
                    }

                    m_caster->CastSpell(unitTarget, spell_id, true, NULL);
                    return;
                }
                case 13006:                                 // Gnomish Shrink Ray
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    uint32 roll = urand(0,99);
                    uint32 inner_roll = urand(1,3);

                    if (roll < 5) // 5% negative backfire
                    {
                        switch (inner_roll)
                        {
                            case 1:
                                m_caster->CastSpell(m_caster, 13003, true, m_CastItem);  // -250 AP + shrink caster
                                break;
                            case 2:
                                m_caster->CastSpell(m_caster, 13010, true, m_CastItem);  // -250AP + shrink all caster's party
                                break;
                            default:
                                unitTarget->CastSpell(unitTarget, 13004, true, NULL);    // +250AP + grow victim
                                break;
                        }
                    }
                    else if (roll < 25) // 20% positive backfire
                    {
                        m_caster->CastSpell(m_caster, 13004, true, m_CastItem);    // +250AP + grow caster's party
                    }
                    else
                    {
                        m_caster->CastSpell(unitTarget, 13003, true, m_CastItem);  // -250AP + shrink victim
                    }

                    return;
                }
                case 13180:                                 // Gnomish mind control cap
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    uint32 roll = urand(0,99);

                    if (roll < 5)                          // 5% victim MC the caster (off-like chance unknown)
                    {
                        unitTarget->CastSpell(m_caster, 13181, true, NULL);
                    }
                    else if (roll < 35)                    // 30% fail (off-like chance unknown)
                    {
                        return;
                    }
                    else                                   // 65% caster MC the victim (off-like chance unknown)
                    {
                        AddTriggeredSpell(13181);
                    }

                    return;
                }
                case 13278:                                // Gnomish Death Ray charging
                {
                    if (unitTarget)
                    {
                        m_caster->CastSpell(m_caster, 13493, true, NULL);
                    }

                    return;
                }
                case 13280:                                // Gnomish Death Ray ending charge
                {
                    if (unitTarget)
                    {
                        uint32 roll = urand(0,7);
                        int32 dmg[8] = {900, 1200, 1500, 1800, 2100, 2400, 2700, 3000};

                        m_caster->CastCustomSpell(unitTarget, 13279, &dmg[roll], NULL, NULL, true);
                    }
                    return;
                }
                case 13535:                                 // Tame Beast
                {
                    if (!m_originalCaster || m_originalCaster->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    Creature* channelTarget = m_originalCaster->GetMap()->GetCreature(m_originalCaster->GetChannelObjectGuid());

                    if (!channelTarget)
                    {
                        return;
                    }

                    m_originalCaster->CastSpell(channelTarget, 13481, true, NULL, NULL, m_originalCasterGUID, m_spellInfo);
                    return;
                }
                case 13489:
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    unitTarget->CastSpell(unitTarget, 14744, true);
                    return;
                }
                case 13567:                                 // Dummy Trigger
                {
                    // can be used for different aura triggering, so select by aura
                    if (!m_triggeredByAuraSpell || !unitTarget)
                    {
                        return;
                    }

                    switch (m_triggeredByAuraSpell->ID)
                    {
                        case 26467:                         // Persistent Shield
                            m_caster->CastCustomSpell(unitTarget, 26470, &damage, NULL, NULL, true);
                            break;
                        default:
                            sLog.outError("EffectDummy: Non-handled case for spell 13567 for triggered aura %u", m_triggeredByAuraSpell->ID);
                            break;
                    }
                    return;
                }
                case 14185:                                 // Preparation Rogue
                {
                    if (m_caster->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    // immediately finishes the cooldown on certain Rogue abilities
                    const SpellCooldowns& cm = ((Player*)m_caster)->GetSpellCooldownMap();
                    for (SpellCooldowns::const_iterator itr = cm.begin(); itr != cm.end();)
                    {
                        SpellEntry const* spellInfo = sSpellStore.LookupEntry(itr->first);

                        if (spellInfo->SpellClassSet == SPELLFAMILY_ROGUE &&
                            spellInfo->ID != m_spellInfo->ID && GetSpellRecoveryTime(spellInfo) > 0)
                        {
                            ((Player*)m_caster)->RemoveSpellCooldown((itr++)->first, true);
                        }
                        else
                        {
                            ++itr;
                        }
                    }
                    return;
                }
                case 14537:                                 // Six Demon Bag
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    Unit* newTarget = unitTarget;
                    uint32 spell_id = 0;
                    uint32 roll = urand(0, 99);
                    if (roll < 25)                          // Fireball (25% chance)
                    {
                        spell_id = 15662;
                    }
                    else if (roll < 50)                     // Frostbolt (25% chance)
                    {
                        spell_id = 11538;
                    }
                    else if (roll < 70)                     // Chain Lighting (20% chance)
                    {
                        spell_id = 21179;
                    }
                    else if (roll < 77)                     // Polymorph (10% chance, 7% to target)
                    {
                        spell_id = 14621;
                    }
                    else if (roll < 80)                     // Polymorph (10% chance, 3% to self, backfire)
                    {
                        spell_id = 14621;
                        newTarget = m_caster;
                    }
                    else if (roll < 95)                     // Enveloping Winds (15% chance)
                    {
                        spell_id = 25189;
                    }
                    else                                    // Summon Felhund minion (5% chance)
                    {
                        spell_id = 14642;
                        newTarget = m_caster;
                    }

                    m_caster->CastSpell(newTarget, spell_id, true, m_CastItem);
                    return;
                }
                case 15998:                                 // Capture Worg Pup
                case 19614:                                 // Despawn Caster
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_UNIT)
                    {
                        return;
                    }

                    Creature* creatureTarget = (Creature*)unitTarget;

                    creatureTarget->ForcedDespawn();
                    return;
                }
                case 16589:                                 // Noggenfogger Elixir
                {
                    if (m_caster->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    uint32 spell_id = 0;
                    switch (urand(1, 3))
                    {
                        case 1: spell_id = 16595; break;
                        case 2: spell_id = 16593; break;
                        default: spell_id = 16591; break;
                    }

                    m_caster->CastSpell(m_caster, spell_id, true, NULL);
                    return;
                }
                case 17009:                                 // Voodoo
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    uint32 spell_id = 0;
                    switch (urand(0, 6))
                    {
                        case 0: spell_id = 16707; break;    // Hex
                        case 1: spell_id = 16708; break;    // Hex
                        case 2: spell_id = 16709; break;    // Hex
                        case 3: spell_id = 16711; break;    // Grow
                        case 4: spell_id = 16712; break;    // Special Brew
                        case 5: spell_id = 16713; break;    // Ghostly
                        case 6: spell_id = 16716; break;    // Launch
                    }

                    m_caster->CastSpell(unitTarget, spell_id, true, NULL, NULL, m_originalCasterGUID, m_spellInfo);
                    return;
                }
                case 17251:                                 // Spirit Healer Res
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    Unit* caster = GetAffectiveCaster();

                    if (caster && caster->GetTypeId() == TYPEID_PLAYER)
                    {
                        WorldPacket data(SMSG_SPIRIT_HEALER_CONFIRM, 8);
                        data << unitTarget->GetObjectGuid();
                        ((Player*)caster)->GetSession()->SendPacket(&data);
                    }
                    return;
                }
                case 17271:                                 // Test Fetid Skull
                {
                    if (!itemTarget && m_caster->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    uint32 spell_id = urand(0, 1)
                        ? 17269               // Create Resonating Skull
                        : 17270;              // Create Bone Dust

                    m_caster->CastSpell(m_caster, spell_id, true, NULL);
                    return;
                }
                case 17770:                                 // Wolfshead Helm Energy
                {
                    m_caster->CastSpell(m_caster, 29940, true, NULL);
                    return;
                }
                case 17950:                                 // Shadow Portal
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    // Shadow Portal
                    const uint32 spell_list[6] = {17863, 17939, 17943, 17944, 17946, 17948};

                    m_caster->CastSpell(unitTarget, spell_list[urand(0, 5)], true);
                    return;
                }
                case 18269:                                 // Kodo Kombobulator
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    if (unitTarget->GetTypeId() == TYPEID_PLAYER)
                    {
                        return;
                    }

                    ((Creature*)unitTarget)->ForcedDespawn();
                    return;
                }
                case 18350:                                 // Dummy Trigger
                {
                    if (unitTarget->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    // Need remove self if Lightning Shield not active
                    Unit::SpellAuraHolderMap const& auras = unitTarget->GetSpellAuraHolderMap();
                    for (Unit::SpellAuraHolderMap::const_iterator itr = auras.begin(); itr != auras.end(); ++itr)
                    {
                        SpellEntry const* spell = itr->second->GetSpellProto();
                        if (spell->SpellClassSet == SPELLFAMILY_SHAMAN &&
                            (spell->SpellClassMask & UI64LIT(0x0000000000000400)))
                        {
                            return;
                        }
                    }
                    unitTarget->RemoveAurasDueToSpell(28820);
                    return;
                }
                case 19395:                                 // Gordunni Trap
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    unitTarget->CastSpell(unitTarget, urand(0, 1) ? 19394 : 11756, true);
                    return;
                }
                case 19411:                                 // Lava Bomb
                case 20474:                                 // Lava Bomb
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    unitTarget->CastSpell(unitTarget, 20494, true);
                    return;
                }
                case 20572:                                 // Blood Fury
                {
                    if (m_caster->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    m_caster->CastSpell(m_caster, 23230, true);

                    damage = uint32(damage * (m_caster->GetTotalAttackPowerValue(BASE_ATTACK)) / 100);
                    m_caster->CastCustomSpell(m_caster, 23234, &damage, NULL, NULL, true, NULL);
                    return;
                }
                case 19869:                                 // Dragon Orb
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER || unitTarget->HasAura(23958))
                    {
                        return;
                    }

                    unitTarget->CastSpell(unitTarget, 19832, true);
                    return;
                }
                case 20037:                                 // Explode Orb Effect
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    unitTarget->CastSpell(unitTarget, 20038, true);
                    return;
                }
                case 20577:                                 // Cannibalize
                {
                    if (unitTarget)
                    {
                        AddTriggeredSpell(20578);
                    }
                    return;
                }
                case 21147:                                 // Arcane Vacuum
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    // Spell used by Azuregos to teleport all the players to him
                    // This also resets the target threat
                    if (m_caster->GetThreatManager().getThreat(unitTarget))
                    {
                        m_caster->GetThreatManager().modifyThreatPercent(unitTarget, -100);
                    }

                    // cast summon player if not affected by Aura of Frost (23186)
                    if (!unitTarget->HasAura(23186))
                    {
                        m_caster->CastSpell(unitTarget, 21150, true);
                    }

                    return;
                }
                case 23019:                                 // Crystal Prison Dummy DND
                {
                    if (!unitTarget || !unitTarget->IsAlive() || unitTarget->GetTypeId() != TYPEID_UNIT || ((Creature*)unitTarget)->IsPet())
                    {
                        return;
                    }

                    Creature* creatureTarget = (Creature*)unitTarget;
                    if (creatureTarget->IsPet())
                    {
                        return;
                    }

                    creatureTarget->CastSpell(creatureTarget, 23022, true);
                    creatureTarget->ForcedDespawn();
                    return;
                }
                case 23074:                                 // Arcanite Dragonling
                {
                    if (!m_CastItem)
                    {
                        return;
                    }

                    m_caster->CastSpell(m_caster, 19804, true, m_CastItem);
                    return;
                }
                case 23075:                                 // Mithril Mechanical Dragonling
                {
                    if (!m_CastItem)
                    {
                        return;
                    }

                    m_caster->CastSpell(m_caster, 12749, true, m_CastItem);
                    return;
                }
                case 23076:                                 // Mechanical Dragonling
                {
                    if (!m_CastItem)
                    {
                        return;
                    }

                    m_caster->CastSpell(m_caster, 4073, true, m_CastItem);
                    return;
                }
                case 23133:                                 // Gnomish Battle Chicken
                {
                    if (!m_CastItem)
                    {
                        return;
                    }

                    m_caster->CastSpell(m_caster, 13166, true, m_CastItem);
                    return;
                }
                case 23138:                                 // Gate of Shazzrah
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    // Effect probably include a threat change, but it is unclear if fully
                    // reset or just forced upon target for teleport (SMSG_HIGHEST_THREAT_UPDATE)

                    // Gate of Shazzrah
                    m_caster->CastSpell(unitTarget, 23139, true);
                    return;
                }
                case 23448:                                 // Transporter Arrival - Ultrasafe Transporter: Gadgetzan - backfires
                {
                    int32 r = irand(0, 119);
                    if (r < 20)                             // Transporter Malfunction - 1/6 polymorph
                    {
                        m_caster->CastSpell(m_caster, 23444, true);
                    }
                    else if (r < 100)                       // Evil Twin               - 4/6 evil twin
                    {
                        m_caster->CastSpell(m_caster, 23445, true);
                    }
                    else                                    // Transporter Malfunction - 1/6 miss the target
                    {
                        m_caster->CastSpell(m_caster, 36902, true);
                    }

                    return;
                }
                case 23453:                                 // Gnomish Transporter - Ultrasafe Transporter: Gadgetzan
                {
                    if (roll_chance_i(50))                  // Gadgetzan Transporter         - success
                    {
                        m_caster->CastSpell(m_caster, 23441, true);
                    }
                    else                                    // Gadgetzan Transporter Failure - failure
                    {
                        m_caster->CastSpell(m_caster, 23446, true);
                    }

                    return;
                }
                case 23645:                                 // Hourglass Sand
                    m_caster->RemoveAurasDueToSpell(23170); // Brood Affliction: Bronze
                    return;
                case 23725:                                 // Gift of Life (warrior bwl trinket)
                {
                    int32 basepoints = m_caster->GetMaxHealth() * 0.15;
                    m_caster->CastCustomSpell(m_caster, 23782, &basepoints, NULL, NULL, true, NULL);
                    m_caster->CastCustomSpell(m_caster, 23783, &basepoints, NULL, NULL, true, NULL);
                    return;
                }
                case 24781:                                 // Dream Fog
                {
                    if (m_caster->GetTypeId() != TYPEID_UNIT || !unitTarget)
                    {
                        return;
                    }
                    // TODO Note: Should actually not only AttackStart, but fixate on the target
                    ((Creature*)m_caster)->AI()->AttackStart(unitTarget);
                    return;
                }
                case 24930:                                 // Hallow's End Treat
                {
                    uint32 spell_id = 0;

                    switch (urand(1, 4))
                    {
                        case 1: spell_id = 24924; break;    // Larger and Orange
                        case 2: spell_id = 24925; break;    // Skeleton
                        case 3: spell_id = 24926; break;    // Pirate
                        case 4: spell_id = 24927; break;    // Ghost
                    }

                    m_caster->CastSpell(m_caster, spell_id, true);
                    return;
                }
                case 25860:                                 // Reindeer Transformation
                {
                    if (!m_caster->HasAuraType(SPELL_AURA_MOUNTED))
                    {
                        return;
                    }

                    float speed = m_caster->GetSpeedRate(MOVE_RUN);

                    m_caster->RemoveSpellsCausingAura(SPELL_AURA_MOUNTED);

                    // 5 different spells used depending on mounted speed
                    if (speed >= 2.0f)
                    {
                        m_caster->CastSpell(m_caster, 25859, true);  // 100% ground Reindeer
                    }
                    else
                        // Reindeer
                    {
                        m_caster->CastSpell(m_caster, 25858, true);  // 60% ground Reindeer
                    }

                    return;
                }
                case 26074:                                 // Holiday Cheer
                    // implemented at client side
                    return;
                case 26626:                                 // Mana Burn Area
                {
                    if (unitTarget->GetTypeId() != TYPEID_UNIT || unitTarget->GetPowerType() != POWER_MANA)
                    {
                        return;
                    }

                    m_caster->CastSpell(unitTarget, 25779, true);
                    return;
                }
                case 28006:                                 // Arcane Cloaking
                {
                    if (unitTarget && unitTarget->GetTypeId() == TYPEID_PLAYER)
                        // Naxxramas Entry Flag Effect DND
                    {
                        m_caster->CastSpell(unitTarget, 29294, true);
                    }

                    return;
                }
                case 28098:                                 // Stalagg Tesla Effect
                case 28110:                                 // Feugen Tesla Effect
                {
                    if (unitTarget->GetTypeId() != TYPEID_UNIT)
                    {
                        return;
                    }

                    if (m_caster->getVictim() && !m_caster->IsWithinDistInMap(unitTarget, 60.0f))
                    {
                        // Cast Shock on nearby targets
                        if (Unit* pTarget = ((Creature*)m_caster)->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0))
                        {
                            unitTarget->CastSpell(pTarget, 28099, false);
                        }
                    }
                    else
                    {
                        // "Evade"
                        unitTarget->RemoveAurasDueToSpell(m_spellInfo->ID == 28098 ? 28097 : 28109);
                        unitTarget->DeleteThreatList();
                        unitTarget->CombatStop(true);
                        // Recast chain (Stalagg Chain or Feugen Chain
                        unitTarget->CastSpell(m_caster, m_spellInfo->ID == 28098 ? 28096 : 28111, false);
                    }
                    return;
                }
            }

            // All IconID Check in there
            switch (m_spellInfo->SpellIconID)
            {
                // Berserking (troll racial traits)
                case 1661:
                {
                    uint32 healthPerc = uint32((float(m_caster->GetHealth()) / m_caster->GetMaxHealth()) * 100);
                    int32 speed_mod = 10;
                    if (healthPerc <= 40)
                    {
                        speed_mod = 30;
                    }
                    if (healthPerc < 100 && healthPerc > 40)
                    {
                        speed_mod = 10 + (100 - healthPerc) / 3;
                    }

                    int32 hasteModBasePoints0 = speed_mod;
                    int32 hasteModBasePoints1 = speed_mod;
                    int32 hasteModBasePoints2 = speed_mod;

                    // FIXME: custom spell required this aura state by some unknown reason, we not need remove it anyway
                    m_caster->ModifyAuraState(AURA_STATE_BERSERKING, true);
                    m_caster->CastCustomSpell(m_caster, 26635, &hasteModBasePoints0, &hasteModBasePoints1, &hasteModBasePoints2, true, NULL);
                    return;
                }
            }
            break;
        }
        case SPELLFAMILY_MAGE:
        {
            switch (m_spellInfo->ID)
            {
                case 11189:                                 // Frost Warding
                case 28332:
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    // increase reflection chance (effect 1) of Frost Ward, removed in aura boosts
                    SpellModifier *mod = new SpellModifier(SPELLMOD_RESIST_MISS_CHANCE, SPELLMOD_FLAT, damage, m_spellInfo->ID, UI64LIT(0x0000000000000100));
                    ((Player*)unitTarget)->AddSpellMod(mod, true);
                    break;
                }
                case 12472:                                 // Cold Snap
                {
                    if (m_caster->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    // immediately finishes the cooldown on Frost spells
                    const SpellCooldowns& cm = ((Player*)m_caster)->GetSpellCooldownMap();
                    for (SpellCooldowns::const_iterator itr = cm.begin(); itr != cm.end();)
                    {
                        SpellEntry const* spellInfo = sSpellStore.LookupEntry(itr->first);

                        if (spellInfo->SpellClassSet == SPELLFAMILY_MAGE &&
                            (GetSpellSchoolMask(spellInfo) & SPELL_SCHOOL_MASK_FROST) &&
                            spellInfo->ID != m_spellInfo->ID && GetSpellRecoveryTime(spellInfo) > 0)
                        {
                            ((Player*)m_caster)->RemoveSpellCooldown((itr++)->first, true);
                        }
                        else
                        {
                            ++itr;
                        }
                    }
                    return;
                }
            }
            break;
        }
        case SPELLFAMILY_WARRIOR:
        {
            // Execute
            if (m_spellInfo->SpellClassMask & UI64LIT(0x20000000))
            {
                if (!unitTarget)
                {
                    return;
                }

                int32 basePoints0 = damage + int32(m_caster->GetPower(POWER_RAGE) * m_spellInfo->EffectChainAmplitude[eff_idx]);
                m_caster->CastCustomSpell(unitTarget, 20647, &basePoints0, NULL, NULL, true, 0);
                m_caster->SetPower(POWER_RAGE, 0);
                return;
            }
            // Warrior's Wrath
            if (m_spellInfo->ID == 21977)
            {
                if (!unitTarget)
                {
                    return;
                }

                m_caster->CastSpell(unitTarget, 21887, true); // spell mod
                return;
            }
            break;
        }
        case SPELLFAMILY_WARLOCK:
        {
            // Life Tap
            if (m_spellInfo->SpellClassMask & UI64LIT(0x0000000000040000))
            {
                float cost = m_currentBasePoints[EFFECT_INDEX_0];

                if (Player* modOwner = m_caster->GetSpellModOwner())
                {
                    modOwner->ApplySpellMod(m_spellInfo->ID, SPELLMOD_COST, cost, this);
                }

                int32 dmg = m_caster->SpellDamageBonusDone(m_caster, m_spellInfo, uint32(cost > 0 ? cost : 0), SPELL_DIRECT_DAMAGE);
                dmg = m_caster->SpellDamageBonusTaken(m_caster, m_spellInfo, dmg, SPELL_DIRECT_DAMAGE);

                if (int32(m_caster->GetHealth()) > dmg)
                {
                    // Shouldn't Appear in Combat Log
                    m_caster->ModifyHealth(-dmg);

                    int32 mana = dmg;

                    // Improved Life Tap mod
                    Unit::AuraList const& auraDummy = m_caster->GetAurasByType(SPELL_AURA_DUMMY);
                    for (Unit::AuraList::const_iterator itr = auraDummy.begin(); itr != auraDummy.end(); ++itr)
                    {
                        if ((*itr)->GetSpellProto()->SpellClassSet == SPELLFAMILY_WARLOCK && (*itr)->GetSpellProto()->SpellIconID == 208)
                        {
                            mana = ((*itr)->GetModifier()->m_amount + 100) * mana / 100;
                        }
                    }

                    m_caster->CastCustomSpell(m_caster, 31818, &mana, NULL, NULL, true);

                    // Mana Feed
                    int32 manaFeedVal = m_caster->CalculateSpellDamage(m_caster, m_spellInfo, EFFECT_INDEX_1);
                    manaFeedVal = manaFeedVal * mana / 100;
                    if (manaFeedVal > 0)
                    {
                        m_caster->CastCustomSpell(m_caster, 32553, &manaFeedVal, NULL, NULL, true, NULL);
                    }
                }
                else
                {
                    SendCastResult(SPELL_FAILED_FIZZLE);
                }

                return;
            }
            break;
        }
        case SPELLFAMILY_PRIEST:
        {
            switch (m_spellInfo->ID)
            {
                case 28598:                                 // Touch of Weakness triggered spell
                {
                    if (!unitTarget || !m_triggeredByAuraSpell)
                    {
                        return;
                    }

                    uint32 spellid = 0;
                    switch (m_triggeredByAuraSpell->ID)
                    {
                        case 2652:  spellid =  2943; break; // Rank 1
                        case 19261: spellid = 19249; break; // Rank 2
                        case 19262: spellid = 19251; break; // Rank 3
                        case 19264: spellid = 19252; break; // Rank 4
                        case 19265: spellid = 19253; break; // Rank 5
                        case 19266: spellid = 19254; break; // Rank 6
                        case 25461: spellid = 25460; break; // Rank 7
                        default:
                            sLog.outError("Spell::EffectDummy: Spell 28598 triggered by unhandeled spell %u", m_triggeredByAuraSpell->ID);
                            return;
                    }
                    m_caster->CastSpell(unitTarget, spellid, true, NULL);
                    return;
                }
            }
            break;
        }
        case SPELLFAMILY_DRUID:
        {
            // Loatheb Corrupted Mind triggered sub spells
            if (m_spellInfo->ID == 29201)
            {
                uint32 spellid = 0;
                switch (unitTarget->getClass())
                {
                    case CLASS_PALADIN: spellid = 29196; break;
                    case CLASS_PRIEST: spellid = 29185; break;
                    case CLASS_SHAMAN: spellid = 29198; break;
                    case CLASS_DRUID: spellid = 29194; break;
                    default: break;
                }
                if (spellid != 0)
                {
                    m_caster->CastSpell(unitTarget, spellid, true, NULL);
                }
            }
            break;
        }
        case SPELLFAMILY_ROGUE:
            break;
        case SPELLFAMILY_HUNTER:
        {
            // Steady Shot
            if (m_spellInfo->SpellClassMask & UI64LIT(0x100000000))
            {
                if (!unitTarget || !unitTarget->IsAlive())
                {
                    return;
                }

                bool found = false;

                // check dazed affect
                Unit::AuraList const& decSpeedList = unitTarget->GetAurasByType(SPELL_AURA_MOD_DECREASE_SPEED);
                for (Unit::AuraList::const_iterator iter = decSpeedList.begin(); iter != decSpeedList.end(); ++iter)
                {
                    if ((*iter)->GetSpellProto()->SpellIconID == 15 && (*iter)->GetSpellProto()->DispelType == 0)
                    {
                        found = true;
                        break;
                    }
                }

                if (found)
                {
                    m_damage += damage;
                }
                return;
            }

            switch (m_spellInfo->ID)
            {
                case 23989:                                 // Readiness talent
                {
                    if (m_caster->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    // immediately finishes the cooldown for hunter abilities
                    const SpellCooldowns& cm = ((Player*)m_caster)->GetSpellCooldownMap();
                    for (SpellCooldowns::const_iterator itr = cm.begin(); itr != cm.end();)
                    {
                        SpellEntry const* spellInfo = sSpellStore.LookupEntry(itr->first);

                        if (spellInfo->SpellClassSet == SPELLFAMILY_HUNTER && spellInfo->ID != 23989 && GetSpellRecoveryTime(spellInfo) > 0)
                        {
                            ((Player*)m_caster)->RemoveSpellCooldown((itr++)->first, true);
                        }
                        else
                        {
                            ++itr;
                        }
                    }
                    return;
                }
            }
            break;
        }
        case SPELLFAMILY_PALADIN:
        {
            switch (m_spellInfo->SpellIconID)
            {
                case 156:                                   // Holy Shock
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    int hurt = 0;
                    int heal = 0;

                    switch (m_spellInfo->ID)
                    {
                        case 20473: hurt = 25912; heal = 25914; break;
                        case 20929: hurt = 25911; heal = 25913; break;
                        case 20930: hurt = 25902; heal = 25903; break;
                        default:
                            sLog.outError("Spell::EffectDummy: Spell %u not handled in HS", m_spellInfo->ID);
                            return;
                    }

                    if (m_caster->IsFriendlyTo(unitTarget))
                    {
                        m_caster->CastSpell(unitTarget, heal, true);
                    }
                    else
                    {
                        m_caster->CastSpell(unitTarget, hurt, true);
                    }

                    return;
                }
                case 561:                                   // Judgement of command
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    uint32 spell_id = m_currentBasePoints[eff_idx];
                    SpellEntry const* spell_proto = sSpellStore.LookupEntry(spell_id);
                    if (!spell_proto)
                    {
                        return;
                    }

                    m_caster->CastSpell(unitTarget, spell_proto, true, NULL);
                    return;
                }
            }
            break;
        }
        case SPELLFAMILY_SHAMAN:
        {
            // Flametongue Weapon Proc, Ranks
            if (m_spellInfo->SpellClassMask & UI64LIT(0x0000000000200000))
            {
                if (!m_CastItem)
                {
                    sLog.outError("Spell::EffectDummy: spell %i requires cast Item", m_spellInfo->ID);
                    return;
                }
                // found spelldamage coefficients of 0.381% per 0.1 speed and 15.244 per 4.0 speed
                // but own calculation say 0.385 gives at most one point difference to published values
                int32 spellDamage = m_caster->SpellBaseDamageBonusDone(GetSpellSchoolMask(m_spellInfo));
                float weaponSpeed = (1.0f / IN_MILLISECONDS) * m_CastItem->GetProto()->Delay;
                int32 totalDamage = int32((damage + 3.85f * spellDamage) * 0.01 * weaponSpeed);

                m_caster->CastCustomSpell(unitTarget, 10444, &totalDamage, NULL, NULL, true, m_CastItem);
                return;
            }

            break;
        }
    }

    // pet auras
    if (PetAura const* petSpell = sSpellMgr.GetPetAura(m_spellInfo->ID))
    {
        m_caster->AddPetAura(petSpell);
        return;
    }

    // Script based implementation. Must be used only for not good for implementation in core spell effects
    // So called only for not processed cases
    bool libraryResult = false;
    if (gameObjTarget)
    {
        libraryResult = sScriptMgr.OnEffectDummy(m_caster, m_spellInfo->ID, eff_idx, gameObjTarget, m_originalCasterGUID);
    }
    else if (unitTarget && (unitTarget->GetTypeId() == TYPEID_UNIT || unitTarget->GetTypeId() == TYPEID_PLAYER))
    {
        libraryResult = sScriptMgr.OnEffectDummy(m_caster, m_spellInfo->ID, eff_idx, unitTarget, m_originalCasterGUID);
    }
    else if (itemTarget)
    {
        libraryResult = sScriptMgr.OnEffectDummy(m_caster, m_spellInfo->ID, eff_idx, itemTarget, m_originalCasterGUID);
    }

    if (libraryResult || !unitTarget)
    {
        return;
    }

    // Previous effect might have started script
    if (!ScriptMgr::CanSpellEffectStartDBScript(m_spellInfo, eff_idx))
    {
        return;
    }

    DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "Spell ScriptStart spellid %u in EffectDummy", m_spellInfo->ID);
    m_caster->GetMap()->ScriptsStart(DBS_ON_SPELL, m_spellInfo->ID, m_caster, unitTarget);
}
