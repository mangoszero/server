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
 */

#ifndef MANGOS_WARDEN_EVIDENCE_H
#define MANGOS_WARDEN_EVIDENCE_H

#include "WardenProtocol.h"

#include <variant>

namespace warden
{
enum class TimingOutcome : uint8
{
    Stable,
    Unstable
};

/** Validated timing evidence only; no keys or raw packet bytes cross here. */
struct TimingEvidence
{
    uint32 requestId = 0;
    TimingOutcome outcome = TimingOutcome::Unstable;
    uint32 clientTick = 0;
};

enum class MpqOutcome : uint8
{
    Match,
    DigestMismatch,
    Unavailable
};

/** Validated archive evidence containing only stable catalogue identity. */
struct MpqEvidence
{
    uint32 requestId = 0;
    uint32 checkId = 0;
    MpqOutcome outcome = MpqOutcome::Unavailable;
};

enum class LuaOutcome : uint8
{
    Match,
    TextMismatch,
    Unavailable
};

/** Validated script evidence containing no query or returned client text. */
struct LuaEvidence
{
    uint32 requestId = 0;
    uint32 checkId = 0;
    LuaOutcome outcome = LuaOutcome::Unavailable;
};

using WardenEvidence =
    std::variant<TimingEvidence, MpqEvidence, LuaEvidence>;

// Fixed labels are safe for operator summaries and contain no client data.
char const* ToString(TimingOutcome outcome);
char const* ToString(MpqOutcome outcome);
char const* ToString(LuaOutcome outcome);
}

#endif
