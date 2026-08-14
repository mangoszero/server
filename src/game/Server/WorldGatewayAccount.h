/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
 */

#ifndef MANGOS_H_WORLDGATEWAY_ACCOUNT
#define MANGOS_H_WORLDGATEWAY_ACCOUNT

#include <string>

class Field;

enum class AccountRestriction
{
    None,
    Banned,
    LockedAddressMismatch
};

AccountRestriction EvaluateAccountRestriction(
    Field const* fields, std::string const& peerAddress);

#endif
