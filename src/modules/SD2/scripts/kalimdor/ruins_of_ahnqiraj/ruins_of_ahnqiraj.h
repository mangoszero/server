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

#ifndef DEF_RUINS_OF_AHNQIRAJ_H
#define DEF_RUINS_OF_AHNQIRAJ_H

enum
{
    MAX_ENCOUNTER               = 6,
    MAX_HELPERS                 = 5,
    TYPE_SIGNAL                 = MAX_ENCOUNTER,

    TYPE_KURINNAXX              = 0,
    TYPE_RAJAXX                 = 1,
    TYPE_MOAM                   = 2,
    TYPE_BURU                   = 3,
    TYPE_AYAMISS                = 4,
    TYPE_OSSIRIAN               = 5,

    NPC_KURINNAXX               = 15348,
    NPC_MOAM                    = 15340,
    NPC_BURU                    = 15370,
    NPC_AYAMISS                 = 15369,
    NPC_OSSIRIAN                = 15339,
    NPC_GENERAL_ANDOROV         = 15471,                    // The general and the kaldorei are escorted for the rajaxx encounter
    NPC_KALDOREI_ELITE          = 15473,
    NPC_RAJAXX                  = 15341,                    // All of the following are used in the rajaxx encounter
    NPC_COLONEL_ZERRAN          = 15385,
    NPC_MAJOR_PAKKON            = 15388,
    NPC_MAJOR_YEGGETH           = 15386,
    NPC_CAPTAIN_XURREM          = 15390,
    NPC_CAPTAIN_DRENN           = 15389,
    NPC_CAPTAIN_TUUBID          = 15392,
    NPC_CAPTAIN_QEEZ            = 15391,
    NPC_QIRAJI_WARRIOR          = 15387,
    NPC_SWARMGUARD_NEEDLER      = 15344,

    MAX_ARMY_WAVES              = 7,

    GO_OSSIRIAN_CRYSTAL         = 180619,                   // Used in the ossirian encounter
    NPC_OSSIRIAN_TRIGGER        = 15590,                    // Triggers ossirian weakness

    SAY_OSSIRIAN_INTRO          = -1509022,                 // Yelled after Kurinnax dies

    // Rajaxx yells
    SAY_WAVE3                   = -1509005,
    SAY_WAVE4                   = -1509006,
    SAY_WAVE5                   = -1509007,
    SAY_WAVE6                   = -1509008,
    SAY_WAVE7                   = -1509009,
    SAY_INTRO                   = -1509010,
    SAY_DEAGGRO                 = -1509015,                 // on Rajaxx evade
    SAY_COMPLETE_QUEST          = -1509017,                 // Yell when realm complete quest 8743 for world event
};

struct SpawnLocation
{
    uint32 m_uiEntry;
    float m_fX, m_fY, m_fZ, m_fO;
};

// Movement locations for Andorov
static const SpawnLocation aAndorovMoveLocs[] =
{
    {0, -8701.51f, 1561.80f, 32.092f},
    {0, -8718.66f, 1577.69f, 21.612f},
    {0, -8876.97f, 1651.96f, 21.57f, 5.52f},
    {0, -8882.15f, 1602.77f, 21.386f},
    {0, -8940.45f, 1550.69f, 21.616f},
};

#endif
