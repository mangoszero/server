/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
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
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#ifndef MANGOS_COMPAT_ELUNA_OBJECTACCESSOR_H
#define MANGOS_COMPAT_ELUNA_OBJECTACCESSOR_H

/**
 * @file
 * @brief TEMPORARY -- answers Eluna's `#include "ObjectAccessor.h"`.
 *
 * ObjectAccessor was split in this fork into PlayerRegistry, CorpseManager and
 * ObjectLookup. Eluna is shared with cores that still ship the old class, so it
 * still includes the old header; this file supplies the name and forwards to the
 * shim that maps the old API onto the new components.
 *
 * Visible to the Eluna target only. Delete together with
 * Object/ElunaObjectAccessorShim.h once Eluna calls the components directly.
 *
 * This is scaffolding with a known end date. The change this stands in for is
 * already written and queued as a pull request against the submodule's own
 * repository; once that merges, bump the submodule and delete this file and its
 * mangos_submodule_compat() call. Do not widen it to cover new divergence --
 * that is a sign the fix belongs upstream, not here.
 */

#include "Object/ElunaObjectAccessorShim.h"

#endif // MANGOS_COMPAT_ELUNA_OBJECTACCESSOR_H
