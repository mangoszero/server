/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2020 MaNGOS <https://getmangos.eu>
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

#ifndef DBCENUMS_H
#define DBCENUMS_H

// Client expected level limitation, like as used in DBC item max levels for "until max player level"
// use as default max player level, must be fit max level for used client
// also see MAX_LEVEL and STRONG_MAX_LEVEL define
#define DEFAULT_MAX_LEVEL 60

// client supported max level for player/pets/etc. Avoid overflow or client stability affected.
// also see GT_MAX_LEVEL define
#define MAX_LEVEL    100

// Server side limitation. Base at used code requirements.
// also see MAX_LEVEL and GT_MAX_LEVEL define
#define STRONG_MAX_LEVEL 255

enum AreaTeams
{
    AREATEAM_NONE  = 0,
    AREATEAM_ALLY  = 2,
    AREATEAM_HORDE = 4
};

enum AreaFlags
{
    AREA_FLAG_SNOW                  = 0x00000001,           // snow (only Dun Morogh, Naxxramas, Razorfen Downs and Winterspring)
    AREA_FLAG_UNK1                  = 0x00000002,           // unknown, (only Naxxramas and Razorfen Downs)
    AREA_FLAG_UNK2                  = 0x00000004,           // Only used on development map
    AREA_FLAG_SLAVE_CAPITAL         = 0x00000008,           // slave capital city flag?
    AREA_FLAG_UNK3                  = 0x00000010,           // unknown
    AREA_FLAG_SLAVE_CAPITAL2        = 0x00000020,           // slave capital city flag?
    AREA_FLAG_DUEL                  = 0x00000040,           // zones where duels allowed
    AREA_FLAG_ARENA                 = 0x00000080,           // arena, both instanced and world arenas
    AREA_FLAG_CAPITAL               = 0x00000100,           // main capital city flag
    AREA_FLAG_CITY                  = 0x00000200,           // only for one zone named "City" (where it located?)
};

enum FactionTemplateFlags
{
    FACTION_TEMPLATE_FLAG_PVP               = 0x00000800,   // flagged for PvP
    FACTION_TEMPLATE_FLAG_CONTESTED_GUARD   = 0x00001000,   // faction will attack players that were involved in PvP combats
};

enum FactionMasks
{
    FACTION_MASK_PLAYER   = 1,                              // any player
    FACTION_MASK_ALLIANCE = 2,                              // player or creature from alliance team
    FACTION_MASK_HORDE    = 4,                              // player or creature from horde team
    FACTION_MASK_MONSTER  = 8                               // aggressive creature from monster team
    // if none flags set then non-aggressive creature
};

enum MapTypes                                               // Lua_IsInInstance
{
    MAP_COMMON          = 0,                                // none
    MAP_INSTANCE        = 1,                                // party
    MAP_RAID            = 2,                                // raid
    MAP_BATTLEGROUND    = 3,                                // pvp
};

enum AbilytyLearnType
{
    ABILITY_LEARNED_ON_GET_PROFESSION_SKILL     = 1,
    ABILITY_LEARNED_ON_GET_RACE_OR_CLASS_SKILL  = 2
};

enum AbilitySkillFlags
{
    ABILITY_SKILL_NONTRAINABLE = 0x100
};

enum ItemEnchantmentType
{
    ITEM_ENCHANTMENT_TYPE_NONE             = 0,
    ITEM_ENCHANTMENT_TYPE_COMBAT_SPELL     = 1,
    ITEM_ENCHANTMENT_TYPE_DAMAGE           = 2,
    ITEM_ENCHANTMENT_TYPE_EQUIP_SPELL      = 3,
    ITEM_ENCHANTMENT_TYPE_RESISTANCE       = 4,
    ITEM_ENCHANTMENT_TYPE_STAT             = 5,
    ITEM_ENCHANTMENT_TYPE_TOTEM            = 6
};

/**
 * See \ref SpellEntry::Targets member for some use of these flags
 * \todo Properly document this
 */
enum SpellCastTargetFlags
{
    TARGET_FLAG_SELF            = 0x00000000,
    TARGET_FLAG_UNUSED1         = 0x00000001,       ///< not used in any spells (can be set dynamically)
    TARGET_FLAG_UNIT            = 0x00000002,       ///< pguid
    TARGET_FLAG_UNUSED2         = 0x00000004,       ///< not used in any spells (can be set dynamically)
    TARGET_FLAG_UNUSED3         = 0x00000008,       ///< not used in any spells (can be set dynamically)
    TARGET_FLAG_ITEM            = 0x00000010,       ///< pguid
    TARGET_FLAG_SOURCE_LOCATION = 0x00000020,       ///< 3 float
    TARGET_FLAG_DEST_LOCATION   = 0x00000040,       ///< 3 float
    TARGET_FLAG_OBJECT_UNK      = 0x00000080,       ///< used in 7 spells only
    TARGET_FLAG_UNIT_UNK        = 0x00000100,       ///< looks like self target (389 spells)
    TARGET_FLAG_PVP_CORPSE      = 0x00000200,       ///< pguid
    TARGET_FLAG_UNIT_CORPSE     = 0x00000400,       ///< 10 spells (gathering professions)
    TARGET_FLAG_OBJECT          = 0x00000800,       ///< pguid, 0 spells
    TARGET_FLAG_TRADE_ITEM      = 0x00001000,       ///< pguid, 0 spells
    TARGET_FLAG_STRING          = 0x00002000,       ///< string, 0 spells
    TARGET_FLAG_GAMEOBJECT_ITEM = 0x00004000,       ///< 199 spells, opening object/lock
    TARGET_FLAG_CORPSE          = 0x00008000,       ///< pguid, resurrection spells
    TARGET_FLAG_UNK2            = 0x00010000,       ///< pguid, not used in any spells (can be set dynamically)
};

/**
 * Used in \ref Unit::HasAura for example, also used in \ref Spell::m_currentBasePoints. These
 * reference the three effects a \ref Spell can have which do different things, for instance it
 * can be applying a \ref Aura. For a reference see QSW which you can get at
 * https://bitbucket.org/sidsukana/qsw
 */
enum SpellEffectIndex
{
    EFFECT_INDEX_0     = 0, ///< The first spell effect
    EFFECT_INDEX_1     = 1, ///< The second spell effect
    EFFECT_INDEX_2     = 2  ///< The third spell effect
};

#define MAX_EFFECT_INDEX 3

enum SpellFamily
{
    SPELLFAMILY_GENERIC     = 0,
    SPELLFAMILY_ENVIRONMENT = 1,                            // events, holidays, mostly aura related spells
    // 2 - unused
    SPELLFAMILY_MAGE        = 3,
    SPELLFAMILY_WARRIOR     = 4,
    SPELLFAMILY_WARLOCK     = 5,
    SPELLFAMILY_PRIEST      = 6,
    SPELLFAMILY_DRUID       = 7,
    SPELLFAMILY_ROGUE       = 8,
    SPELLFAMILY_HUNTER      = 9,
    SPELLFAMILY_PALADIN     = 10,
    SPELLFAMILY_SHAMAN      = 11,
    // 12 - unused
    SPELLFAMILY_POTION      = 13,
};

#endif
