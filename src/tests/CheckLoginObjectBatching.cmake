# SPDX-License-Identifier: GPL-3.0-or-later
#
# MaNGOS is a full featured server for World of Warcraft, supporting
# the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
#
# Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
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
# along with this program. If not, see <https://www.gnu.org/licenses/>.

function(require_count TEXT_VAR PATTERN EXPECTED DESCRIPTION)
    string(REGEX MATCHALL "${PATTERN}" MATCHES "${${TEXT_VAR}}")
    list(LENGTH MATCHES COUNT)
    if(NOT COUNT EQUAL EXPECTED)
        message(FATAL_ERROR
            "Login object batching: ${DESCRIPTION}; expected ${EXPECTED}, found ${COUNT}")
    endif()
endfunction()

function(require_before TEXT_VAR FIRST SECOND DESCRIPTION)
    string(FIND "${${TEXT_VAR}}" "${FIRST}" FIRST_AT)
    string(FIND "${${TEXT_VAR}}" "${SECOND}" SECOND_AT)
    if(FIRST_AT EQUAL -1 OR SECOND_AT EQUAL -1 OR
       SECOND_AT LESS_EQUAL FIRST_AT)
        message(FATAL_ERROR "Login object batching: ${DESCRIPTION}")
    endif()
endfunction()

set(GAME_ROOT "${SOURCE_ROOT}/src/game")
file(READ "${GAME_ROOT}/WorldHandlers/UpdateData.h" UPDATE_DATA_H)
file(READ "${GAME_ROOT}/WorldHandlers/Map.cpp" MAP_CPP)
file(READ "${GAME_ROOT}/WorldHandlers/TransportMap.cpp" TRANSPORT_MAP_CPP)
file(READ "${GAME_ROOT}/Object/Camera.h" CAMERA_H)
file(READ "${GAME_ROOT}/WorldHandlers/GridNotifiers.cpp" GRID_NOTIFIERS_CPP)

require_count(UPDATE_DATA_H "class[ \\t]+InitialWorldUpdateBatch" 1
    "one login-scoped batch owner must carry update data and transport state")

string(FIND "${MAP_CPP}" "bool Map::Add(Player* player)" MAP_ADD_BEGIN)
if(MAP_ADD_BEGIN EQUAL -1)
    message(FATAL_ERROR "Login object batching: cannot locate Map::Add(Player*)")
endif()
string(SUBSTRING "${MAP_CPP}" ${MAP_ADD_BEGIN} -1 MAP_ADD_TAIL)
string(FIND "${MAP_ADD_TAIL}" "template<class T>" MAP_ADD_LENGTH)
if(MAP_ADD_LENGTH LESS_EQUAL 0)
    message(FATAL_ERROR "Login object batching: cannot bound Map::Add(Player*)")
endif()
string(SUBSTRING "${MAP_ADD_TAIL}" 0 ${MAP_ADD_LENGTH} MAP_ADD_BODY)
require_count(MAP_ADD_BODY "PlayerLoading[ \\t]*\\(" 1
    "ordinary map entry must select batching only from the login lifecycle")
require_count(MAP_ADD_BODY
    "GetCamera[ \\t]*\\([ \\t]*\\)[ \\t]*\\.[ \\t]*GetBody[ \\t]*\\([ \\t]*\\)[ \\t]*==[ \\t]*player" 1
    "a redirected login camera must retain the legacy multi-packet path")
require_count(MAP_ADD_BODY "InitialWorldUpdateBatch" 1
    "ordinary-map login must own exactly one initial batch")
require_before(MAP_ADD_BODY "SendInitSelf(player" "SendInitTransports(player"
    "self and inventory must precede map-wide vessels")
require_before(MAP_ADD_BODY "SendInitTransports(player" "Event_AddedToWorld("
    "vessels must be accumulated before the owner visibility sweep flushes")
require_before(MAP_ADD_BODY "Event_AddedToWorld(" "UpdateObjectVisibility(player"
    "the login batch must flush before other clients are notified")
require_count(MAP_ADD_BODY "return[ \\t]+true" 1
    "batch failure must not skip balanced map and instance enter hooks")

string(FIND "${TRANSPORT_MAP_CPP}" "bool TransportMap::Add(Player* passenger)"
    TRANSPORT_ADD_BEGIN)
string(FIND "${TRANSPORT_MAP_CPP}" "void TransportMap::Embark(Player* passenger)"
    TRANSPORT_ADD_END)
if(TRANSPORT_ADD_BEGIN EQUAL -1 OR TRANSPORT_ADD_END EQUAL -1 OR
   TRANSPORT_ADD_END LESS_EQUAL TRANSPORT_ADD_BEGIN)
    message(FATAL_ERROR "Login object batching: cannot locate TransportMap::Add(Player*)")
endif()
math(EXPR TRANSPORT_ADD_LENGTH
    "${TRANSPORT_ADD_END} - ${TRANSPORT_ADD_BEGIN}")
string(SUBSTRING "${TRANSPORT_MAP_CPP}" ${TRANSPORT_ADD_BEGIN}
    ${TRANSPORT_ADD_LENGTH} TRANSPORT_ADD_BODY)
require_count(TRANSPORT_ADD_BODY "PlayerLoading[ \\t]*\\(" 1
    "transport-map entry must batch only during login")
require_count(TRANSPORT_ADD_BODY
    "GetCamera[ \\t]*\\([ \\t]*\\)[ \\t]*\\.[ \\t]*GetBody[ \\t]*\\([ \\t]*\\)[ \\t]*==[ \\t]*passenger" 1
    "a redirected transport login camera must retain the legacy multi-packet path")
require_count(TRANSPORT_ADD_BODY "InitialWorldUpdateBatch" 1
    "transport-map login must own exactly one initial batch")
require_before(TRANSPORT_ADD_BODY "BuildCreateUpdateBlockForPlayer"
    "Event_AddedToWorld("
    "vessel and passenger blocks must precede the owner visibility sweep")
require_before(TRANSPORT_ADD_BODY "Event_AddedToWorld("
    "UpdateObjectVisibility(passenger"
    "transport login must flush before other clients are notified")
require_count(TRANSPORT_ADD_BODY "return[ \\t]+true" 1
    "transport batch failure must leave through the normal add epilogue")

require_count(TRANSPORT_MAP_CPP
    "TransportMap::AppendVesselCreateBlocks[ \\t]*\\(" 1
    "one append-only vessel seam must serve login and ordinary announcements")
require_count(TRANSPORT_MAP_CPP
    "AppendVesselCreateBlocks[ \\t]*\\(" 3
    "the append seam must serve transport login and ordinary announcements")
require_count(MAP_CPP
    "TransportMap::AppendVesselCreateBlocks[ \\t]*\\(" 2
    "both normal-map transport sources must append into the shared login batch")

require_count(CAMERA_H
    "GetOwner[ \\t]*\\([ \\t]*\\)[ \\t]*==[ \\t]*batchOwner" 1
    "only the camera owned by the logging-in player may consume the batch")

string(FIND "${GRID_NOTIFIERS_CPP}" "void VisibleNotifier::Notify()" NOTIFY_BEGIN)
string(FIND "${GRID_NOTIFIERS_CPP}" "void MessageDeliverer::Visit" NOTIFY_END)
if(NOTIFY_BEGIN EQUAL -1 OR NOTIFY_END EQUAL -1 OR
   NOTIFY_END LESS_EQUAL NOTIFY_BEGIN)
    message(FATAL_ERROR "Login object batching: cannot locate VisibleNotifier::Notify")
endif()
math(EXPR NOTIFY_LENGTH "${NOTIFY_END} - ${NOTIFY_BEGIN}")
string(SUBSTRING "${GRID_NOTIFIERS_CPP}" ${NOTIFY_BEGIN}
    ${NOTIFY_LENGTH} NOTIFY_BODY)
require_count(NOTIFY_BODY "BuildPacket[ \\t]*\\(" 1
    "the visibility notifier must own the single update-packet build")
require_count(NOTIFY_BODY "MarkSent[ \\t]*\\(" 1
    "a successful shared flush must consume the batch exactly once")
require_before(NOTIFY_BODY "BuildPacket(" "MarkSent("
    "the batch cannot be consumed before its packet is built")
require_before(NOTIFY_BODY "MarkSent(" "SendAuraDurationsForTarget("
    "object creation must be sent before aura packets reference visible units")

message(STATUS "Login object batching boundary intact")
