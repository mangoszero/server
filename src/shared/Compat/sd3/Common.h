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

#ifndef MANGOS_COMPAT_SD3_COMMON_H
#define MANGOS_COMPAT_SD3_COMMON_H

/**
 * @file
 * @brief TEMPORARY -- lets an unmodified ScriptDev3 build against this fork.
 *
 * SD3 is shared with other cores and is not modified here. Only one of its
 * files, system/ScriptDevMgr.h, actually includes Common.h; the other ~490
 * scripts received its contents transitively, through a core header that
 * included it. This fork deleted Common.h, which breaks both cases -- and the
 * second cannot be fixed by any include directory, because those scripts name
 * no header to redirect.
 *
 * So this file is force-included into every SD3 C++ source. That keeps the
 * scripts compiling without a single edit inside the submodule, which is the
 * whole point: SD3 must stay at its upstream commit.
 *
 * Contents are the union of what commit 7d9bb6e (kept on branch
 * sd3-compat-patch) added when it made the same adaptation inside SD3. Not a
 * copy of the old Common.h, and it must not grow into one.
 *
 * This is scaffolding with a known end date. The change this stands in for is
 * already written and queued as a pull request against the submodule's own
 * repository; once that merges, bump the submodule and delete this file and its
 * mangos_submodule_compat() call. Do not widen it to cover new divergence --
 * that is a sign the fix belongs upstream, not here.
 */

#include <algorithm>
#include <cmath>
#include <list>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "Common/ServerDefines.h"
#include "Common/TimeConstants.h"
#include "Platform/Define.h"
#include "Utilities/MathDefines.h"
#include "Utilities/Util.h"

#endif // MANGOS_COMPAT_SD3_COMMON_H
