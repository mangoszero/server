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

#ifndef DEF_BFD_H
#define DEF_BFD_H

enum
{
    MAX_ENCOUNTER               = 3,
    MAX_FIRES                   = 4,
    MAX_COUNT_POS               = 3,

    TYPE_KELRIS                 = 1,
    TYPE_SHRINE                 = 2,
    TYPE_STONE                  = 3,

    NPC_KELRIS                  = 4832,
    NPC_BARON_AQUANIS           = 12876,

    // Shrine event
    NPC_AKUMAI_SERVANT          = 4978,
    NPC_AKUMAI_SNAPJAW          = 4825,
    NPC_BARBED_CRUSTACEAN       = 4823,
    NPC_MURKSHALLOW_SOFTSHELL   = 4977,

    GO_PORTAL_DOOR              = 21117,
    GO_SHRINE_1                 = 21118,
    GO_SHRINE_2                 = 21119,
    GO_SHRINE_3                 = 21120,
    GO_SHRINE_4                 = 21121,
};

/* This is the spawn pattern for the event mobs
*     D
* 0        3
* 1   S    4
* 2        5
*     E

* This event spawns 4 sets of mobs
* The order in whitch the fires are lit doesn't matter

* First:    3 Snapjaws:     Positions 0, 1, 5
* Second:   2 Servants:     Positions 1, 4
* Third:    4 Crabs:        Positions 0, 2, 3, 4
* Fourth:  10 Murkshallows: Positions 2*0, 1, 2*2; 3, 2*4, 2*5

* On wipe the mobs don't despawn; they stay there until player returns
*/

#endif
