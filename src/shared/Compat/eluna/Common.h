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

#ifndef MANGOS_COMPAT_ELUNA_COMMON_H
#define MANGOS_COMPAT_ELUNA_COMMON_H

/**
 * @file
 * @brief TEMPORARY -- lets an unmodified Eluna build against this fork.
 *
 * Eluna is shared with other cores and is not modified here. Five of its
 * headers open with `#include "Common.h"`, a header this fork deleted; this
 * file answers that include, and is visible to the Eluna target alone, so
 * Common.h stays dead everywhere else.
 *
 * It is also force-included into every Eluna C++ source. That is not belt and
 * braces: ElunaConfig.h uses AccountTypes without including anything for it,
 * having received it transitively through some core header that itself included
 * Common.h. No include directory can fix a file that names no header, and
 * editing Eluna is the thing being avoided -- so the declarations are injected
 * ahead of the source instead.
 *
 * Contents are the union of what commit c0e04e6 (kept on branch
 * eluna-compat-patch) added to Eluna when it removed those includes: what Eluna
 * demonstrably uses, and nothing more. It is not a copy of the old Common.h and
 * must not grow into one.
 *
 * This is scaffolding with a known end date. The change this stands in for is
 * already written and queued as a pull request against the submodule's own
 * repository; once that merges, bump the submodule and delete this file and its
 * mangos_submodule_compat() call. Do not widen it to cover new divergence --
 * that is a sign the fix belongs upstream, not here.
 */

#include <cmath>
#include <cstdio>
#include <list>
#include <memory>
#include <string>
#include <vector>

// AccountTypes, for ElunaConfig.h.
#include "Common/ServerDefines.h"
#include "Platform/Define.h"

#endif // MANGOS_COMPAT_ELUNA_COMMON_H
