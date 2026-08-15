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

#include "WardenIncidentStore.h"

#include "Database/DatabaseEnv.h"

#include <memory>
#include <utility>

namespace warden
{
WardenIncidentStore& WardenIncidentStore::Instance()
{
    static WardenIncidentStore instance;
    return instance;
}

std::optional<WardenIncidentWindowState> WardenIncidentStore::Load(
    uint32 accountId, uint32 incidentWindowSeconds,
    uint32 aggressiveThreshold) const
{
    if (accountId == 0 || incidentWindowSeconds == 0 ||
        aggressiveThreshold == 0)
    {
        return std::nullopt;
    }

    // The NULL sentinel makes an empty successful query distinguishable from
    // a database failure. One statement clock also lets mangosd preserve the
    // remaining aggressive duration without comparing host wall clocks.
    std::unique_ptr<QueryResult> result(LoginDatabase.PQuery(
        "SELECT `recent`.`occurred_at`, "
        "UNIX_TIMESTAMP() AS `database_now` FROM ("
        "SELECT `occurred_at` FROM `warden_incident` "
        "WHERE `account_id` = %u "
        "AND `occurred_at` > UNIX_TIMESTAMP() - %u "
        "UNION ALL SELECT NULL AS `occurred_at`"
        ") AS `recent` "
        "ORDER BY `recent`.`occurred_at` IS NULL, "
        "`recent`.`occurred_at` DESC",
        accountId, incidentWindowSeconds));
    if (!result)
        return std::nullopt;

    std::vector<uint64> timestamps;
    uint64 databaseNow = 0;
    do
    {
        Field const* fields = result->Fetch();
        databaseNow = fields[1].GetUInt64();
        if (!fields[0].IsNULL())
            timestamps.push_back(fields[0].GetUInt64());
    }
    while (result->NextRow());

    if (databaseNow == 0)
        return std::nullopt;

    // SQL already applied the exclusive window with the database clock. Do
    // not filter a second time with the potentially different host clock.
    WardenIncidentWindowState state =
        detail::ClassifyRecentIncidentWindow(std::move(timestamps),
            incidentWindowSeconds, aggressiveThreshold);
    state.databaseNow = databaseNow;
    return state;
}

WardenIncidentWriteResult WardenIncidentStore::Record(
    WardenIncidentContext const& context,
    WardenConfiguration const& configuration) const
{
    WardenIncidentWriteResult failed;
    if (configuration.enforcementMode == WardenEnforcementMode::Observe ||
        context.accountId == 0 || context.clientBuild > 0xFFFFu ||
        context.clientLocale.size() != 4u || context.checkId == 0)
    {
        return failed;
    }

    std::string safeLocale = context.clientLocale;
    LoginDatabase.escape_string(safeLocale);

    if (!LoginDatabase.BeginTransaction())
        return failed;

    bool queued = LoginDatabase.PExecute(
        "UPDATE `account` "
        "SET `active_realm_id` = `active_realm_id` "
        "WHERE `id` = %u",
        context.accountId);
    queued = queued && LoginDatabase.PExecute(
        "INSERT INTO `warden_incident` "
        "(`account_id`,`occurred_at`,`realm_id`,`client_build`,"
        "`client_locale`,`check_id`,`outcome`,`ban_triggered`) "
        "VALUES (%u,UNIX_TIMESTAMP(),%u,%u,'%s',%u,%u,0)",
        context.accountId, context.realmId, context.clientBuild,
        safeLocale.c_str(), context.checkId, uint32(context.outcome));

    if (queued && configuration.enforcementMode ==
        WardenEnforcementMode::KickAndBan)
    {
        queued = LoginDatabase.PExecute(
            "UPDATE `warden_incident` AS `wi` "
            "JOIN ("
            "SELECT `recent_count` FROM ("
            "SELECT COUNT(*) AS `recent_count` "
            "FROM `warden_incident` "
            "WHERE `account_id` = %u "
            "AND `occurred_at` > UNIX_TIMESTAMP() - %u"
            ") AS `materialized_count`"
            ") AS `recent` "
            "SET `wi`.`ban_triggered` = 1 "
            "WHERE `wi`.`incident_id` = LAST_INSERT_ID() "
            "AND `recent`.`recent_count` >= %u "
            "AND NOT EXISTS ("
            "SELECT 1 FROM `account_banned` "
            "WHERE `id` = %u AND `active` = 1 "
            "AND `bandate` = `unbandate`"
            ")",
            context.accountId, configuration.incidentWindowSeconds,
            configuration.banThreshold, context.accountId);
        queued = queued && LoginDatabase.PExecute(
            "INSERT INTO `account_banned` "
            "(`id`,`bandate`,`unbandate`,`bannedby`,`banreason`,`active`) "
            "SELECT `account_id`,`occurred_at`,`occurred_at`,"
            "'MaNGOS Warden',"
            "'Repeated confirmed Warden memory violations',1 "
            "FROM `warden_incident` "
            "WHERE `incident_id` = LAST_INSERT_ID() "
            "AND `ban_triggered` = 1");
    }

    if (!queued)
    {
        LoginDatabase.RollbackTransaction();
        return failed;
    }

    if (!LoginDatabase.CommitTransactionChecked())
        return failed;

    std::optional<WardenIncidentWindowState> const after = Load(
        context.accountId, configuration.incidentWindowSeconds,
        configuration.aggressiveThreshold);
    std::unique_ptr<QueryResult> permanentBan(LoginDatabase.PQuery(
        "SELECT EXISTS("
        "SELECT 1 FROM `account_banned` "
        "WHERE `id` = %u AND `active` = 1 "
        "AND `bandate` = `unbandate` "
        "AND `bannedby` = 'MaNGOS Warden'"
        ") AS `permanent_ban_active`",
        context.accountId));
    if (!after || !permanentBan)
    {
        WardenIncidentWriteResult unknown;
        unknown.status =
            WardenIncidentWriteStatus::CommittedStateUnavailable;
        return unknown;
    }

    WardenIncidentWriteResult committed;
    committed.status = WardenIncidentWriteStatus::Committed;
    committed.recentCount = after->recentCount;
    committed.permanentBanActive = permanentBan->Fetch()[0].GetBool();
    return committed;
}
}
