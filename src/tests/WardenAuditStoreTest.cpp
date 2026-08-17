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

#include "TestHarness.h"

#include "WardenAuditStore.h"

#include <optional>

namespace
{
warden::WardenAuditContext ValidContext()
{
    warden::WardenAuditContext context;
    context.accountId = 6;
    context.realmId = 1;
    context.clientBuild = 5875;
    context.clientPlatform = "Win";
    context.clientLocale = "enUS";
    context.checkId = 1;
    context.checkType = warden::WardenCheckType::Mpq;
    context.evidenceClass = warden::WardenEvidenceClass::Corroboration;
    context.outcome = warden::WardenAuditOutcome::Mismatch;
    return context;
}
}

TEST(WardenAuditOutcome_accepts_only_mismatch_and_unavailable)
{
    CHECK(!warden::ToAuditOutcome(
        warden::WardenCheckOutcome::Match).has_value());
    CHECK(!warden::ToAuditOutcome(
        warden::WardenCheckOutcome::Stable).has_value());
    CHECK(!warden::ToAuditOutcome(
        warden::WardenCheckOutcome::Unstable).has_value());

    std::optional<warden::WardenAuditOutcome> mismatch =
        warden::ToAuditOutcome(warden::WardenCheckOutcome::Mismatch);
    REQUIRE(mismatch.has_value());
    CHECK_EQ(uint32(*mismatch), uint32(1));
    std::optional<warden::WardenAuditOutcome> unavailable =
        warden::ToAuditOutcome(warden::WardenCheckOutcome::Unavailable);
    REQUIRE(unavailable.has_value());
    CHECK_EQ(uint32(*unavailable), uint32(2));
}

TEST(WardenAuditContext_accepts_complete_legal_nonactionable_evidence)
{
    warden::WardenAuditContext context = ValidContext();
    CHECK(warden::IsValidWardenAuditContext(context));
    context.checkType = warden::WardenCheckType::Lua;
    CHECK(warden::IsValidWardenAuditContext(context));
    context.checkType = warden::WardenCheckType::Mem;
    context.evidenceClass = warden::WardenEvidenceClass::ThreatSignature;
    context.outcome = warden::WardenAuditOutcome::Unavailable;
    CHECK(warden::IsValidWardenAuditContext(context));
}

TEST(WardenAuditContext_rejects_invalid_identity_tokens_and_enums)
{
    warden::WardenAuditContext context = ValidContext();
    context.accountId = 0;
    CHECK(!warden::IsValidWardenAuditContext(context));
    context = ValidContext();
    context.checkId = 0;
    CHECK(!warden::IsValidWardenAuditContext(context));
    context = ValidContext();
    context.clientBuild = 65536;
    CHECK(!warden::IsValidWardenAuditContext(context));

    context = ValidContext();
    context.clientPlatform.clear();
    CHECK(!warden::IsValidWardenAuditContext(context));
    context.clientPlatform.assign(5, 'A');
    CHECK(!warden::IsValidWardenAuditContext(context));
    context.clientPlatform.assign("W\0in", 4);
    CHECK(!warden::IsValidWardenAuditContext(context));
    context.clientPlatform = "W in";
    CHECK(!warden::IsValidWardenAuditContext(context));

    context = ValidContext();
    context.clientLocale = "enU";
    CHECK(!warden::IsValidWardenAuditContext(context));
    context.clientLocale.assign("e\0US", 4);
    CHECK(!warden::IsValidWardenAuditContext(context));
    context.clientLocale = "en S";
    CHECK(!warden::IsValidWardenAuditContext(context));

    context = ValidContext();
    context.checkType = warden::WardenCheckType::Timing;
    CHECK(!warden::IsValidWardenAuditContext(context));
    context = ValidContext();
    context.evidenceClass = warden::WardenEvidenceClass::ProtocolHealth;
    CHECK(!warden::IsValidWardenAuditContext(context));
    context = ValidContext();
    context.evidenceClass = warden::WardenEvidenceClass::ThreatSignature;
    CHECK(!warden::IsValidWardenAuditContext(context));
    context = ValidContext();
    context.checkType = static_cast<warden::WardenCheckType>(0xFF);
    CHECK(!warden::IsValidWardenAuditContext(context));
    context = ValidContext();
    context.evidenceClass =
        static_cast<warden::WardenEvidenceClass>(0xFF);
    CHECK(!warden::IsValidWardenAuditContext(context));
    context = ValidContext();
    context.outcome = static_cast<warden::WardenAuditOutcome>(0);
    CHECK(!warden::IsValidWardenAuditContext(context));
    context.outcome = static_cast<warden::WardenAuditOutcome>(3);
    CHECK(!warden::IsValidWardenAuditContext(context));
}
