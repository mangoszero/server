/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2019  MaNGOS project <https://getmangos.eu>
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

HMACSHA1::HMACSHA1(uint32 len, uint8 *seed)
{
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    HMAC_CTX_init(&m_ctx);
    HMAC_Init_ex(&m_ctx, seed, len, EVP_sha1(), NULL);
#else
    m_ctx = HMAC_CTX_new();
    HMAC_Init_ex(m_ctx, seed, len, EVP_sha1(), NULL);
#endif
}

HMACSHA1::~HMACSHA1()
{
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    HMAC_CTX_cleanup(&m_ctx);
#else
    HMAC_CTX_free(m_ctx);
#endif
}

void HMACSHA1::UpdateBigNumber(BigNumber *bn)
{
    UpdateData(bn->AsByteArray(), bn->GetNumBytes());
}

void HMACSHA1::UpdateData(const uint8 *data, int length)
{
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    HMAC_Update(&m_ctx, data, length);
#else
    HMAC_Update(m_ctx, data, length);
#endif
}

void HMACSHA1::UpdateData(const std::string &str)
{
    UpdateData((uint8 const*)str.c_str(), str.length());
}

void HMACSHA1::Finalize()
{
    uint32 length = 0;
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    HMAC_Final(&m_ctx, (uint8*)m_digest, &length);
#else
    HMAC_Final(m_ctx, (uint8*)m_digest, &length);
#endif
    MANGOS_ASSERT(length == SHA_DIGEST_LENGTH);
}

uint8 *HMACSHA1::ComputeHash(BigNumber *bn)
{
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    HMAC_Update(&m_ctx, bn->AsByteArray(), bn->GetNumBytes());
#else
    HMAC_Update(m_ctx, bn->AsByteArray(), bn->GetNumBytes());
#endif
    Finalize();
    return (uint8*)m_digest;
}
