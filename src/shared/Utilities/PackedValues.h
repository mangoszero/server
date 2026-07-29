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

#ifndef MANGOS_PACKEDVALUES_H
#define MANGOS_PACKEDVALUES_H

#include "Platform/Define.h"

/**
 * @file
 * @brief Packing two 16-bit halves into one 32-bit value, and back.
 *
 * Used for composite keys -- map/zone, entry/index -- where the database and the
 * client both expect the halves fused into a single integer.
 *
 * The 64-bit MAKE_PAIR64 / PAIR64_HIPART / PAIR64_LOPART variants that used to
 * live alongside these had no callers left anywhere in the tree and are gone.
 */

#define MAKE_PAIR32(l, h)  uint32(uint16(l) | (uint32(h) << 16))
#define PAIR32_HIPART(x)   uint16((uint32(x) >> 16) & 0x0000FFFF)
#define PAIR32_LOPART(x)   uint16(uint32(x)         & 0x0000FFFF)

#endif
