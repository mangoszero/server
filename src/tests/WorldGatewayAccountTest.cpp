/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
 */

#include "TestHarness.h"
#include "Database/Field.h"
#include "WorldGatewayAccount.h"

namespace
{
template <size_t Size>
void SetClearRow(Field (&fields)[Size])
{
    for (Field& field : fields)
    {
        field.SetValue("0");
    }
    fields[3].SetValue("192.0.2.10");
}

TEST(WorldGatewayAccount_warden_platform_hint_is_appended_at_eleven)
{
    Field fields[12];
    SetClearRow(fields);
    fields[11].SetValue("Win");
    CHECK_STR(ReadWardenPlatformHint(fields), "Win");
}

TEST(WorldGatewayAccount_warden_client_locale_is_appended_at_twelve)
{
    Field fields[13];
    SetClearRow(fields);

    fields[12].SetValue("enGB");
    CHECK_STR(ReadWardenClientLocale(fields), "enGB");

    fields[12].SetValue("zhCN");
    CHECK_STR(ReadWardenClientLocale(fields), "zhCN");
}

TEST(WorldGatewayAccount_invalid_warden_client_locale_is_not_promoted)
{
    Field fields[13];
    SetClearRow(fields);

    fields[12].SetValue("");
    CHECK_STR(ReadWardenClientLocale(fields), "");

    fields[12].SetValue("enUK");
    CHECK_STR(ReadWardenClientLocale(fields), "");

    fields[12].SetValue("enGB-extra");
    CHECK_STR(ReadWardenClientLocale(fields), "");

    fields[12].SetValue("ruRU");
    CHECK_STR(ReadWardenClientLocale(fields), "");
}
}

TEST(WorldGatewayAccount_account_ban_is_post_strip_field_nine)
{
    Field fields[11];
    SetClearRow(fields);
    fields[9].SetValue("1");
    CHECK_EQ(int(EvaluateAccountRestriction(fields, "192.0.2.10")),
             int(AccountRestriction::Banned));
}

TEST(WorldGatewayAccount_ip_ban_is_post_strip_field_ten)
{
    Field fields[11];
    SetClearRow(fields);
    fields[10].SetValue("1");
    CHECK_EQ(int(EvaluateAccountRestriction(fields, "192.0.2.10")),
             int(AccountRestriction::Banned));
}

TEST(WorldGatewayAccount_locked_account_rejects_a_different_address)
{
    Field fields[11];
    SetClearRow(fields);
    fields[4].SetValue("1");
    CHECK_EQ(int(EvaluateAccountRestriction(fields, "198.51.100.20")),
             int(AccountRestriction::LockedAddressMismatch));
    CHECK_EQ(int(EvaluateAccountRestriction(fields, "192.0.2.10")),
             int(AccountRestriction::None));
}
