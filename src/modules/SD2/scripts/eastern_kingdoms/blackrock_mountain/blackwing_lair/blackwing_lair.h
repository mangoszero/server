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

#ifndef DEF_BLACKWING_LAIR
#define DEF_BLACKWING_LAIR

enum
{
    MAX_ENCOUNTER               = 8,

    TYPE_RAZORGORE              = 0,
    TYPE_VAELASTRASZ            = 1,
    TYPE_LASHLAYER              = 2,
    TYPE_FIREMAW                = 3,
    TYPE_EBONROC                = 4,
    TYPE_FLAMEGOR               = 5,
    TYPE_CHROMAGGUS             = 6,
    TYPE_NEFARIAN               = 7,

    DATA_DRAGON_EGG             = 1,                        // track the used eggs

    NPC_RAZORGORE               = 12435,
    NPC_VAELASTRASZ             = 13020,
    NPC_LASHLAYER               = 12017,
    NPC_FIREMAW                 = 11983,
    NPC_EBONROC                 = 14601,
    NPC_FLAMEGOR                = 11981,
    NPC_CHROMAGGUS              = 14020,
    NPC_NEFARIAN                = 11583,
    NPC_LORD_VICTOR_NEFARIUS    = 10162,
    NPC_BLACKWING_TECHNICIAN    = 13996,                    // Flees at Vael intro event

    // Razorgore event related
    NPC_GRETHOK_CONTROLLER      = 12557,
    NPC_BLACKWING_ORB_TRIGGER   = 14449,
    NPC_NEFARIANS_TROOPS        = 14459,
    NPC_MONSTER_GENERATOR       = 12434,
    NPC_BLACKWING_LEGIONNAIRE   = 12416,                    // one spawn per turn
    NPC_BLACKWING_MAGE          = 12420,                    // one spawn per turn
    NPC_DRAGONSPAWN             = 12422,                    // two spawns per turn

    GO_DOOR_RAZORGORE_ENTER     = 176964,
    GO_DOOR_RAZORGORE_EXIT      = 176965,
    GO_DOOR_NEFARIAN            = 176966,
    // GO_DOOR_CHROMAGGUS_ENTER  = 179115,
    // GO_DOOR_CHROMAGGUS_SIDE   = 179116,
    GO_DOOR_CHROMAGGUS_EXIT     = 179117,
    GO_DOOR_VAELASTRASZ         = 179364,
    GO_DOOR_LASHLAYER           = 179365,
    GO_ORB_OF_DOMINATION        = 177808,                   // trigger 19832 on Razorgore
    GO_BLACK_DRAGON_EGG         = 177807,
    GO_DRAKONID_BONES           = 179804,

    EMOTE_ORB_SHUT_OFF          = -1469035,
    EMOTE_TROOPS_FLEE           = -1469033,                 // emote by Nefarian's Troops npc

    MAX_EGGS_DEFENDERS          = 4,
};

// Coords used to spawn Nefarius at the throne
static const float aNefariusSpawnLoc[4] = { -7466.16f, -1040.80f, 412.053f, 2.14675f};

#endif
