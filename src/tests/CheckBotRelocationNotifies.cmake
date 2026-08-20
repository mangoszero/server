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
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.

# =============================================================================
# A BOT THAT MOVES WITHOUT TELLING ANYONE IS RENDERED AS A GLIDE.
#
# A playerbot has no client, so nothing echoes its movement back. When the server
# relocates one directly, no observer is told: UpdateVisibilityOf is transition-only,
# so a hop that stays inside the visibility bubble sends nothing at all. The watcher
# goes on rendering the bot where it was.
#
# The 1.12 client then makes that visible. On the next SMSG_MONSTER_MOVE it does NOT
# read the packet's declared start -- it prepends its own rendered position to the path
# and travels the stitched leg at min(runSpeed * 4, length/duration). So the bot slides
# the whole relocation distance at up to 28 yd/s while every packet on the wire remains
# an innocent 7.00 yd/s. Measured once at 56.89 yards covered in 2032 ms, where running
# it would have taken 8.13 seconds.
#
# No packet-level audit can find that motion, because the client manufactures it. The
# only defence is not to relocate a bot silently in the first place.
#
# This check therefore does two things:
#   1. refuses any NEW raw relocation in the bot module, and
#   2. pins the near-teleport notification so it cannot be quietly removed.
#
# Source-level for the same reason as CheckMovementStateSeed: reaching it needs Player,
# Map and a live client, and the test binary deliberately links no `game`.
#
# Comments are stripped before matching. Without that this check would match its own
# explanation and could never go red -- commenting a call out would leave its name in
# the comment above it and the guard would pass. Each requirement below was verified by
# deleting the call and watching this fail.
#
# It remains a lexical tripwire, not a proof. It reads text, so it cannot tell live code
# from a call sitting inside `#if 0`, and it cannot see a relocation reached through a
# helper or an alias it does not name. Its job is to make the silent kind of relocation
# something you have to argue for in this file, not to certify the ones that are here.
# =============================================================================

if(NOT DEFINED SOURCE_ROOT)
  message(FATAL_ERROR "SOURCE_ROOT must be set")
endif()

set(BOT_DIR "${SOURCE_ROOT}/src/modules/Bots/playerbot")
set(VIOLATIONS "")

# Both comment forms, because either can hide a call or fake one. The block-comment pattern
# is the classic C form rather than "/\*.*\*/": CMake regexes are greedy with no lazy
# quantifier, so the naive version would swallow everything between the FIRST /* and the
# LAST */ in the file -- including all the code in between.
set(LINE_COMMENT "//[^\n]*")
set(BLOCK_COMMENT "/\\*[^*]*\\*+([^/*][^*]*\\*+)*/")

# Strip comments from ${IN}, leaving executable text in ${OUT}.
#
# A function, not a macro. Macro arguments are substituted as literal text before parsing,
# so passing a source file through one makes CMake try to parse the C++ as CMake code and
# fail on the first `#ifndef`.
function(strip_comments IN OUT)
  string(REGEX REPLACE "${BLOCK_COMMENT}" "" STRIPPED "${IN}")
  string(REGEX REPLACE "${LINE_COMMENT}" "" STRIPPED "${STRIPPED}")
  set(${OUT} "${STRIPPED}" PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------
# 1. Raw relocations, allowlisted per call and counted.
#
# The allowlist grants ONE pattern in ONE file, N times -- not a blanket exemption for the
# file. Both halves of that are load-bearing:
#
#   * Per file+pattern, because a whole-file entry for MovementActions.cpp would have
#     re-admitted the silent SetPosition in Follow that caused the glide, in the very file
#     the fix was made in.
#   * Counted, because otherwise the grant covers every future occurrence too: one
#     legitimate relocation in a file would license a second, silent one beside it.
#
# A count that no longer matches is not necessarily a bug -- but it is always a new
# relocation, and the whole point is that a human looks at those. Update the number when
# you have confirmed the new call notifies somebody.
#
# Every entry below is packet-backed. If you add one, be sure it sends something and say
# what: MSG_MOVE_JUMP, MSG_MOVE_FALL_LAND, a create packet, a heartbeat.
#
# ->SetPosition( is granted to nobody, in any file, at any count. It is the one call here
# that notifies nothing whatsoever on its own.
# ---------------------------------------------------------------------------
set(RELOCATION_PATTERNS
    "->[ \t]*SetPosition[ \t]*\\("
    "PlayerRelocation[ \t]*\\("
    "Place[ \t]*\\([ \t]*\\)[ \t]*\\.[ \t]*MoveTo[ \t]*\\("
    "m_movementInfo[ \t]*\\.[ \t]*ChangePosition[ \t]*\\(")

# file / pattern / how many times it may appear
set(ALLOWED_RELOCATIONS
    # Jump simulator: integrates its own parabola, broadcasting MSG_MOVE_JUMP as it leaves
    # the ground and MSG_MOVE_FALL_LAND when it touches down. Three position writes -- the
    # launch, each airborne step, and the landing -- plus the grid relocation for the last.
    "PlayerbotAI.cpp"                      "m_movementInfo[ \t]*\\.[ \t]*ChangePosition[ \t]*\\("  3
    "PlayerbotAI.cpp"                      "PlayerRelocation[ \t]*\\("                             1
    # Taxi landing: the action broadcasts the arrival itself.
    "strategy/actions/TaxiAction.cpp"      "m_movementInfo[ \t]*\\.[ \t]*ChangePosition[ \t]*\\("  1
    "strategy/actions/TaxiAction.cpp"      "PlayerRelocation[ \t]*\\("                             1)

file(GLOB_RECURSE BOT_SOURCES "${BOT_DIR}/*.cpp" "${BOT_DIR}/*.h")
list(LENGTH ALLOWED_RELOCATIONS ALLOWED_COUNT)
math(EXPR ALLOWED_LAST "${ALLOWED_COUNT} - 1")

foreach(FILE IN LISTS BOT_SOURCES)
  file(RELATIVE_PATH RELATIVE "${BOT_DIR}" "${FILE}")
  file(READ "${FILE}" RAW)
  strip_comments("${RAW}" TEXT)

  foreach(PATTERN IN LISTS RELOCATION_PATTERNS)
    string(REGEX MATCHALL "${PATTERN}" FOUND "${TEXT}")
    list(LENGTH FOUND FOUND_COUNT)
    if(FOUND_COUNT EQUAL 0)
      continue()
    endif()

    set(GRANTED 0)
    foreach(INDEX RANGE 0 ${ALLOWED_LAST} 3)
      list(GET ALLOWED_RELOCATIONS ${INDEX} ALLOWED_FILE)
      math(EXPR PATTERN_INDEX "${INDEX} + 1")
      math(EXPR COUNT_INDEX "${INDEX} + 2")
      list(GET ALLOWED_RELOCATIONS ${PATTERN_INDEX} ALLOWED_PATTERN)
      list(GET ALLOWED_RELOCATIONS ${COUNT_INDEX} ALLOWED_TIMES)
      if(RELATIVE STREQUAL "${ALLOWED_FILE}" AND PATTERN STREQUAL "${ALLOWED_PATTERN}")
        set(GRANTED ${ALLOWED_TIMES})
      endif()
    endforeach()

    if(FOUND_COUNT GREATER GRANTED)
      list(APPEND VIOLATIONS
          "${RELATIVE}: relocates a bot directly, ${FOUND_COUNT} time(s) matching ${PATTERN} where ${GRANTED} is allowed -- route it through TeleportTo, or grant this file AND this call AND the new count, naming the packet it sends")
    endif()
  endforeach()
endforeach()

# ---------------------------------------------------------------------------
# 2. The near-teleport notification itself.
#
# Each requirement is checked inside the ONE function that has to make the call, not
# anywhere in the file. Scanning the whole file cannot go red: `CancelJump()` would be
# satisfied by its own definition, so deleting the call from the ack path would leave
# the check green. Function scope is what gives the check teeth.
#
# Bodies are sliced by literal offset rather than regex. CMake regexes are greedy with
# no lazy quantifier, so ".*\n}" would swallow every function to the end of the file;
# FIND stops at the first closing brace in column 0, which -- since everything nested is
# indented -- is the end of the function and nothing else.
# ---------------------------------------------------------------------------
set(ACK_FILE "${BOT_DIR}/PlayerbotAI.cpp")

# Slice the body of ${SIGNATURE} out of ${CODE} into ${OUT}, or "" if absent.
function(extract_body CODE SIGNATURE OUT)
  string(FIND "${CODE}" "${SIGNATURE}" START)
  if(START LESS 0)
    set(${OUT} "" PARENT_SCOPE)
    return()
  endif()
  string(SUBSTRING "${CODE}" ${START} -1 TAIL)
  string(FIND "${TAIL}" "\n}" STOP)
  if(STOP LESS 0)
    set(${OUT} "${TAIL}" PARENT_SCOPE)
  else()
    string(SUBSTRING "${TAIL}" 0 ${STOP} BODY)
    set(${OUT} "${BODY}" PARENT_SCOPE)
  endif()
endfunction()

if(NOT EXISTS "${ACK_FILE}")
  list(APPEND VIOLATIONS "PlayerbotAI.cpp: file is gone")
else()
  file(READ "${ACK_FILE}" ACK_RAW)
  strip_comments("${ACK_RAW}" ACK_CODE)

  extract_body("${ACK_CODE}" "void PlayerbotAI::HandleTeleportAck()" ACK_BODY)
  extract_body("${ACK_CODE}" "void PlayerbotAI::ResyncObserversAfterTeleport()" RESYNC_BODY)

  # The bodies are real code and can contain any delimiter, so they are never packed
  # into a list -- each function keeps its own loop over "call -> why it is load-bearing".
  set(ACK_CALLS
      "CancelJump\\(\\)"
      "a jump surviving the teleport lands the bot back where the old parabola started, silently overwriting the destination"
      "SendHeartBeat\\(\\)"
      "nothing else carries the destination to observers, so they keep rendering the bot at its old position and the client glides it across the gap"
      "ResyncObserversAfterTeleport\\(\\)"
      "the heartbeat only moves observers who can already see the bot, leaving a stale copy with any who lost and regained visibility")

  set(RESYNC_CALLS
      "DestroyForPlayer\\("
      "the observer's stale copy of the bot is never dropped"
      "m_clientGUIDs\\.erase\\("
      "DestroyForPlayer sends the packet but does not update the ledger, so the observer is never offered the bot again"
      "GetCamera\\(\\)\\.UpdateVisibilityOf\\("
      "visibility must be recomputed from the observer's camera rather than its body, which differ under farsight")

  foreach(FUNCTION HandleTeleportAck ResyncObserversAfterTeleport)
    if(FUNCTION STREQUAL "HandleTeleportAck")
      set(BODY "${ACK_BODY}")
      set(CALLS ${ACK_CALLS})
    else()
      set(BODY "${RESYNC_BODY}")
      set(CALLS ${RESYNC_CALLS})
    endif()

    if(BODY STREQUAL "")
      list(APPEND VIOLATIONS "PlayerbotAI.cpp: PlayerbotAI::${FUNCTION} is gone -- the near-teleport notification cannot be verified")
      continue()
    endif()

    list(LENGTH CALLS COUNT)
    math(EXPR LAST "${COUNT} - 1")
    foreach(INDEX RANGE 0 ${LAST} 2)
      list(GET CALLS ${INDEX} CALL)
      math(EXPR NEXT "${INDEX} + 1")
      list(GET CALLS ${NEXT} WHY)
      if(NOT BODY MATCHES "${CALL}")
        list(APPEND VIOLATIONS "PlayerbotAI.cpp: ${FUNCTION} no longer calls ${CALL} -- ${WHY}")
      endif()
    endforeach()
  endforeach()
endif()

if(VIOLATIONS)
  string(REPLACE ";" "\n  " REPORT "${VIOLATIONS}")
  message(FATAL_ERROR "Bot relocations that do not notify observers:\n  ${REPORT}")
endif()

message(STATUS "Bot relocations notify observers")
