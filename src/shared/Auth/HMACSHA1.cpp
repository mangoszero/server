/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2015  MaNGOS project <http://getmangos.eu>
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

HMACSHA1::HMACSHA1()
{
    uint8 temp[SEED_KEY_SIZE] = { 0x38, 0xA7, 0x83, 0x15, 0xF8, 0x92, 0x25, 0x30, 0x71, 0x98, 0x67, 0xB1, 0x8C, 0x4, 0xE2, 0xAA };

    memcpy(&m_key, &temp, SEED_KEY_SIZE);
    HMAC_CTX_init(&m_ctx);
    HMAC_Init_ex(&m_ctx, &m_key, SEED_KEY_SIZE, EVP_sha1(), NULL);
}

HMACSHA1::~HMACSHA1()
{
    memset(&m_key, 0x00, SEED_KEY_SIZE);
    HMAC_CTX_cleanup(&m_ctx);
}

void HMACSHA1::UpdateBigNumber(BigNumber* bn)
{
    UpdateData(bn->AsByteArray(), bn->GetNumBytes());
}

void HMACSHA1::UpdateData(const uint8* data, int length)
{
    HMAC_Update(&m_ctx, data, length);
}

void HMACSHA1::Initialize()
{
    HMAC_Init_ex(&m_ctx, &m_key, SEED_KEY_SIZE, EVP_sha1(), NULL);
}

void HMACSHA1::Finalize()
{
    uint32 length = 0;
    HMAC_Final(&m_ctx, m_digest, &length);
    MANGOS_ASSERT(length == SHA_DIGEST_LENGTH);
}
