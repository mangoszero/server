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

#include "WardenCheckCatalogLoader.h"

#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "WardenCheckPlanner.h"
#include "WardenManager.h"
#include "WardenPacketCodec.h"

#include <array>
#include <limits>
#include <memory>
#include <string>

namespace
{
std::string SafeToken(std::string const& value)
{
    if (value.empty())
        return "<unavailable>";
    for (unsigned char byte : value)
    {
        bool const alphaNumeric = (byte >= '0' && byte <= '9') ||
            (byte >= 'A' && byte <= 'Z') ||
            (byte >= 'a' && byte <= 'z');
        if (!alphaNumeric)
            return "<invalid>";
    }
    return value;
}

std::string SqlHex(Field const& field)
{
    char const* value = field.GetString();
    return value ? std::string(value) : std::string();
}

char const* PurposeName(warden::CheckPlanPurpose purpose)
{
    switch (purpose)
    {
        case warden::CheckPlanPurpose::Initial: return "Initial";
        case warden::CheckPlanPurpose::Recurring: return "Recurring";
        case warden::CheckPlanPurpose::AggressiveImmediate:
            return "AggressiveImmediate";
        case warden::CheckPlanPurpose::AggressiveRecurring:
            return "AggressiveRecurring";
        case warden::CheckPlanPurpose::Confirmation: return "Confirmation";
    }
    return "Unknown";
}

void LogLoadFailure(warden::WardenCheckCatalogLoadFailure failure)
{
    sLog.outError("Warden catalogue load failed: %s.",
        warden::ToString(failure));
}

void LogLoadFailure(warden::WardenCheckCatalogLoadFailure failure,
    warden::WardenCheckDiagnostic const& diagnostic)
{
    std::string const platform = SafeToken(diagnostic.profile.platform);
    std::string const locale = SafeToken(diagnostic.profile.locale);
    sLog.outError("Warden catalogue load failed: %s (build %u; platform %s; "
        "locale %s; check %u; validation %s).", warden::ToString(failure),
        diagnostic.profile.build, platform.c_str(), locale.c_str(),
        diagnostic.checkId, warden::ToString(diagnostic.validation));
}
}

namespace warden
{
char const* ToString(WardenCheckCatalogLoadFailure failure)
{
    switch (failure)
    {
        case WardenCheckCatalogLoadFailure::None: return "None";
        case WardenCheckCatalogLoadFailure::CatalogueQueryFailed:
            return "CatalogueQueryFailed";
        case WardenCheckCatalogLoadFailure::EmptyCatalogue:
            return "EmptyCatalogue";
        case WardenCheckCatalogLoadFailure::InvalidRow: return "InvalidRow";
        case WardenCheckCatalogLoadFailure::ProfileWithoutModule:
            return "ProfileWithoutModule";
        case WardenCheckCatalogLoadFailure::ModuleWithoutProfile:
            return "ModuleWithoutProfile";
        case WardenCheckCatalogLoadFailure::InvalidPlan: return "InvalidPlan";
        case WardenCheckCatalogLoadFailure::PublicationFailed:
            return "PublicationFailed";
    }
    return "Unknown";
}

bool WardenCheckCatalogLoader::LoadAndPublish() const
{
    std::unique_ptr<QueryResult> count(WorldDatabase.Query(
        "SELECT COUNT(*) FROM `warden_checks`"));
    if (!count)
    {
        LogLoadFailure(WardenCheckCatalogLoadFailure::CatalogueQueryFailed);
        return false;
    }
    uint64 const expectedRows = count->Fetch()[0].GetUInt64();
    if (!expectedRows)
    {
        LogLoadFailure(WardenCheckCatalogLoadFailure::EmptyCatalogue);
        return false;
    }
    if (expectedRows > std::numeric_limits<uint32>::max())
    {
        LogLoadFailure(WardenCheckCatalogLoadFailure::InvalidRow);
        return false;
    }

    std::unique_ptr<QueryResult> result(WorldDatabase.Query(
        "SELECT `build`, HEX(`platform`), HEX(`locale`), `check_id`, `type`, "
        "`enabled`, `sort_order`, `evidence_class`, HEX(`module`), "
        "`address`, `length`, HEX(`request`), HEX(`expected`) "
        "FROM `warden_checks` "
        "ORDER BY `build`, `platform`, `locale`, `sort_order`, `check_id`"));
    if (!result)
    {
        LogLoadFailure(WardenCheckCatalogLoadFailure::CatalogueQueryFailed);
        return false;
    }

    WardenCheckCatalogBuilder builder;
    WardenCheckDiagnostic diagnostic;
    uint64 loadedRows = 0;
    do
    {
        Field const* fields = result->Fetch();
        WardenCheckRowInput input;
        input.build = fields[0].GetUInt32();
        input.platformHex = SqlHex(fields[1]);
        input.localeHex = SqlHex(fields[2]);
        input.checkId = fields[3].GetUInt32();
        input.type = fields[4].GetUInt32();
        input.enabled = fields[5].GetUInt32();
        input.sortOrder = fields[6].GetUInt32();
        input.evidenceClass = fields[7].GetUInt32();
        input.moduleHex = SqlHex(fields[8]);
        input.address = fields[9].GetUInt32();
        input.length = fields[10].GetUInt32();
        input.requestHex = SqlHex(fields[11]);
        input.expectedHex = SqlHex(fields[12]);
        if (builder.Add(input, diagnostic) != CheckCatalogValidation::Valid)
        {
            LogLoadFailure(WardenCheckCatalogLoadFailure::InvalidRow,
                diagnostic);
            return false;
        }
        ++loadedRows;
    }
    while (result->NextRow());

    if (loadedRows != expectedRows)
    {
        LogLoadFailure(WardenCheckCatalogLoadFailure::CatalogueQueryFailed);
        return false;
    }

    WardenCheckCatalog candidate;
    if (builder.Build(candidate, diagnostic) != CheckCatalogValidation::Valid)
    {
        LogLoadFailure(WardenCheckCatalogLoadFailure::InvalidRow, diagnostic);
        return false;
    }

    WardenModuleCatalog modules;
    WardenCheckCatalogLoadFailure const coverage =
        ValidateWardenCatalogCoverage(candidate, modules);
    if (coverage != WardenCheckCatalogLoadFailure::None)
    {
        LogLoadFailure(coverage);
        return false;
    }

    for (WardenCheckProfile const& profile : candidate.Profiles())
    {
        std::string const platform = SafeToken(profile.key.platform);
        std::string const locale = SafeToken(profile.key.locale);
        std::vector<CheckPlan> const plans =
            BuildWardenPreflightPlans(profile);
        if (plans.empty())
        {
            LogLoadFailure(WardenCheckCatalogLoadFailure::InvalidPlan);
            return false;
        }
        for (CheckPlan const& plan : plans)
        {
            WardenCheckPlanBudget budget;
            CheckPlanValidation const validation =
                InspectCheckPlan(plan, budget);
            if (validation != CheckPlanValidation::Valid)
            {
                sLog.outError("Warden catalogue load failed: %s (build %u; "
                    "platform %s; locale %s; purpose %s; validation %s).",
                    ToString(WardenCheckCatalogLoadFailure::InvalidPlan),
                    profile.key.build, platform.c_str(), locale.c_str(),
                    PurposeName(plan.purpose),
                    ToString(validation));
                return false;
            }
        }
    }

    auto mutableSnapshot =
        std::make_shared<WardenCheckCatalog>(std::move(candidate));
    std::shared_ptr<WardenCheckCatalog const> snapshot = mutableSnapshot;
    if (!WardenManager::Instance().PublishCheckCatalog(snapshot))
    {
        LogLoadFailure(WardenCheckCatalogLoadFailure::PublicationFailed);
        return false;
    }

    sLog.outString("Warden catalogue loaded: %u total rows, %u enabled rows, "
        "%u profiles.", snapshot->TotalRows(), snapshot->EnabledRows(),
        uint32(snapshot->Profiles().size()));
    for (WardenCheckProfile const& profile : snapshot->Profiles())
    {
        std::string const platform = SafeToken(profile.key.platform);
        std::string const locale = SafeToken(profile.key.locale);
        std::array<uint32, 4> typeCounts{};
        std::array<uint32, 4> classCounts{};
        for (WardenCheckDefinition const& definition : profile.checks)
        {
            switch (GetWardenCheckType(definition))
            {
                case WardenCheckType::Timing: ++typeCounts[0]; break;
                case WardenCheckType::Mpq: ++typeCounts[1]; break;
                case WardenCheckType::Lua: ++typeCounts[2]; break;
                case WardenCheckType::Mem: ++typeCounts[3]; break;
            }
            ++classCounts[uint8(definition.evidenceClass)];
        }
        sLog.outString("Warden profile %u/%s/%s: Timing %u, MPQ %u, Lua %u, "
            "MEM %u; ProtocolHealth %u, IntegrityInvariant %u, "
            "ThreatSignature %u, Corroboration %u.", profile.key.build,
            platform.c_str(), locale.c_str(),
            typeCounts[0], typeCounts[1], typeCounts[2], typeCounts[3],
            classCounts[0], classCounts[1], classCounts[2], classCounts[3]);
        if (!profile.hasActionableChecks)
        {
            sLog.outError("Warden profile %u/%s/%s is observation-only and "
                "cannot create incidents.", profile.key.build,
                platform.c_str(), locale.c_str());
        }
    }
    return true;
}
}
