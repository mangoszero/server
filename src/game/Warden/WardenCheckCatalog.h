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

#ifndef MANGOS_WARDEN_CHECK_CATALOG_H
#define MANGOS_WARDEN_CHECK_CATALOG_H

#include "WardenProtocol.h"

#include <string>
#include <variant>
#include <vector>

namespace warden
{
enum class CheckCatalogValidation : uint8
{
    Valid,
    InvalidHex,
    InvalidBuild,
    InvalidPlatform,
    InvalidLocale,
    InvalidId,
    InvalidType,
    InvalidEnabled,
    InvalidSortOrder,
    InvalidEvidenceClass,
    IllegalTypeEvidenceClass,
    InvalidUnusedField,
    InvalidPath,
    InvalidQuery,
    InvalidExpectedText,
    InvalidAddress,
    InvalidModuleName,
    InvalidLength,
    InvalidExpectedBytes,
    DuplicateId,
    DuplicateSortOrder,
    ConflictingRequestExpectation,
    EmptyCatalog,
    MissingTiming,
    MultipleTiming,
    DisabledTiming,
    MissingNonHealth
};

enum class WardenCheckType : uint8
{
    Timing = 0x57,
    Lua = 0x8B,
    Mpq = 0x98,
    Mem = 0xF3
};

enum class WardenEvidenceClass : uint8
{
    ProtocolHealth = 0,
    IntegrityInvariant = 1,
    ThreatSignature = 2,
    Corroboration = 3
};

enum class WardenCheckOutcome : uint8
{
    Match = 0,
    Mismatch = 1,
    Unavailable = 2,
    Stable = 3,
    Unstable = 4
};

/** Immutable identity for the transport-health check. */
struct TimingCheckProfile
{
    uint32 checkId = 0;
};

/** Immutable input needed to request and evaluate one client MPQ digest. */
struct MpqCheckProfile
{
    uint32 checkId = 0;
    std::string path;
    Digest20 expectedSha1{};
};

/** Immutable input needed to request and evaluate one client Lua lookup. */
struct LuaCheckProfile
{
    uint32 checkId = 0;
    std::string query;
    std::string expectedText;
};

/** Immutable input needed to request and classify one process-memory read. */
struct MemCheckProfile
{
    uint32 checkId = 0;
    // Empty selects an absolute process address; nonempty selects module+RVA.
    std::string moduleName;
    uint32 addressOrRva = 0;
    Bytes expectedBytes;
};

using WardenCheckPayload = std::variant<TimingCheckProfile,
    MpqCheckProfile, LuaCheckProfile, MemCheckProfile>;

/** One enabled, decoded check in canonical profile order. */
struct WardenCheckDefinition
{
    uint16 sortOrder = 0;
    WardenEvidenceClass evidenceClass =
        WardenEvidenceClass::ProtocolHealth;
    WardenCheckPayload payload;
};

uint32 GetWardenCheckId(WardenCheckDefinition const& definition);
WardenCheckType GetWardenCheckType(WardenCheckDefinition const& definition);
bool IsActionableEvidenceClass(WardenEvidenceClass evidenceClass);
bool IsConfirmationEligible(WardenCheckDefinition const& definition);

struct WardenProfileKey
{
    uint32 build = 0;
    std::string platform;
    std::string locale;
};

/** Immutable, complete set of enabled checks for one exact client profile. */
struct WardenCheckProfile
{
    WardenProfileKey key;
    uint32 totalRows = 0;
    std::vector<WardenCheckDefinition> checks;
    bool hasActionableChecks = false;
};

/** Raw SQL projection retained at full width until validation succeeds. */
struct WardenCheckRowInput
{
    uint32 build = 0;
    std::string platformHex;
    std::string localeHex;
    uint32 checkId = 0;
    uint32 type = 0;
    uint32 enabled = 0;
    uint32 sortOrder = 0;
    uint32 evidenceClass = 0;
    std::string moduleHex;
    uint32 address = 0;
    uint32 length = 0;
    std::string requestHex;
    std::string expectedHex;
};

/** Identifies the row or profile responsible for a validation failure. */
struct WardenCheckDiagnostic
{
    CheckCatalogValidation validation = CheckCatalogValidation::Valid;
    WardenProfileKey profile;
    uint32 checkId = 0;
};

/**
 * Selects active checks independently of the delivered module catalogue.
 * Archive contents are locale/build scoped even when module bytes are shared.
 */
class WardenCheckCatalog
{
public:
    WardenCheckProfile const* Find(uint32 build,
        std::string const& platform, std::string const& locale) const;
    std::vector<WardenCheckProfile> const& Profiles() const;
    uint32 TotalRows() const;
    uint32 EnabledRows() const;

private:
    friend class WardenCheckCatalogBuilder;

    std::vector<WardenCheckProfile> m_profiles;
    uint32 m_totalRows = 0;
    uint32 m_enabledRows = 0;
};

/** Validates raw rows and atomically constructs an immutable catalogue. */
class WardenCheckCatalogBuilder
{
public:
    CheckCatalogValidation Add(WardenCheckRowInput const& input,
        WardenCheckDiagnostic& diagnostic);
    CheckCatalogValidation Build(WardenCheckCatalog& output,
        WardenCheckDiagnostic& diagnostic);

private:
    struct PendingRow
    {
        WardenProfileKey key;
        uint32 checkId = 0;
        bool enabled = false;
        WardenCheckDefinition definition;
    };

    std::vector<PendingRow> m_rows;
};

char const* ToString(CheckCatalogValidation validation);
}

#endif
