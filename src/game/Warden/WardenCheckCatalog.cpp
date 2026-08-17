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

#include "WardenCheckCatalog.h"

#include <algorithm>
#include <array>
#include <limits>
#include <set>
#include <utility>

namespace
{
struct MpqCatalogRecord
{
    uint32 build;
    char const* platform;
    char const* locale;
    warden::MpqCheckProfile profile;
};

struct LuaCatalogRecord
{
    uint32 build;
    char const* platform;
    char const* locale;
    warden::LuaCheckProfile profile;
};

struct MemCatalogRecord
{
    uint32 build;
    char const* platform;
    char const* locale;
    std::vector<warden::MemCheckProfile> profiles;
};

MpqCatalogRecord const& Windows5875EnUsMpqRecord()
{
    static MpqCatalogRecord const record
    {
        5875,
        "Win",
        "enUS",
        {
            1,
            "DBFilesClient\\AreaTable.dbc",
            {
                0x7D, 0x88, 0x15, 0x4D, 0x34, 0x11, 0x81, 0x19,
                0x85, 0xF5, 0xD8, 0x11, 0x77, 0xC5, 0x45, 0x32,
                0x48, 0x13, 0x34, 0x43
            }
        }
    };
    return record;
}

MpqCatalogRecord const& Windows6005EnGbMpqRecord()
{
    static MpqCatalogRecord const record
    {
        6005,
        "Win",
        "enGB",
        {
            1,
            "DBFilesClient\\AreaTable.dbc",
            {
                0x7D, 0x88, 0x15, 0x4D, 0x34, 0x11, 0x81, 0x19,
                0x85, 0xF5, 0xD8, 0x11, 0x77, 0xC5, 0x45, 0x32,
                0x48, 0x13, 0x34, 0x43
            }
        }
    };
    return record;
}

MpqCatalogRecord const& Windows6141ZhCnMpqRecord()
{
    static MpqCatalogRecord const record
    {
        6141,
        "Win",
        "zhCN",
        {
            1,
            "DBFilesClient\\AreaTable.dbc",
            {
                0xC5, 0xA1, 0xDE, 0x4C, 0x1C, 0xD4, 0x12, 0xEB,
                0x4D, 0x2E, 0x02, 0xAF, 0xAB, 0x61, 0x31, 0xB7,
                0x37, 0xEF, 0xCA, 0xF0
            }
        }
    };
    return record;
}

LuaCatalogRecord const& Windows5875EnUsLuaRecord()
{
    static LuaCatalogRecord const record
    {
        5875,
        "Win",
        "enUS",
        {2, "OKAY", "Okay"}
    };
    return record;
}

LuaCatalogRecord const& Windows6005EnGbLuaRecord()
{
    static LuaCatalogRecord const record
    {
        6005,
        "Win",
        "enGB",
        {2, "OKAY", "Okay"}
    };
    return record;
}

LuaCatalogRecord const& Windows6141ZhCnLuaRecord()
{
    static LuaCatalogRecord const record
    {
        6141,
        "Win",
        "zhCN",
        // Exact UTF-8 bytes for U+786E U+5B9A avoid source-code-page drift.
        {2, "OKAY", "\xE7\xA1\xAE\xE5\xAE\x9A"}
    };
    return record;
}

MemCatalogRecord const& Windows5875EnUsMemRecord()
{
    static MemCatalogRecord const record
    {
        5875,
        "Win",
        "enUS",
        {
            {
                1107, "", 0x00618900,
                {
                    0x55, 0x8B, 0xEC, 0x8B, 0x51, 0x40, 0x8B, 0x45,
                    0x0C, 0x81, 0xE2, 0xFF, 0x7D, 0xA0, 0x75, 0x50,
                    0x89, 0x50, 0x10, 0x8B, 0x45, 0x08, 0x50, 0xE8,
                    0x24, 0xDA, 0x1A, 0x00, 0x5D, 0xC2, 0x08, 0x00
                }
            },
            {
                827, "", 0x007C6206,
                {
                    0x25, 0xFF, 0xFF, 0xDF, 0xFB, 0x0D, 0x00,
                    0x20, 0x00, 0x00, 0x89, 0x46, 0x40
                }
            },
            // The inherited 1566 window began at +7 and stayed unchanged
            // under the source-backed early-return patch. Read the complete
            // entry MOV instead.
            {
                1566, "", 0x00494A50,
                {0xA1, 0xC0, 0xEA, 0xCE, 0x00}
            },
            // Zzuk names this exact wall-climb constant, but its mutation is
            // disabled in the inspected source. Any negative still requires
            // the standard isolated confirmation before enforcement.
            {
                1135, "", 0x0080DFFC,
                {0xBB, 0x8D, 0x24, 0x3F}
            }
        }
    };
    return record;
}

MemCatalogRecord const& Windows6005EnGbMemRecord()
{
    static MemCatalogRecord const record
    {
        6005,
        "Win",
        "enGB",
        {
            {
                1107, "", 0x00618900,
                {
                    0x55, 0x8B, 0xEC, 0x8B, 0x51, 0x40, 0x8B, 0x45,
                    0x0C, 0x81, 0xE2, 0xFF, 0x7D, 0xA0, 0x75, 0x50,
                    0x89, 0x50, 0x10, 0x8B, 0x45, 0x08, 0x50, 0xE8,
                    0x64, 0xDA, 0x1A, 0x00, 0x5D, 0xC2, 0x08, 0x00
                }
            },
            {
                827, "", 0x007C6246,
                {
                    0x25, 0xFF, 0xFF, 0xDF, 0xFB, 0x0D, 0x00,
                    0x20, 0x00, 0x00, 0x89, 0x46, 0x40
                }
            },
            {
                1566, "", 0x00494A50,
                {0xA1, 0xC0, 0xEA, 0xCE, 0x00}
            },
            {
                1135, "", 0x0080DFFC,
                {0xBB, 0x8D, 0x24, 0x3F}
            }
        }
    };
    return record;
}

MemCatalogRecord const& Windows6141ZhCnMemRecord()
{
    static MemCatalogRecord const record
    {
        6141,
        "Win",
        "zhCN",
        {
            {
                1107, "", 0x0061ACA0,
                {
                    0x55, 0x8B, 0xEC, 0x8B, 0x51, 0x40, 0x8B, 0x45,
                    0x0C, 0x81, 0xE2, 0xFF, 0x7D, 0xA0, 0x75, 0x50,
                    0x89, 0x50, 0x10, 0x8B, 0x45, 0x08, 0x50, 0xE8,
                    0x64, 0xEB, 0x1A, 0x00, 0x5D, 0xC2, 0x08, 0x00
                }
            },
            {
                827, "", 0x007C96E6,
                {
                    0x25, 0xFF, 0xFF, 0xDF, 0xFB, 0x0D, 0x00,
                    0x20, 0x00, 0x00, 0x89, 0x46, 0x40
                }
            },
            {
                1566, "", 0x00495840,
                {0xA1, 0xE0, 0x31, 0xCF, 0x00}
            },
            {
                1135, "", 0x008121BC,
                {0xBB, 0x8D, 0x24, 0x3F}
            }
        }
    };
    return record;
}

int HexNibble(char value)
{
    if (value >= '0' && value <= '9')
        return value - '0';
    if (value >= 'A' && value <= 'F')
        return value - 'A' + 10;
    if (value >= 'a' && value <= 'f')
        return value - 'a' + 10;
    return -1;
}

bool DecodeHex(std::string const& hex, warden::Bytes& output)
{
    output.clear();
    if (hex.size() % 2 != 0)
        return false;

    output.reserve(hex.size() / 2);
    for (size_t index = 0; index < hex.size(); index += 2)
    {
        int const high = HexNibble(hex[index]);
        int const low = HexNibble(hex[index + 1]);
        if (high < 0 || low < 0)
        {
            output.clear();
            return false;
        }
        output.push_back(static_cast<uint8>((high << 4) | low));
    }
    return true;
}

std::string BytesAsString(warden::Bytes const& bytes)
{
    if (bytes.empty())
        return std::string();
    return std::string(reinterpret_cast<char const*>(bytes.data()),
        bytes.size());
}

bool IsAsciiWhitespace(uint8 value)
{
    return value == 0x09 || value == 0x0A || value == 0x0B ||
        value == 0x0C || value == 0x0D || value == 0x20;
}

bool HasNulOrAsciiWhitespace(warden::Bytes const& bytes)
{
    return std::any_of(bytes.begin(), bytes.end(), [](uint8 value)
    {
        return value == 0 || IsAsciiWhitespace(value);
    });
}

bool HasNul(warden::Bytes const& bytes)
{
    return std::find(bytes.begin(), bytes.end(), uint8(0)) != bytes.end();
}

bool SameProfile(warden::WardenProfileKey const& left,
    warden::WardenProfileKey const& right)
{
    return left.build == right.build && left.platform == right.platform &&
        left.locale == right.locale;
}

bool ProfileLess(warden::WardenProfileKey const& left,
    warden::WardenProfileKey const& right)
{
    if (left.build != right.build)
        return left.build < right.build;
    if (left.platform != right.platform)
        return left.platform < right.platform;
    return left.locale < right.locale;
}

warden::CheckCatalogValidation SetDiagnostic(
    warden::WardenCheckDiagnostic& diagnostic,
    warden::CheckCatalogValidation validation,
    warden::WardenProfileKey const& profile, uint32 checkId)
{
    diagnostic.validation = validation;
    diagnostic.profile = profile;
    diagnostic.checkId = checkId;
    return validation;
}

bool IsKnownType(uint32 type)
{
    return type == static_cast<uint32>(warden::WardenCheckType::Timing) ||
        type == static_cast<uint32>(warden::WardenCheckType::Lua) ||
        type == static_cast<uint32>(warden::WardenCheckType::Mpq) ||
        type == static_cast<uint32>(warden::WardenCheckType::Mem);
}

bool IsLegalEvidenceClass(warden::WardenCheckType type,
    warden::WardenEvidenceClass evidenceClass)
{
    switch (type)
    {
        case warden::WardenCheckType::Timing:
            return evidenceClass == warden::WardenEvidenceClass::ProtocolHealth;
        case warden::WardenCheckType::Mpq:
            return evidenceClass ==
                    warden::WardenEvidenceClass::IntegrityInvariant ||
                evidenceClass == warden::WardenEvidenceClass::Corroboration;
        case warden::WardenCheckType::Lua:
            return evidenceClass == warden::WardenEvidenceClass::Corroboration;
        case warden::WardenCheckType::Mem:
            return evidenceClass != warden::WardenEvidenceClass::ProtocolHealth;
    }
    return false;
}
}

namespace warden
{
uint32 GetWardenCheckId(WardenCheckDefinition const& definition)
{
    return std::visit([](auto const& payload)
    {
        return payload.checkId;
    }, definition.payload);
}

WardenCheckType GetWardenCheckType(WardenCheckDefinition const& definition)
{
    if (std::holds_alternative<TimingCheckProfile>(definition.payload))
        return WardenCheckType::Timing;
    if (std::holds_alternative<LuaCheckProfile>(definition.payload))
        return WardenCheckType::Lua;
    if (std::holds_alternative<MpqCheckProfile>(definition.payload))
        return WardenCheckType::Mpq;
    return WardenCheckType::Mem;
}

bool IsActionableEvidenceClass(WardenEvidenceClass evidenceClass)
{
    return evidenceClass == WardenEvidenceClass::IntegrityInvariant ||
        evidenceClass == WardenEvidenceClass::ThreatSignature;
}

bool IsConfirmationEligible(WardenCheckDefinition const& definition)
{
    return GetWardenCheckType(definition) != WardenCheckType::Timing;
}

WardenCheckProfile const* WardenCheckCatalog::Find(uint32 build,
    std::string const& platform, std::string const& locale) const
{
    for (WardenCheckProfile const& profile : m_profiles)
    {
        if (profile.key.build == build && profile.key.platform == platform &&
            profile.key.locale == locale)
            return &profile;
    }
    return nullptr;
}

std::vector<WardenCheckProfile> const& WardenCheckCatalog::Profiles() const
{
    return m_profiles;
}

uint32 WardenCheckCatalog::TotalRows() const
{
    return m_totalRows;
}

uint32 WardenCheckCatalog::EnabledRows() const
{
    return m_enabledRows;
}

CheckCatalogValidation WardenCheckCatalogBuilder::Add(
    WardenCheckRowInput const& input, WardenCheckDiagnostic& diagnostic)
{
    Bytes platformBytes;
    Bytes localeBytes;
    Bytes moduleBytes;
    Bytes requestBytes;
    Bytes expectedBytes;
    bool validHex = DecodeHex(input.platformHex, platformBytes);
    validHex &= DecodeHex(input.localeHex, localeBytes);
    validHex &= DecodeHex(input.moduleHex, moduleBytes);
    validHex &= DecodeHex(input.requestHex, requestBytes);
    validHex &= DecodeHex(input.expectedHex, expectedBytes);

    WardenProfileKey const key
    {
        input.build,
        BytesAsString(platformBytes),
        BytesAsString(localeBytes)
    };
    diagnostic = WardenCheckDiagnostic();
    diagnostic.profile = key;
    diagnostic.checkId = input.checkId;

    if (!validHex)
        return SetDiagnostic(diagnostic, CheckCatalogValidation::InvalidHex,
            key, input.checkId);
    if (!input.build || input.build > std::numeric_limits<uint16>::max())
        return SetDiagnostic(diagnostic, CheckCatalogValidation::InvalidBuild,
            key, input.checkId);
    if (platformBytes.empty() || platformBytes.size() > 4 ||
        HasNulOrAsciiWhitespace(platformBytes))
        return SetDiagnostic(diagnostic,
            CheckCatalogValidation::InvalidPlatform, key, input.checkId);
    if (localeBytes.size() != 4 || HasNulOrAsciiWhitespace(localeBytes))
        return SetDiagnostic(diagnostic, CheckCatalogValidation::InvalidLocale,
            key, input.checkId);
    if (!input.checkId)
        return SetDiagnostic(diagnostic, CheckCatalogValidation::InvalidId,
            key, input.checkId);
    if (!IsKnownType(input.type))
        return SetDiagnostic(diagnostic, CheckCatalogValidation::InvalidType,
            key, input.checkId);
    if (input.enabled > 1)
        return SetDiagnostic(diagnostic, CheckCatalogValidation::InvalidEnabled,
            key, input.checkId);
    if (input.sortOrder > std::numeric_limits<uint16>::max())
        return SetDiagnostic(diagnostic,
            CheckCatalogValidation::InvalidSortOrder, key, input.checkId);
    if (input.evidenceClass >
        static_cast<uint32>(WardenEvidenceClass::Corroboration))
        return SetDiagnostic(diagnostic,
            CheckCatalogValidation::InvalidEvidenceClass, key, input.checkId);

    WardenCheckType const type = static_cast<WardenCheckType>(input.type);
    WardenEvidenceClass const evidenceClass =
        static_cast<WardenEvidenceClass>(input.evidenceClass);
    if (!IsLegalEvidenceClass(type, evidenceClass))
        return SetDiagnostic(diagnostic,
            CheckCatalogValidation::IllegalTypeEvidenceClass,
            key, input.checkId);

    WardenCheckDefinition definition;
    definition.sortOrder = static_cast<uint16>(input.sortOrder);
    definition.evidenceClass = evidenceClass;
    switch (type)
    {
        case WardenCheckType::Timing:
            if (!moduleBytes.empty() || !requestBytes.empty() ||
                !expectedBytes.empty() || input.address || input.length)
                return SetDiagnostic(diagnostic,
                    CheckCatalogValidation::InvalidUnusedField,
                    key, input.checkId);
            definition.payload = TimingCheckProfile{input.checkId};
            break;
        case WardenCheckType::Mpq:
        {
            if (!moduleBytes.empty() || input.address || input.length)
                return SetDiagnostic(diagnostic,
                    CheckCatalogValidation::InvalidUnusedField,
                    key, input.checkId);
            if (requestBytes.empty() || requestBytes.size() > 255 ||
                HasNul(requestBytes))
                return SetDiagnostic(diagnostic,
                    CheckCatalogValidation::InvalidPath, key, input.checkId);
            if (expectedBytes.size() != Digest20().size())
                return SetDiagnostic(diagnostic,
                    CheckCatalogValidation::InvalidExpectedBytes,
                    key, input.checkId);
            Digest20 digest{};
            std::copy(expectedBytes.begin(), expectedBytes.end(),
                digest.begin());
            definition.payload = MpqCheckProfile
            {
                input.checkId,
                BytesAsString(requestBytes),
                digest
            };
            break;
        }
        case WardenCheckType::Lua:
            if (!moduleBytes.empty() || input.address || input.length)
                return SetDiagnostic(diagnostic,
                    CheckCatalogValidation::InvalidUnusedField,
                    key, input.checkId);
            if (requestBytes.empty() || requestBytes.size() > 255 ||
                HasNul(requestBytes))
                return SetDiagnostic(diagnostic,
                    CheckCatalogValidation::InvalidQuery, key, input.checkId);
            if (expectedBytes.empty() || expectedBytes.size() > 64 ||
                HasNul(expectedBytes))
                return SetDiagnostic(diagnostic,
                    CheckCatalogValidation::InvalidExpectedText,
                    key, input.checkId);
            definition.payload = LuaCheckProfile
            {
                input.checkId,
                BytesAsString(requestBytes),
                BytesAsString(expectedBytes)
            };
            break;
        case WardenCheckType::Mem:
            if (!input.address)
                return SetDiagnostic(diagnostic,
                    CheckCatalogValidation::InvalidAddress,
                    key, input.checkId);
            if (moduleBytes.size() > 255 || HasNul(moduleBytes))
                return SetDiagnostic(diagnostic,
                    CheckCatalogValidation::InvalidModuleName,
                    key, input.checkId);
            if (!input.length || input.length > 255)
                return SetDiagnostic(diagnostic,
                    CheckCatalogValidation::InvalidLength,
                    key, input.checkId);
            if (!requestBytes.empty())
                return SetDiagnostic(diagnostic,
                    CheckCatalogValidation::InvalidUnusedField,
                    key, input.checkId);
            if (expectedBytes.size() != input.length)
                return SetDiagnostic(diagnostic,
                    CheckCatalogValidation::InvalidExpectedBytes,
                    key, input.checkId);
            definition.payload = MemCheckProfile
            {
                input.checkId,
                BytesAsString(moduleBytes),
                input.address,
                expectedBytes
            };
            break;
    }

    PendingRow row;
    row.key = key;
    row.checkId = input.checkId;
    row.enabled = input.enabled != 0;
    row.definition = std::move(definition);
    m_rows.push_back(std::move(row));
    return SetDiagnostic(diagnostic, CheckCatalogValidation::Valid,
        key, input.checkId);
}

CheckCatalogValidation WardenCheckCatalogBuilder::Build(
    WardenCheckCatalog& output, WardenCheckDiagnostic& diagnostic)
{
    diagnostic = WardenCheckDiagnostic();
    if (m_rows.empty())
        return SetDiagnostic(diagnostic, CheckCatalogValidation::EmptyCatalog,
            WardenProfileKey(), 0);

    std::vector<PendingRow> rows = m_rows;
    std::sort(rows.begin(), rows.end(),
        [](PendingRow const& left, PendingRow const& right)
        {
            if (ProfileLess(left.key, right.key))
                return true;
            if (ProfileLess(right.key, left.key))
                return false;
            if (left.definition.sortOrder != right.definition.sortOrder)
                return left.definition.sortOrder < right.definition.sortOrder;
            return left.checkId < right.checkId;
        });

    WardenCheckCatalog candidate;
    size_t begin = 0;
    while (begin < rows.size())
    {
        size_t end = begin + 1;
        while (end < rows.size() && SameProfile(rows[begin].key, rows[end].key))
            ++end;

        WardenCheckProfile profile;
        profile.key = rows[begin].key;
        profile.totalRows = static_cast<uint32>(end - begin);
        std::set<uint32> checkIds;
        std::set<uint16> sortOrders;
        uint32 timingCount = 0;
        bool timingEnabled = false;
        bool hasEnabledNonHealth = false;
        uint32 timingCheckId = 0;

        for (size_t index = begin; index < end; ++index)
        {
            PendingRow const& row = rows[index];
            if (!checkIds.insert(row.checkId).second)
                return SetDiagnostic(diagnostic,
                    CheckCatalogValidation::DuplicateId,
                    row.key, row.checkId);
            if (!sortOrders.insert(row.definition.sortOrder).second)
                return SetDiagnostic(diagnostic,
                    CheckCatalogValidation::DuplicateSortOrder,
                    row.key, row.checkId);

            WardenCheckType const type = GetWardenCheckType(row.definition);
            if (type == WardenCheckType::Timing)
            {
                ++timingCount;
                timingCheckId = row.checkId;
                timingEnabled = timingEnabled || row.enabled;
            }
            else if (row.enabled)
            {
                hasEnabledNonHealth = true;
            }

            if (row.enabled)
            {
                profile.checks.push_back(row.definition);
                ++candidate.m_enabledRows;
            }
        }

        if (!timingCount)
            return SetDiagnostic(diagnostic,
                CheckCatalogValidation::MissingTiming, profile.key, 0);
        if (timingCount > 1)
            return SetDiagnostic(diagnostic,
                CheckCatalogValidation::MultipleTiming,
                profile.key, timingCheckId);
        if (!timingEnabled)
            return SetDiagnostic(diagnostic,
                CheckCatalogValidation::DisabledTiming,
                profile.key, timingCheckId);
        if (!hasEnabledNonHealth)
            return SetDiagnostic(diagnostic,
                CheckCatalogValidation::MissingNonHealth, profile.key, 0);

        profile.hasActionableChecks = std::any_of(profile.checks.begin(),
            profile.checks.end(), [](WardenCheckDefinition const& definition)
            {
                return IsActionableEvidenceClass(definition.evidenceClass);
            });
        candidate.m_totalRows += profile.totalRows;
        candidate.m_profiles.push_back(std::move(profile));
        begin = end;
    }

    output = std::move(candidate);
    diagnostic = WardenCheckDiagnostic();
    return CheckCatalogValidation::Valid;
}

char const* ToString(CheckCatalogValidation validation)
{
    switch (validation)
    {
        case CheckCatalogValidation::Valid: return "Valid";
        case CheckCatalogValidation::InvalidHex: return "InvalidHex";
        case CheckCatalogValidation::InvalidBuild: return "InvalidBuild";
        case CheckCatalogValidation::InvalidPlatform: return "InvalidPlatform";
        case CheckCatalogValidation::InvalidLocale: return "InvalidLocale";
        case CheckCatalogValidation::InvalidId: return "InvalidId";
        case CheckCatalogValidation::InvalidType: return "InvalidType";
        case CheckCatalogValidation::InvalidEnabled: return "InvalidEnabled";
        case CheckCatalogValidation::InvalidSortOrder: return "InvalidSortOrder";
        case CheckCatalogValidation::InvalidEvidenceClass: return "InvalidEvidenceClass";
        case CheckCatalogValidation::IllegalTypeEvidenceClass: return "IllegalTypeEvidenceClass";
        case CheckCatalogValidation::InvalidUnusedField: return "InvalidUnusedField";
        case CheckCatalogValidation::InvalidPath: return "InvalidPath";
        case CheckCatalogValidation::InvalidQuery: return "InvalidQuery";
        case CheckCatalogValidation::InvalidExpectedText: return "InvalidExpectedText";
        case CheckCatalogValidation::InvalidAddress: return "InvalidAddress";
        case CheckCatalogValidation::InvalidModuleName: return "InvalidModuleName";
        case CheckCatalogValidation::InvalidLength: return "InvalidLength";
        case CheckCatalogValidation::InvalidExpectedBytes: return "InvalidExpectedBytes";
        case CheckCatalogValidation::DuplicateId: return "DuplicateId";
        case CheckCatalogValidation::DuplicateSortOrder: return "DuplicateSortOrder";
        case CheckCatalogValidation::EmptyCatalog: return "EmptyCatalog";
        case CheckCatalogValidation::MissingTiming: return "MissingTiming";
        case CheckCatalogValidation::MultipleTiming: return "MultipleTiming";
        case CheckCatalogValidation::DisabledTiming: return "DisabledTiming";
        case CheckCatalogValidation::MissingNonHealth: return "MissingNonHealth";
    }
    return "Unknown";
}

MpqCheckProfile const* WardenCheckCatalog::FindMpq(uint32 build,
    std::string const& platform, std::string const& locale) const
{
    std::array<MpqCatalogRecord const*, 3> const records =
    {
        &Windows5875EnUsMpqRecord(),
        &Windows6005EnGbMpqRecord(),
        &Windows6141ZhCnMpqRecord()
    };
    for (MpqCatalogRecord const* record : records)
    {
        if (record->build == build && platform == record->platform &&
            locale == record->locale && Validate(record->profile) ==
                CheckCatalogValidation::Valid)
            return &record->profile;
    }
    return nullptr;
}

LuaCheckProfile const* WardenCheckCatalog::FindLua(uint32 build,
    std::string const& platform, std::string const& locale) const
{
    std::array<LuaCatalogRecord const*, 3> const records =
    {
        &Windows5875EnUsLuaRecord(),
        &Windows6005EnGbLuaRecord(),
        &Windows6141ZhCnLuaRecord()
    };
    for (LuaCatalogRecord const* record : records)
    {
        if (record->build == build && platform == record->platform &&
            locale == record->locale && Validate(record->profile) ==
                CheckCatalogValidation::Valid)
            return &record->profile;
    }
    return nullptr;
}

std::vector<MemCheckProfile> const* WardenCheckCatalog::FindMem(uint32 build,
    std::string const& platform, std::string const& locale) const
{
    std::array<MemCatalogRecord const*, 3> const records =
    {
        &Windows5875EnUsMemRecord(),
        &Windows6005EnGbMemRecord(),
        &Windows6141ZhCnMemRecord()
    };
    for (MemCatalogRecord const* record : records)
    {
        if (record->build == build && platform == record->platform &&
            locale == record->locale &&
            Validate(record->profiles) == CheckCatalogValidation::Valid)
            return &record->profiles;
    }
    return nullptr;
}

CheckCatalogValidation WardenCheckCatalog::Validate(
    MpqCheckProfile const& profile) const
{
    if (!profile.checkId)
        return CheckCatalogValidation::InvalidId;
    if (profile.path.empty() || profile.path.size() > 255 ||
        profile.path.find('\0') != std::string::npos)
        return CheckCatalogValidation::InvalidPath;
    return CheckCatalogValidation::Valid;
}

CheckCatalogValidation WardenCheckCatalog::Validate(
    LuaCheckProfile const& profile) const
{
    if (!profile.checkId)
        return CheckCatalogValidation::InvalidId;
    if (profile.query.empty() || profile.query.size() > 255 ||
        profile.query.find('\0') != std::string::npos)
        return CheckCatalogValidation::InvalidQuery;

    // The exact delivered module truncates callback output at byte 64.
    if (profile.expectedText.size() > 64 ||
        profile.expectedText.find('\0') != std::string::npos)
        return CheckCatalogValidation::InvalidExpectedText;
    return CheckCatalogValidation::Valid;
}

CheckCatalogValidation WardenCheckCatalog::Validate(
    MemCheckProfile const& profile) const
{
    if (!profile.checkId)
        return CheckCatalogValidation::InvalidId;
    if (!profile.addressOrRva)
        return CheckCatalogValidation::InvalidAddress;
    if (profile.moduleName.size() > std::numeric_limits<uint8>::max() ||
        profile.moduleName.find('\0') != std::string::npos)
        return CheckCatalogValidation::InvalidModuleName;
    if (profile.expectedBytes.empty() ||
        profile.expectedBytes.size() > std::numeric_limits<uint8>::max())
        return CheckCatalogValidation::InvalidExpectedBytes;
    return CheckCatalogValidation::Valid;
}

CheckCatalogValidation WardenCheckCatalog::Validate(
    std::vector<MemCheckProfile> const& profiles) const
{
    if (profiles.empty())
        return CheckCatalogValidation::InvalidId;

    for (size_t index = 0; index < profiles.size(); ++index)
    {
        CheckCatalogValidation const validation = Validate(profiles[index]);
        if (validation != CheckCatalogValidation::Valid)
            return validation;
        for (size_t prior = 0; prior < index; ++prior)
        {
            if (profiles[prior].checkId == profiles[index].checkId)
                return CheckCatalogValidation::DuplicateId;
        }
    }
    return CheckCatalogValidation::Valid;
}
}
