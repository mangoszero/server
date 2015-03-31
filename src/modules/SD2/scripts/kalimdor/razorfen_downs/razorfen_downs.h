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

#ifndef DEF_RFD_H
#define DEF_RFD_H

enum
{
    MAX_ENCOUNTER   = 0,
    TYPE_GONG_USED  = MAX_ENCOUNTER,
    //TYPE_PLAYER     = MAX_ENCOUNTER + 1,

    GO_GONG = 148917,
    NPC_TOMB_FIEND = 7349,
    TOTAL_FIENDS = 8,
    NPC_TOMB_REAVER = 7351,
    TOTAL_REAVERS = 4,
    NPC_TUTENKASH = 7355,
};

struct TUTENKASH_CreatureLocation
{
    float fX, fY, fZ, fO;
};

static const TUTENKASH_CreatureLocation aCreatureLocation[] =
{
    { 2540.479f, 906.539f, 46.663f, 5.47f },               // Tomb Fiend/Reaver spawn point
    { 2541.511f, 912.857f, 46.216f, 5.39f },               // Tomb Fiend/Reaver spawn point
    { 2536.703f, 917.214f, 46.094f, 5.57f },               // Tomb Fiend/Reaver spawn point
    { 2530.443f, 913.598f, 46.083f, 5.69f },               // Tomb Fiend/Reaver spawn point
    { 2529.833f, 920.977f, 45.836f, 5.47f },               // Tomb Fiend spawn point
    { 2524.738f, 915.195f, 46.248f, 5.97f },               // Tomb Fiend spawn point
    { 2517.829f, 917.746f, 46.073f, 5.83f },               // Tomb Fiend spawn point
    { 2512.750f, 924.458f, 46.504f, 5.92f }                // Tomb Fiend spawn point
};

static const TUTENKASH_CreatureLocation aTutenkashLocation[] =
{
    { 2493.968f, 790.974f, 39.849f, 5.92f }                // Tuten'kash spawn point
};

#endif
