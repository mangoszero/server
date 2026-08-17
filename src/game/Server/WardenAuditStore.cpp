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

#include "WardenAuditStore.h"

#include "Database/DatabaseEnv.h"

namespace warden
{
WardenAuditStore& WardenAuditStore::Instance()
{
    static WardenAuditStore instance;
    return instance;
}

bool WardenAuditStore::Record(WardenAuditContext const& context) const
{
    if (!IsValidWardenAuditContext(context))
        return false;

    std::string safePlatform = context.clientPlatform;
    std::string safeLocale = context.clientLocale;
    LoginDatabase.escape_string(safePlatform);
    LoginDatabase.escape_string(safeLocale);
    return LoginDatabase.PExecute(
        "INSERT INTO `warden_audit` "
        "(`account_id`,`occurred_at`,`realm_id`,`client_build`,"
        "`client_platform`,`client_locale`,`check_id`,`check_type`,"
        "`evidence_class`,`outcome`) "
        "VALUES (%u,UNIX_TIMESTAMP(),%u,%u,'%s','%s',%u,%u,%u,%u)",
        context.accountId, context.realmId, context.clientBuild,
        safePlatform.c_str(), safeLocale.c_str(), context.checkId,
        uint32(context.checkType), uint32(context.evidenceClass),
        uint32(context.outcome));
}
}
