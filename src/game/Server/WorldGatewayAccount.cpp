/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
 */

#include "WorldGatewayAccount.h"
#include "Common/Locales.h"
#include "Database/Field.h"

namespace
{
enum AccountFieldIndex
{
    ACCOUNT_ID = 0,
    ACCOUNT_SECURITY = 1,
    ACCOUNT_SESSION_KEY = 2,
    ACCOUNT_LAST_IP = 3,
    ACCOUNT_LOCKED = 4,
    ACCOUNT_VERIFIER = 5,
    ACCOUNT_SALT = 6,
    ACCOUNT_MUTE_TIME = 7,
    ACCOUNT_LOCALE = 8,
    ACCOUNT_BANNED = 9,
    IP_BANNED = 10,
    WARDEN_PLATFORM = 11,
    WARDEN_CLIENT_LOCALE = 12,
    ACCOUNT_FIELD_COUNT = 13
};
}

AccountRestriction EvaluateAccountRestriction(
    Field const* fields, std::string const& peerAddress)
{
    if (fields[ACCOUNT_BANNED].GetUInt32() || fields[IP_BANNED].GetUInt32())
    {
        return AccountRestriction::Banned;
    }

    if (fields[ACCOUNT_LOCKED].GetBool()
        && fields[ACCOUNT_LAST_IP].GetCppString() != peerAddress)
    {
        return AccountRestriction::LockedAddressMismatch;
    }

    return AccountRestriction::None;
}

std::string ReadWardenPlatformHint(Field const* fields)
{
    return fields[WARDEN_PLATFORM].GetCppString();
}

std::string ReadWardenClientLocale(Field const* fields)
{
    char const* exactLocale =
        GetExactLocaleName(fields[WARDEN_CLIENT_LOCALE].GetCppString());
    return exactLocale ? exactLocale : "";
}
