/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2025 MaNGOS <https://www.getmangos.eu>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include "Auth/HMACSHA1.h"
#include "BigNumber.h"
#include "Log/Log.h"

#include <openssl/core_names.h>
#include <openssl/params.h>
#include <string>

HMACSHA1::HMACSHA1(uint32 len, uint8 *seed) : m_ctx(nullptr), m_digest()
{
    EVP_MAC* mac = EVP_MAC_fetch(nullptr, "HMAC", nullptr);
    if (!mac)
    {
        sLog.outError("HMACSHA1: Failed to fetch the HMAC implementation");
        return;
    }

    // The context takes its own reference on the algorithm.
    m_ctx = EVP_MAC_CTX_new(mac);
    EVP_MAC_free(mac);

    if (!m_ctx)
    {
        sLog.outError("HMACSHA1: Failed to create MAC context");
        return;
    }

    // OSSL_PARAM_construct_utf8_string() wants a mutable buffer.
    char digest[] = "SHA1";
    OSSL_PARAM params[2];
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_DIGEST, digest, 0);
    params[1] = OSSL_PARAM_construct_end();

    if (EVP_MAC_init(m_ctx, seed, len, params) != 1)
    {
        sLog.outError("HMACSHA1: Failed to initialize HMAC-SHA1 with the given key");
        EVP_MAC_CTX_free(m_ctx);
        m_ctx = nullptr;
    }
}

HMACSHA1::~HMACSHA1()
{
    EVP_MAC_CTX_free(m_ctx);
    m_ctx = nullptr;
}

void HMACSHA1::UpdateBigNumber(BigNumber *bn)
{
    UpdateData(bn->AsByteArray(), bn->GetNumBytes());
}

void HMACSHA1::UpdateData(const uint8 *data, int length)
{
    if (!m_ctx)
    {
        return;
    }

    EVP_MAC_update(m_ctx, data, length);
}

void HMACSHA1::UpdateData(const std::string &str)
{
    UpdateData((uint8 const*)str.c_str(), str.length());
}

void HMACSHA1::Finalize()
{
    if (!m_ctx)
    {
        return;
    }

    size_t length = 0;
    EVP_MAC_final(m_ctx, (uint8*)m_digest, &length, SHA_DIGEST_LENGTH);
    MANGOS_ASSERT(length == SHA_DIGEST_LENGTH);
}

uint8 *HMACSHA1::ComputeHash(BigNumber *bn)
{
    UpdateData(bn->AsByteArray(), bn->GetNumBytes());
    Finalize();
    return (uint8*)m_digest;
}
