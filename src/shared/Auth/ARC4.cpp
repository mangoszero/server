/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2017  MaNGOS project <https://getmangos.eu>
 * Copyright (C) 2008-2015 TrinityCore <http://www.trinitycore.org/>
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

#include "ARC4.h"

ARC4::ARC4(uint8 len) : m_ctx()
{
    EVP_CIPHER_CTX_init(&m_ctx);
    EVP_EncryptInit_ex(&m_ctx, EVP_rc4(), nullptr, nullptr, nullptr);
    EVP_CIPHER_CTX_set_key_length(&m_ctx, len);
}

ARC4::ARC4(uint8 *seed, uint8 len) : m_ctx()
{
    EVP_CIPHER_CTX_init(&m_ctx);
    EVP_EncryptInit_ex(&m_ctx, EVP_rc4(), nullptr, nullptr, nullptr);
    EVP_CIPHER_CTX_set_key_length(&m_ctx, len);
    EVP_EncryptInit_ex(&m_ctx, nullptr, nullptr, seed, nullptr);
}

ARC4::~ARC4()
{
    EVP_CIPHER_CTX_cleanup(&m_ctx);
}

void ARC4::Init(uint8 *seed)
{
    EVP_EncryptInit_ex(&m_ctx, nullptr, nullptr, seed, nullptr);
}

void ARC4::UpdateData(int len, uint8 *data)
{
    int outlen = 0;
    EVP_EncryptUpdate(&m_ctx, data, &outlen, data, len);
    EVP_EncryptFinal_ex(&m_ctx, data, &outlen);
}
