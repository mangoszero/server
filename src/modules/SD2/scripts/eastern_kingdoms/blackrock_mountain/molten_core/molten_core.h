/**
 * ScriptDev2 is an extension for mangos providing enhanced features for
 * area triggers, creatures, game objects, instances, items, and spells beyond
 * the default database scripting in mangos.
 *
 * Copyright (C) 2006-2013  ScriptDev2 <http://www.scriptdev2.com/>
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

#ifndef DEF_MOLTEN_CORE_H
#define DEF_MOLTEN_CORE_H

enum
{
    MAX_ENCOUNTER               = 10,

    TYPE_LUCIFRON               = 0,
    TYPE_MAGMADAR               = 1,    // do not change order: begin
    TYPE_GEHENNAS               = 2,
    TYPE_GARR                   = 3,
    TYPE_SHAZZRAH               = 4,
    TYPE_GEDDON                 = 5,
    TYPE_GOLEMAGG               = 6,
    TYPE_SULFURON               = 7,    // do not change order: end
    TYPE_MAJORDOMO              = 8, 
    TYPE_RAGNAROS               = 9,
    TYPE_FLAME_DOSED            = MAX_ENCOUNTER,
    TYPE_DO_FREE_GARR_ADDS      = MAX_ENCOUNTER+1,

    MAX_MOLTEN_RUNES            = 7,

    TYPE_RUNE_KRESS             = 0,
    TYPE_RUNE_MOHN              = 1,
    TYPE_RUNE_BLAZ              = 2,
    TYPE_RUNE_MAZJ              = 3,
    TYPE_RUNE_ZETH              = 4,
    TYPE_RUNE_THERI             = 5,
    TYPE_RUNE_KORO              = 6,

    NPC_LUCIFRON                = 12118,
    NPC_MAGMADAR                = 11982,
    NPC_GEHENNAS                = 12259,
    NPC_GARR                    = 12057,
    NPC_SHAZZRAH                = 12264,
    NPC_GEDDON                  = 12056,
    NPC_GOLEMAGG                = 11988,
    NPC_SULFURON                = 12098,
    NPC_MAJORDOMO               = 12018,
    NPC_RAGNAROS                = 11502,

    // Adds
    // Used for respawn in case of wipe
    NPC_FLAMEWAKER_PROTECTOR    = 12119,                    // Lucifron
    NPC_FLAMEWAKER              = 11661,                    // Gehennas
    NPC_FIRESWORN               = 12099,                    // Garr
    NPC_CORE_RAGER              = 11672,                    // Golemagg
    NPC_FLAMEWAKER_PRIEST       = 11662,                    // Sulfuron
    NPC_FLAMEWAKER_HEALER       = 11663,                    // Majordomo
    NPC_FLAMEWAKER_ELITE        = 11664,                    // Majordomo
    NPC_LAVA_SURGER             = 12101,

    GO_LAVA_STEAM               = 178107,
    GO_LAVA_SPLASH              = 178108,
    GO_CACHE_OF_THE_FIRE_LORD   = 179703,
    GO_RUNE_KRESS               = 176956,                   // Magmadar
    GO_RUNE_MOHN                = 176957,                   // Gehennas
    GO_RUNE_BLAZ                = 176955,                   // Garr
    GO_RUNE_MAZJ                = 176953,                   // Shazzah
    GO_RUNE_ZETH                = 176952,                   // Geddon
    GO_RUNE_THERI               = 176954,                   // Golemagg
    GO_RUNE_KORO                = 176951,                   // Sulfuron

    GO_RUNE_KRESS_TRAP          = 178192,
    GO_RUNE_MOHN_TRAP           = 178193,
    GO_RUNE_BLAZ_TRAP           = 178191,
    GO_RUNE_MAZJ_TRAP           = 178189,
    GO_RUNE_ZETH_TRAP           = 178188,
    GO_RUNE_THERI_TRAP          = 178190,
    GO_RUNE_KORO_TRAP           = 178187,

    MAX_MAJORDOMO_ADDS          = 8,
    FACTION_MAJORDOMO_FRIENDLY  = 1080,
    SAY_MAJORDOMO_SPAWN         = -1409004,

    // Garr encounter spells
    SPELL_GARR_ENRAGE           = 19516,
    SPELL_GARR_ARMOR_DEBUFF     = 20481,
    SPELL_SEPARATION_ANXIETY    = 23492,    // selfcast, for 5 sec: dmg +300% and banish immunity
};

struct sSpawnLocation
{
    uint32 m_uiEntry;
    float m_fX, m_fY, m_fZ, m_fO;
};

static sSpawnLocation m_aMajordomoLocations[2] =
{
    {NPC_MAJORDOMO, 758.089f, -1176.71f, -118.640f, 3.12414f},  // Summon fight position
    {NPC_MAJORDOMO, 847.103f, -816.153f, -229.775f, 4.344f} // Summon and teleport location (near Ragnaros)
};

struct sRuneEncounters
{
    uint32 m_uiRuneEntry, m_uiTrapEntry;
    uint8 m_bossType;
    uint8 getRuneType() const { return m_bossType - 1; }
};

static const sRuneEncounters m_aMoltenCoreRunes[MAX_MOLTEN_RUNES] =
{
    { GO_RUNE_KRESS, GO_RUNE_KRESS_TRAP, TYPE_MAGMADAR },
    { GO_RUNE_MOHN,  GO_RUNE_MOHN_TRAP,  TYPE_GEHENNAS },
    { GO_RUNE_BLAZ,  GO_RUNE_BLAZ_TRAP,  TYPE_GARR },
    { GO_RUNE_MAZJ,  GO_RUNE_MAZJ_TRAP,  TYPE_SHAZZRAH },
    { GO_RUNE_ZETH,  GO_RUNE_ZETH_TRAP,  TYPE_GEDDON },
    { GO_RUNE_THERI, GO_RUNE_THERI_TRAP, TYPE_GOLEMAGG },
    { GO_RUNE_KORO,  GO_RUNE_KORO_TRAP,  TYPE_SULFURON }
};
#endif

