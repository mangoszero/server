# MaNGOS is a full featured server for World of Warcraft, supporting
# the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
#
# Copyright (C) 2005-2025 MaNGOS <https://www.getmangos.eu>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

# =============================================================================
# Compatibility glue for the submodules.
#
# The submodules (Eluna, SD3, realmd) are shared with other
# cores and MUST NOT be modified: this fork cannot carry local commits in them,
# because a clone would then reference objects that exist on no remote. So every
# adaptation they need in order to build here lives in this tree instead, under
# src/shared/Compat/<name>/, and is attached to their targets from the outside.
#
# Two things are needed, and the second is the reason this file exists.
#
#   1. Headers this fork removed or moved, supplied under their original names
#      so an unmodified `#include "Common.h"` still resolves. A plain
#      target_include_directories is enough for that.
#
#   2. Declarations that used to arrive *transitively*. Most submodule sources
#      never included Common.h themselves -- they got its contents through some
#      core header that included it. Nothing in their include chain names the
#      shim, so no include directory can reach them, and adding an include to
#      the submodule is exactly what is forbidden. The only lever left from
#      outside a translation unit is a forced include, so the shim is injected
#      at the top of every source in the target.
#
# Restricted to C++: some of these targets also compile vendored C (Lua), which
# must not see a C++ header.
#
# WHY THIS IS A SHIM AND NOT A PATCH
#
# The submodule repositories will be updated properly when that becomes
# possible -- the changes are already written and are queued as upstream pull
# requests. Until they land, this fork must build against untouched upstream
# checkouts, and the alternative (applying patches to the submodule worktrees at
# build time) was rejected deliberately: it leaves the submodules dirty, is not
# idempotent across re-runs, and breaks silently whenever upstream moves.
#
# So this layer is scaffolding with a known end date. When a submodule's pull
# request merges, bump the submodule and delete its directory under
# src/shared/Compat/ together with the call below -- nothing else in the tree
# refers to it. Do not extend a shim to cover new divergence; that is a sign the
# fix belongs upstream instead.
# =============================================================================

function(mangos_submodule_compat target compat_dir)
    if(NOT TARGET ${target})
        message(FATAL_ERROR
            "mangos_submodule_compat: no such target '${target}'. The submodule's "
            "CMakeLists likely renamed it; the compat layer would silently do "
            "nothing, so this is fatal rather than skipped.")
    endif()

    if(NOT IS_DIRECTORY "${compat_dir}")
        message(FATAL_ERROR
            "mangos_submodule_compat: '${compat_dir}' does not exist.")
    endif()

    target_include_directories(${target} PRIVATE "${compat_dir}")

    set(_prelude "${compat_dir}/Common.h")
    if(EXISTS "${_prelude}")
        if(MSVC)
            target_compile_options(${target} PRIVATE
                "$<$<COMPILE_LANGUAGE:CXX>:/FI${_prelude}>")
        else()
            target_compile_options(${target} PRIVATE
                "$<$<COMPILE_LANGUAGE:CXX>:-include${_prelude}>")
        endif()
    endif()
endfunction()

# The same thing, scoped to a list of sources instead of a target.
#
# Eluna has no target of its own: this fork globs its sources straight into
# `game`. Applying the compat layer to that target would force-include the shim
# into all ~700 core sources -- which is precisely the tree-wide Common.h that
# was deleted, reinstated through the back door. So the shim is attached to the
# Eluna sources alone, and the rest of `game` never sees it.
function(mangos_submodule_compat_sources sources compat_dir)
    if(NOT IS_DIRECTORY "${compat_dir}")
        message(FATAL_ERROR
            "mangos_submodule_compat_sources: '${compat_dir}' does not exist.")
    endif()

    if(NOT sources)
        message(FATAL_ERROR
            "mangos_submodule_compat_sources: empty source list for "
            "'${compat_dir}'. The submodule glob matched nothing, so the compat "
            "layer would apply to no file at all.")
    endif()

    set(_prelude "${compat_dir}/Common.h")
    if(MSVC)
        set(_flag "/FI${_prelude}")
    else()
        set(_flag "-include${_prelude}")
    endif()

    set_source_files_properties(${sources} PROPERTIES
        INCLUDE_DIRECTORIES "${compat_dir}"
        COMPILE_OPTIONS     "${_flag}")
endfunction()
