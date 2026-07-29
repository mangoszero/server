/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
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

#ifndef MANGOS_H_LINEOFSIGHTEXEMPTIONS
#define MANGOS_H_LINEOFSIGHTEXEMPTIONS

// Spells configured to skip the line-of-sight check (vmap.ignoreSpellIds).
//
// This used to live on VMapFactory, which had nothing to do with spells and was reached
// only because it happened to be linked. The list is a game rule, so it lives with the
// game; the collision engine never hears about it.

#include "Platform/Define.h"

#include <string>
#include <unordered_set>

namespace LineOfSightExemptions
{
    /// Replaces the set from a comma-separated list of spell ids.
    void Load(const std::string& spellIds);

    bool Has(uint32 spellId);

    void Clear();
}

#endif
