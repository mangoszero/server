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



#include "Creature.h"
#include "World.h"
#include "ObjectMgr.h"
#include "LivingWorldAnchorPolicy.h"
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "ScriptMgr.h"
#include "ObjectGuid.h"
#include "SQLStorages.h"
#include "SpellMgr.h"
#include "GossipDef.h"
#include "Player.h"
#include "GameEventMgr.h"
#include "PoolManager.h"
#include "Opcodes.h"
#include "Log.h"
#include "LootMgr.h"
#include "MapManager.h"
#include "CreatureAI.h"
#include "CreatureAISelector.h"
#include "InstanceData.h"
#include "MapPersistentStateMgr.h"
#include "BattleGround/BattleGroundMgr.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "Spell.h"
#include "Util.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "movement/MoveSplineInit.h"
#include "CreatureLinkingMgr.h"
#include "DisableMgr.h"
#include "MovementGenerator.h"
#include "Policies/Singleton.h"
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */

/**
 * @brief Selects the creature level and recalculates level-dependent stats.
 *
 * @param forcedLevel Optional forced level override.
 */
void Creature::SelectLevel(uint32 forcedLevel /*= USE_DEFAULT_DATABASE_LEVEL*/)
{
    CreatureInfo const* cinfo = GetCreatureInfo();
    if (!cinfo)
    {
        return;
    }

    uint32 rank = IsPet() ? 0 : cinfo->Rank;                // TODO :: IsPet probably not needed here

    // level
    uint32 level = forcedLevel;
    uint32 const minlevel = cinfo->MinLevel;
    uint32 const maxlevel = cinfo->MaxLevel;

    if (level == USE_DEFAULT_DATABASE_LEVEL)
    {
        level = minlevel == maxlevel ? minlevel : urand(minlevel, maxlevel);
    }

    SetLevel(level);

    //////////////////////////////////////////////////////////////////////////
    // Calculate level dependent stats
    //////////////////////////////////////////////////////////////////////////

    uint32 health;
    uint32 mana;

    // TODO: Remove cinfo->ArmorMultiplier test workaround to disable classlevelstats when DB is ready
    CreatureClassLvlStats const* cCLS = sObjectMgr.GetCreatureClassLvlStats(level, cinfo->UnitClass);
    if (cinfo->ArmorMultiplier > 0 && cCLS)
    {
        // Use Creature Stats to calculate stat values

        // health
        health = cCLS->BaseHealth * cinfo->HealthMultiplier;

        // mana
        mana = cCLS->BaseMana * cinfo->PowerMultiplier;
    }
    else
    {
        if (forcedLevel == USE_DEFAULT_DATABASE_LEVEL || (forcedLevel >= minlevel && forcedLevel <= maxlevel))
        {
            // Use old style to calculate stat values
            float rellevel = maxlevel == minlevel ? 0 : (float(level - minlevel)) / (maxlevel - minlevel);

            // health
            uint32 minhealth = std::min(cinfo->MaxLevelHealth, cinfo->MinLevelHealth);
            uint32 maxhealth = std::max(cinfo->MaxLevelHealth, cinfo->MinLevelHealth);
            health = uint32(minhealth + uint32(rellevel * (maxhealth - minhealth)));

            // mana
            uint32 minmana = std::min(cinfo->MaxLevelMana, cinfo->MinLevelMana);
            uint32 maxmana = std::max(cinfo->MaxLevelMana, cinfo->MinLevelMana);
            mana = minmana + uint32(rellevel * (maxmana - minmana));
        }
        else
        {
            sLog.outError("Creature::SelectLevel> Error trying to set level(%u) for creature %s without enough data to do it!", level, GetGuidStr().c_str());
            // probably wrong
            health = (cinfo->MaxLevelHealth / cinfo->MaxLevel) * level;
            mana = (cinfo->MaxLevelMana / cinfo->MaxLevel) * level;
        }
    }

    health *= _GetHealthMod(rank); // Apply custom config setting
    if (health < 1)
    {
        health = 1;
    }

    //////////////////////////////////////////////////////////////////////////
    // Set values
    //////////////////////////////////////////////////////////////////////////

    // health
    SetCreateHealth(health);
    SetMaxHealth(health);
    SetHealth(health);

    SetModifierValue(UNIT_MOD_HEALTH, BASE_VALUE, float(health));

    // all power types
    for (int i = POWER_MANA; i <= POWER_HAPPINESS; ++i)
    {
        uint32 maxValue;

        switch (i)
        {
            case POWER_MANA:        maxValue = mana; break;
            case POWER_RAGE:        maxValue = 0; break;
            case POWER_FOCUS:       maxValue = POWER_FOCUS_DEFAULT; break;
            case POWER_ENERGY:      maxValue = POWER_ENERGY_DEFAULT * cinfo->PowerMultiplier; break;
            case POWER_HAPPINESS:   maxValue = POWER_HAPPINESS_DEFAULT; break;
        }

        uint32 value = maxValue;

        // For non regenerating powers set 0
        if ((i == POWER_ENERGY || i == POWER_MANA) && !IsRegeneratingPower())
        {
            value = 0;
        }

        // Mana requires an extra field to be set
        if (i == POWER_MANA)
        {
            SetCreateMana(value);
        }

        SetMaxPower(Powers(i), maxValue);
        SetPower(Powers(i), value);
        SetModifierValue(UnitMods(UNIT_MOD_POWER_START + i), BASE_VALUE, float(value));
    }

    // damage
    float damagemod = _GetDamageMod(rank);

    SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, cinfo->MinMeleeDmg * damagemod);
    SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, cinfo->MaxMeleeDmg * damagemod);

    SetBaseWeaponDamage(OFF_ATTACK, MINDAMAGE, cinfo->MinMeleeDmg * damagemod);
    SetBaseWeaponDamage(OFF_ATTACK, MAXDAMAGE, cinfo->MaxMeleeDmg * damagemod);

    SetFloatValue(UNIT_FIELD_MINRANGEDDAMAGE, cinfo->MinRangedDmg * damagemod);
    SetFloatValue(UNIT_FIELD_MAXRANGEDDAMAGE, cinfo->MaxRangedDmg * damagemod);

    SetModifierValue(UNIT_MOD_ATTACK_POWER, BASE_VALUE, cinfo->MeleeAttackPower * damagemod);
}

/**
 * @brief Gets the configured health multiplier for a creature rank.
 *
 * @param Rank The creature elite rank.
 * @return The configured health rate multiplier.
 */
float Creature::_GetHealthMod(int32 Rank)
{
    switch (Rank)                                           // define rates for each elite rank
    {
        case CREATURE_ELITE_NORMAL:
            return sWorld.getConfig(CONFIG_FLOAT_RATE_CREATURE_NORMAL_HP);
        case CREATURE_ELITE_ELITE:
            return sWorld.getConfig(CONFIG_FLOAT_RATE_CREATURE_ELITE_ELITE_HP);
        case CREATURE_ELITE_RAREELITE:
            return sWorld.getConfig(CONFIG_FLOAT_RATE_CREATURE_ELITE_RAREELITE_HP);
        case CREATURE_ELITE_WORLDBOSS:
            return sWorld.getConfig(CONFIG_FLOAT_RATE_CREATURE_ELITE_WORLDBOSS_HP);
        case CREATURE_ELITE_RARE:
            return sWorld.getConfig(CONFIG_FLOAT_RATE_CREATURE_ELITE_RARE_HP);
        default:
            return sWorld.getConfig(CONFIG_FLOAT_RATE_CREATURE_ELITE_ELITE_HP);
    }
}

/**
 * @brief Gets the configured damage multiplier for a creature rank.
 *
 * @param Rank The creature elite rank.
 * @return The configured damage rate multiplier.
 */
float Creature::_GetDamageMod(int32 Rank)
{
    switch (Rank)                                           // define rates for each elite rank
    {
        case CREATURE_ELITE_NORMAL:
            return sWorld.getConfig(CONFIG_FLOAT_RATE_CREATURE_NORMAL_DAMAGE);
        case CREATURE_ELITE_ELITE:
            return sWorld.getConfig(CONFIG_FLOAT_RATE_CREATURE_ELITE_ELITE_DAMAGE);
        case CREATURE_ELITE_RAREELITE:
            return sWorld.getConfig(CONFIG_FLOAT_RATE_CREATURE_ELITE_RAREELITE_DAMAGE);
        case CREATURE_ELITE_WORLDBOSS:
            return sWorld.getConfig(CONFIG_FLOAT_RATE_CREATURE_ELITE_WORLDBOSS_DAMAGE);
        case CREATURE_ELITE_RARE:
            return sWorld.getConfig(CONFIG_FLOAT_RATE_CREATURE_ELITE_RARE_DAMAGE);
        default:
            return sWorld.getConfig(CONFIG_FLOAT_RATE_CREATURE_ELITE_ELITE_DAMAGE);
    }
}

/**
 * @brief Gets the spell damage modifier for a creature elite rank.
 *
 * @param Rank The creature elite rank.
 * @return The configured spell damage multiplier.
 */
float Creature::_GetSpellDamageMod(int32 Rank)
{
    switch (Rank)                                           // define rates for each elite rank
    {
        case CREATURE_ELITE_NORMAL:
            return sWorld.getConfig(CONFIG_FLOAT_RATE_CREATURE_NORMAL_SPELLDAMAGE);
        case CREATURE_ELITE_ELITE:
            return sWorld.getConfig(CONFIG_FLOAT_RATE_CREATURE_ELITE_ELITE_SPELLDAMAGE);
        case CREATURE_ELITE_RAREELITE:
            return sWorld.getConfig(CONFIG_FLOAT_RATE_CREATURE_ELITE_RAREELITE_SPELLDAMAGE);
        case CREATURE_ELITE_WORLDBOSS:
            return sWorld.getConfig(CONFIG_FLOAT_RATE_CREATURE_ELITE_WORLDBOSS_SPELLDAMAGE);
        case CREATURE_ELITE_RARE:
            return sWorld.getConfig(CONFIG_FLOAT_RATE_CREATURE_ELITE_RARE_SPELLDAMAGE);
        default:
            return sWorld.getConfig(CONFIG_FLOAT_RATE_CREATURE_ELITE_ELITE_SPELLDAMAGE);
    }
}
