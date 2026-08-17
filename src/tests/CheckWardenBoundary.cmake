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

function(read_code PATH OUTPUT)
    file(STRINGS "${PATH}" RAW_LINES)
    set(IN_BLOCK OFF)
    set(CODE_ONLY "")

    foreach(LINE IN LISTS RAW_LINES)
        if(IN_BLOCK)
            string(FIND "${LINE}" "*/" CLOSE_AT)
            if(CLOSE_AT EQUAL -1)
                continue()
            endif()
            math(EXPR CLOSE_AT "${CLOSE_AT} + 2")
            string(SUBSTRING "${LINE}" ${CLOSE_AT} -1 LINE)
            set(IN_BLOCK OFF)
        endif()

        string(REGEX REPLACE "/\\*[^*]*\\*+([^/*][^*]*\\*+)*/" " " LINE "${LINE}")
        string(FIND "${LINE}" "/*" OPEN_AT)
        if(NOT OPEN_AT EQUAL -1)
            string(SUBSTRING "${LINE}" 0 ${OPEN_AT} LINE)
            set(IN_BLOCK ON)
        endif()
        string(REGEX REPLACE "//.*$" "" LINE "${LINE}")
        string(APPEND CODE_ONLY "${LINE}\n")
    endforeach()

    set(${OUTPUT} "${CODE_ONLY}" PARENT_SCOPE)
endfunction()

function(require_count TEXT PATTERN EXPECTED DESCRIPTION)
    string(REGEX MATCHALL "${PATTERN}" MATCHES "${TEXT}")
    list(LENGTH MATCHES COUNT)
    if(NOT COUNT EQUAL EXPECTED)
        message(FATAL_ERROR
            "Warden boundary: ${DESCRIPTION}; expected ${EXPECTED}, found ${COUNT}")
    endif()
endfunction()

set(GAME_ROOT "${SOURCE_ROOT}/src/game")
read_code("${GAME_ROOT}/WorldHandlers/WardenHandler.cpp" WARDEN_HANDLER)
read_code("${GAME_ROOT}/WorldHandlers/World.cpp" WORLD_CPP)
read_code("${GAME_ROOT}/WorldHandlers/WorldSessionMgr.cpp" SESSION_MGR)
read_code("${GAME_ROOT}/WorldHandlers/CharacterHandler.cpp" CHARACTER_HANDLER)
read_code("${GAME_ROOT}/Server/WorldSession.cpp" SESSION_CPP)
read_code("${GAME_ROOT}/WorldHandlers/WorldConfig.cpp" WORLD_CONFIG)
read_code("${GAME_ROOT}/WorldHandlers/Map.cpp" MAP_CPP)
file(READ "${SOURCE_ROOT}/src/mangosd/mangosd.conf.dist.in"
    MANGOSD_CONFIG)
file(STRINGS "${SOURCE_ROOT}/src/mangosd/mangosd.conf.dist.in"
    MANGOSD_ACTIVE_EXACT_PROFILE
    REGEX "^[ \\t]*Warden\\.RequireExactProfile[ \\t]*=")

require_count("${WARDEN_HANDLER}" "m_warden->HandleEncrypted[ \\t]*\\(" 1
    "grouped handler must forward ingress exactly once")
require_count("${WARDEN_HANDLER}" "rfinish[ \\t]*\\(" 1
    "grouped handler must consume the packet exactly once")
if(WARDEN_HANDLER MATCHES "(^|[^A-Za-z0-9_])switch[ \\t]*\\(")
    message(FATAL_ERROR "Warden boundary: grouped handler contains inner-command dispatch")
endif()

require_count("${WORLD_CPP}" "UpdateWarden[ \\t]*\\([ \\t]*diff[ \\t]*\\)" 1
    "World::UpdateSessions must own exactly one deadline update")
require_count("${WORLD_CPP}" "OnAuthenticatedAdmission[ \\t]*\\(" 1
    "immediate AUTH_OK path must admit exactly once")
string(FIND "${WORLD_CPP}" "packet << uint8(AUTH_OK)" AUTH_OK_AT)
string(FIND "${WORLD_CPP}" "s->SendPendingAddonInfo()" ADDON_AT REVERSE)
string(FIND "${WORLD_CPP}" "s->OnAuthenticatedAdmission()" ADMISSION_AT)
if(AUTH_OK_AT EQUAL -1 OR ADDON_AT EQUAL -1 OR ADMISSION_AT EQUAL -1 OR
    ADDON_AT LESS_EQUAL AUTH_OK_AT OR ADMISSION_AT LESS_EQUAL ADDON_AT)
    message(FATAL_ERROR
        "Warden boundary: immediate admission must follow AUTH_OK and addon response")
endif()

require_count("${SESSION_CPP}" "m_warden->Start[ \\t]*\\(" 1
    "session bootstrap seam must own the only direct Start call")
require_count("${SESSION_CPP}"
    "m_warden->Update[ \\t]*\\([ \\t]*eligible[ \\t]*,[ \\t]*diffMs[ \\t]*\\)" 1
    "session update must pass only derived eligibility and elapsed time")
require_count("${SESSION_CPP}" "WardenEvidenceBatch const&" 1
    "session adapter must consume one complete Warden evidence batch")
require_count("${SESSION_CPP}"
    "void WorldSession::HandleWardenEvidenceBatch[ \\t]*\\(" 1
    "complete-batch policy application must have one session owner")
require_count("${SESSION_CPP}"
    "for[ \\t]*\\([ \\t]*warden::WardenEvidence const& evidence[ \\t]*:[ \\t]*batch\\.evidence[ \\t]*\\)" 1
    "session adapter must consume normalized Warden evidence exactly once")
require_count("${SESSION_CPP}" "m_warden->QueueConfirmation[ \\t]*\\(" 1
    "session policy must own one isolated confirmation path")
require_count("${SESSION_CPP}" "IsWardenEnforcementProfile[ \\t]*\\(" 1
    "session enforcement must use the exact-profile predicate")
require_count("${SESSION_CPP}" "ClassifyWardenProfile[ \\t]*\\(" 1
    "session admission must classify the exact-profile policy once")
require_count("${SESSION_CPP}"
    "CONFIG_BOOL_WARDEN_REQUIRE_EXACT_PROFILE" 1
    "session admission must snapshot the strict-profile setting once")
require_count("${WORLD_CONFIG}"
    "Warden\\.RequireExactProfile\"[ \\t]*,[ \\t]*true" 1
    "strict exact-profile admission must default on in world configuration")
require_count("${MANGOSD_ACTIVE_EXACT_PROFILE}"
    "Warden\\.RequireExactProfile[ \\t]*=[ \\t]*1" 1
    "distributed strict-profile admission must have one active default-on setting")
foreach(EXACT_PROFILE IN ITEMS "5875/Win/enUS" "6005/Win/enGB"
    "6141/Win/zhCN")
    if(NOT MANGOSD_CONFIG MATCHES "${EXACT_PROFILE}")
        message(FATAL_ERROR
            "Warden boundary: distributed config must identify exact profile ${EXACT_PROFILE}")
    endif()
endforeach()
if(NOT MANGOSD_CONFIG MATCHES
    "all other build/platform/locale combinations are disconnected")
    message(FATAL_ERROR
        "Warden boundary: distributed config must explain strict-profile lockout scope")
endif()
require_count("${SESSION_CPP}"
    "m_clientLocale[ \\t]*\\([ \\t]*locale[ \\t]*\\)" 1
    "session must preserve the unfallbacked client locale exactly once")
require_count("${SESSION_CPP}"
    "admission\\.clientLocale" 1
    "Warden profile selection must use the authenticated exact client locale")
if(SESSION_CPP MATCHES
    "localeNames\\[GetClientLocale[ \\t]*\\([ \\t]*\\)[ \\t]*\\]")
    message(FATAL_ERROR
        "Warden boundary: profile selection must not reconstruct the numeric DBC locale")
endif()
require_count("${SESSION_CPP}" "Warden healthy for player %s" 1
    "stable evidence must have one normal operator health message")
if(NOT SESSION_CPP MATCHES "GetPlayer[ \\t]*\\([ \\t]*\\)" OR
    NOT SESSION_CPP MATCHES "IsInWorld[ \\t]*\\([ \\t]*\\)" OR
    NOT SESSION_CPP MATCHES "m_playerLoading")
    message(FATAL_ERROR
        "Warden boundary: eligibility must require a non-loading player in world")
endif()
if(SESSION_CPP MATCHES "clientTick|checksum|decrypted|packet body")
    message(FATAL_ERROR
        "Warden boundary: session observability must not expose timing internals")
endif()

string(FIND "${SESSION_CPP}"
    "void WorldSession::HandleWardenLifecycle(" LIFECYCLE_BEGIN)
string(FIND "${SESSION_CPP}"
    "void WorldSession::HandleWardenEvidenceBatch(" EVIDENCE_BEGIN)
string(FIND "${SESSION_CPP}"
    "void WorldSession::PersistWardenAudit(" AUDIT_BEGIN)
string(FIND "${SESSION_CPP}"
    "void WorldSession::PersistWardenIncidentAndKick(" PERSIST_BEGIN)
string(FIND "${SESSION_CPP}"
    "void WorldSession::StartWardenBootstrap()" WARDEN_START_BEGIN)
if(LIFECYCLE_BEGIN EQUAL -1 OR EVIDENCE_BEGIN EQUAL -1 OR
    AUDIT_BEGIN EQUAL -1 OR PERSIST_BEGIN EQUAL -1 OR
    WARDEN_START_BEGIN EQUAL -1 OR
    EVIDENCE_BEGIN LESS_EQUAL LIFECYCLE_BEGIN OR
    AUDIT_BEGIN LESS_EQUAL EVIDENCE_BEGIN OR
    PERSIST_BEGIN LESS_EQUAL AUDIT_BEGIN OR
    WARDEN_START_BEGIN LESS_EQUAL PERSIST_BEGIN)
    message(FATAL_ERROR
        "Warden boundary: cannot locate ordered session enforcement helpers")
endif()

math(EXPR LIFECYCLE_LENGTH "${EVIDENCE_BEGIN} - ${LIFECYCLE_BEGIN}")
string(SUBSTRING "${SESSION_CPP}" ${LIFECYCLE_BEGIN} ${LIFECYCLE_LENGTH}
    LIFECYCLE_BODY)
if(LIFECYCLE_BODY MATCHES "WardenIncidentStore|Record[ \\t]*\\(")
    message(FATAL_ERROR
        "Warden boundary: lifecycle failure must never persist an incident")
endif()

math(EXPR ENFORCEMENT_LENGTH "${WARDEN_START_BEGIN} - ${EVIDENCE_BEGIN}")
string(SUBSTRING "${SESSION_CPP}" ${EVIDENCE_BEGIN} ${ENFORCEMENT_LENGTH}
    ENFORCEMENT_BODY)
if(ENFORCEMENT_BODY MATCHES
    "GetSecurity[ \\t]*\\(|SEC_[A-Z_]+|gmlevel|GameMaster")
    message(FATAL_ERROR
        "Warden boundary: enforcement must not exempt privileged accounts")
endif()
if(NOT ENFORCEMENT_BODY MATCHES "CheckPlanPurpose::Initial" OR
    NOT ENFORCEMENT_BODY MATCHES "DEBUG_LOG[ \\t]*\\(")
    message(FATAL_ERROR
        "Warden boundary: recurring clean evidence must use debug logging")
endif()

math(EXPR AUDIT_LENGTH "${PERSIST_BEGIN} - ${AUDIT_BEGIN}")
string(SUBSTRING "${SESSION_CPP}" ${AUDIT_BEGIN} ${AUDIT_LENGTH}
    AUDIT_BODY)
if(AUDIT_BODY MATCHES
    "KickPlayer|WardenIncidentStore|account_banned|m_wardenAggressive|m_wardenEnforcementClosed")
    message(FATAL_ERROR
        "Warden boundary: audit persistence must never enforce or alter escalation")
endif()

math(EXPR PERSIST_LENGTH "${WARDEN_START_BEGIN} - ${PERSIST_BEGIN}")
string(SUBSTRING "${SESSION_CPP}" ${PERSIST_BEGIN} ${PERSIST_LENGTH}
    PERSIST_BODY)
string(FIND "${PERSIST_BODY}" "WardenIncidentStore::Instance().Record"
    INCIDENT_RECORD_AT)
string(FIND "${PERSIST_BODY}" "KickPlayer()" INCIDENT_KICK_AT)
if(INCIDENT_RECORD_AT EQUAL -1 OR INCIDENT_KICK_AT EQUAL -1 OR
    INCIDENT_KICK_AT LESS_EQUAL INCIDENT_RECORD_AT)
    message(FATAL_ERROR
        "Warden boundary: confirmed incident must persist before link close")
endif()
require_count("${PERSIST_BODY}" "KickPlayer[ \\t]*\\(" 1
    "confirmed violation must request one idempotent link close")

string(FIND "${SESSION_CPP}" "void WorldSession::OnAuthenticatedAdmission()"
    SESSION_ADMISSION_BEGIN)
string(FIND "${SESSION_CPP}" "void WorldSession::HandleWardenLifecycle("
    SESSION_ADMISSION_END)
if(SESSION_ADMISSION_BEGIN EQUAL -1 OR SESSION_ADMISSION_END EQUAL -1 OR
    SESSION_ADMISSION_END LESS_EQUAL SESSION_ADMISSION_BEGIN)
    message(FATAL_ERROR
        "Warden boundary: cannot locate authenticated admission body")
endif()
math(EXPR SESSION_ADMISSION_LENGTH
    "${SESSION_ADMISSION_END} - ${SESSION_ADMISSION_BEGIN}")
string(SUBSTRING "${SESSION_CPP}" ${SESSION_ADMISSION_BEGIN}
    ${SESSION_ADMISSION_LENGTH} SESSION_ADMISSION_BODY)
if(SESSION_ADMISSION_BODY MATCHES
    "StartWardenBootstrap[ \\t]*\\(|m_warden->Start[ \\t]*\\(")
    message(FATAL_ERROR
        "Warden boundary: authenticated admission must provision without emitting")
endif()
string(FIND "${SESSION_ADMISSION_BODY}"
    "WardenProfileDisposition::Reject" PROFILE_REJECT_AT)
string(FIND "${SESSION_ADMISSION_BODY}"
    "WardenManager::Instance().Create" WARDEN_CREATE_AT)
if(PROFILE_REJECT_AT EQUAL -1 OR WARDEN_CREATE_AT EQUAL -1 OR
    PROFILE_REJECT_AT GREATER WARDEN_CREATE_AT)
    message(FATAL_ERROR
        "Warden boundary: strict unprofiled rejection must precede Warden creation")
endif()
math(EXPR PROFILE_REJECT_LENGTH
    "${WARDEN_CREATE_AT} - ${PROFILE_REJECT_AT}")
string(SUBSTRING "${SESSION_ADMISSION_BODY}" ${PROFILE_REJECT_AT}
    ${PROFILE_REJECT_LENGTH} PROFILE_REJECT_BODY)
string(FIND "${PROFILE_REJECT_BODY}" "admission.Clear()" REJECT_CLEAR_AT)
string(FIND "${PROFILE_REJECT_BODY}" "m_wardenEnforcementClosed = true"
    REJECT_LATCH_AT)
string(FIND "${PROFILE_REJECT_BODY}" "KickPlayer()" REJECT_KICK_AT)
string(FIND "${PROFILE_REJECT_BODY}" "return;" REJECT_RETURN_AT)
if(REJECT_CLEAR_AT EQUAL -1 OR REJECT_LATCH_AT EQUAL -1 OR
    REJECT_KICK_AT EQUAL -1 OR REJECT_RETURN_AT EQUAL -1 OR
    REJECT_LATCH_AT LESS_EQUAL REJECT_CLEAR_AT OR
    REJECT_KICK_AT LESS_EQUAL REJECT_LATCH_AT OR
    REJECT_RETURN_AT LESS_EQUAL REJECT_KICK_AT)
    message(FATAL_ERROR
        "Warden boundary: strict rejection must cleanse, latch, close, and terminate admission in order")
endif()
if(SESSION_ADMISSION_BODY MATCHES
    "WardenIncidentStore::Instance[ \\t]*\\(\\)[ \\t]*\\.[ \\t]*Record")
    message(FATAL_ERROR
        "Warden boundary: admission rejection must never record an incident")
endif()

require_count("${CHARACTER_HANDLER}" "StartWardenBootstrap[ \\t]*\\(" 2
    "character list and player login must each schedule bootstrap once")

string(FIND "${CHARACTER_HANDLER}"
    "void WorldSession::HandleCharEnum(QueryResult* result)" CHAR_ENUM_BEGIN)
string(FIND "${CHARACTER_HANDLER}"
    "void WorldSession::HandleCharEnumOpcode" CHAR_ENUM_END)
if(CHAR_ENUM_BEGIN EQUAL -1 OR CHAR_ENUM_END EQUAL -1 OR
    CHAR_ENUM_END LESS_EQUAL CHAR_ENUM_BEGIN)
    message(FATAL_ERROR "Warden boundary: cannot locate character-enum body")
endif()
math(EXPR CHAR_ENUM_LENGTH "${CHAR_ENUM_END} - ${CHAR_ENUM_BEGIN}")
string(SUBSTRING "${CHARACTER_HANDLER}" ${CHAR_ENUM_BEGIN}
    ${CHAR_ENUM_LENGTH} CHAR_ENUM_BODY)
require_count("${CHAR_ENUM_BODY}" "StartWardenBootstrap[ \\t]*\\(" 1
    "character-enum completion must schedule bootstrap exactly once")
string(FIND "${CHAR_ENUM_BODY}" "SendPacket(&data)" CHAR_LIST_SEND_AT)
string(FIND "${CHAR_ENUM_BODY}" "StartWardenBootstrap()" CHAR_ENUM_START_AT)
if(CHAR_LIST_SEND_AT EQUAL -1 OR CHAR_ENUM_START_AT EQUAL -1 OR
    CHAR_ENUM_START_AT LESS_EQUAL CHAR_LIST_SEND_AT)
    message(FATAL_ERROR
        "Warden boundary: character-list send must precede bootstrap emission")
endif()

string(FIND "${CHARACTER_HANDLER}"
    "void WorldSession::HandlePlayerLoginOpcode" PLAYER_LOGIN_BEGIN)
string(FIND "${CHARACTER_HANDLER}"
    "void WorldSession::HandlePlayerLogin(LoginQueryHolder* holder)"
    PLAYER_LOGIN_END)
if(PLAYER_LOGIN_BEGIN EQUAL -1 OR PLAYER_LOGIN_END EQUAL -1 OR
    PLAYER_LOGIN_END LESS_EQUAL PLAYER_LOGIN_BEGIN)
    message(FATAL_ERROR "Warden boundary: cannot locate player-login opcode body")
endif()
math(EXPR PLAYER_LOGIN_LENGTH "${PLAYER_LOGIN_END} - ${PLAYER_LOGIN_BEGIN}")
string(SUBSTRING "${CHARACTER_HANDLER}" ${PLAYER_LOGIN_BEGIN}
    ${PLAYER_LOGIN_LENGTH} PLAYER_LOGIN_BODY)
require_count("${PLAYER_LOGIN_BODY}" "StartWardenBootstrap[ \\t]*\\(" 1
    "player-login path must retain one non-gating bootstrap safety net")
string(FIND "${PLAYER_LOGIN_BODY}" "PlayerLoading()" PLAYER_LOGIN_GUARD_AT)
string(FIND "${PLAYER_LOGIN_BODY}" "StartWardenBootstrap()"
    PLAYER_LOGIN_START_AT)
string(FIND "${PLAYER_LOGIN_BODY}" "m_playerLoading = true"
    PLAYER_LOADING_SET_AT)
if(PLAYER_LOGIN_GUARD_AT EQUAL -1 OR PLAYER_LOGIN_START_AT EQUAL -1 OR
    PLAYER_LOADING_SET_AT EQUAL -1 OR
    PLAYER_LOGIN_START_AT LESS_EQUAL PLAYER_LOGIN_GUARD_AT OR
    PLAYER_LOGIN_START_AT GREATER_EQUAL PLAYER_LOADING_SET_AT)
    message(FATAL_ERROR
        "Warden boundary: login safety net must follow the duplicate guard and never gate loading")
endif()

string(FIND "${SESSION_MGR}" "void World::AddQueuedSession" ADD_QUEUE_BEGIN)
string(FIND "${SESSION_MGR}" "bool World::RemoveQueuedSession" ADD_QUEUE_END)
if(ADD_QUEUE_BEGIN EQUAL -1 OR ADD_QUEUE_END EQUAL -1 OR
    ADD_QUEUE_END LESS_EQUAL ADD_QUEUE_BEGIN)
    message(FATAL_ERROR "Warden boundary: cannot locate AddQueuedSession body")
endif()
math(EXPR ADD_QUEUE_LENGTH "${ADD_QUEUE_END} - ${ADD_QUEUE_BEGIN}")
string(SUBSTRING "${SESSION_MGR}" ${ADD_QUEUE_BEGIN} ${ADD_QUEUE_LENGTH}
    ADD_QUEUE_BODY)
if(ADD_QUEUE_BODY MATCHES "OnAuthenticatedAdmission")
    message(FATAL_ERROR "Warden boundary: queued sessions must not start Warden")
endif()
require_count("${SESSION_MGR}" "OnAuthenticatedAdmission[ \\t]*\\(" 1
    "queue release path must admit exactly once")
string(FIND "${SESSION_MGR}" "pop_sess->SendAuthWaitQue(0)" QUEUE_OK_AT)
string(FIND "${SESSION_MGR}" "pop_sess->OnAuthenticatedAdmission()"
    QUEUE_ADMISSION_AT)
if(QUEUE_OK_AT EQUAL -1 OR QUEUE_ADMISSION_AT EQUAL -1 OR
    QUEUE_ADMISSION_AT LESS_EQUAL QUEUE_OK_AT)
    message(FATAL_ERROR
        "Warden boundary: queue release admission must follow AUTH_OK")
endif()

if(MAP_CPP MATCHES "(^|[^A-Za-z0-9_])Warden([^A-Za-z0-9_]|$)")
    message(FATAL_ERROR "Warden boundary: Map.cpp must not own Warden updates")
endif()

file(GLOB WARDEN_SOURCES
    "${GAME_ROOT}/Warden/*.h" "${GAME_ROOT}/Warden/*.hpp"
    "${GAME_ROOT}/Warden/*.cpp" "${GAME_ROOT}/Warden/*.cc")
set(FORBIDDEN
    "LoginDatabase" "CharacterDatabase" "WorldDatabase" "KickPlayer"
    "BanAccount" "ByteArrayToHexStr" "hexlike[ \\t]*\\(")
foreach(SOURCE IN LISTS WARDEN_SOURCES)
    read_code("${SOURCE}" WARDEN_CODE)
    foreach(PATTERN IN LISTS FORBIDDEN)
        if(WARDEN_CODE MATCHES "${PATTERN}")
            message(FATAL_ERROR
                "Warden boundary: forbidden production dependency ${PATTERN} in ${SOURCE}")
        endif()
    endforeach()
endforeach()

file(GLOB_RECURSE INCIDENT_STORE_FILES
    "${GAME_ROOT}/*WardenIncidentStore.h"
    "${GAME_ROOT}/*WardenIncidentStore.cpp")
list(LENGTH INCIDENT_STORE_FILES INCIDENT_STORE_COUNT)
if(NOT INCIDENT_STORE_COUNT EQUAL 2)
    message(FATAL_ERROR
        "Warden boundary: expected one incident-store header/source pair")
endif()
foreach(SOURCE IN LISTS INCIDENT_STORE_FILES)
    string(FIND "${SOURCE}" "${GAME_ROOT}/Server/" SERVER_PREFIX_AT)
    if(NOT SERVER_PREFIX_AT EQUAL 0)
        message(FATAL_ERROR
            "Warden boundary: incident store escaped src/game/Server: ${SOURCE}")
    endif()
endforeach()

file(GLOB_RECURSE AUDIT_STORE_FILES
    "${GAME_ROOT}/*WardenAuditStore.h"
    "${GAME_ROOT}/*WardenAuditStore.cpp")
list(LENGTH AUDIT_STORE_FILES AUDIT_STORE_COUNT)
if(NOT AUDIT_STORE_COUNT EQUAL 2)
    message(FATAL_ERROR
        "Warden boundary: expected one audit-store header/source pair")
endif()
foreach(SOURCE IN LISTS AUDIT_STORE_FILES)
    string(FIND "${SOURCE}" "${GAME_ROOT}/Server/" SERVER_PREFIX_AT)
    if(NOT SERVER_PREFIX_AT EQUAL 0)
        message(FATAL_ERROR
            "Warden boundary: audit store escaped src/game/Server: ${SOURCE}")
    endif()
endforeach()

message(STATUS "Warden session boundary intact")
