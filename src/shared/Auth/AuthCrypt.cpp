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

#include "AuthCrypt.h"
#include "HMACSHA1.h"
#include "Log/Log.h"
#include "BigNumber.h"

const static size_t CRYPTED_SEND_LEN = 4;
const static size_t CRYPTED_RECV_LEN = 6;

/**
 * Initializes the authentication crypt state in an uninitialized state.
 */
AuthCrypt::AuthCrypt()
{
    _initialized = false;
}

/**
 * Decrypts the fixed-size encrypted receive header in place.
 */
void AuthCrypt::DecryptRecv(uint8* data, size_t len)
{
    if (!_initialized)
    {
        return;
    }
    if (len < CRYPTED_RECV_LEN)
    {
        return;
    }

    for (size_t t = 0; t < CRYPTED_RECV_LEN; t++)
    {
        _recv_i %= _key.size();
        uint8 x = (data[t] - _recv_j) ^ _key[_recv_i];
        ++_recv_i;
        _recv_j = data[t];
        data[t] = x;
    }
}

/**
 * Encrypts the fixed-size outgoing packet header in place.
 */
void AuthCrypt::EncryptSend(uint8* data, size_t len)
{
    if (!_initialized)
    {
        return;
    }

    if (len < CRYPTED_SEND_LEN)
    {
        return;
    }

    for (size_t t = 0; t < CRYPTED_SEND_LEN; t++)
    {
        _send_i %= _key.size();
        uint8 x = (data[t] ^ _key[_send_i]) + _send_j;
        ++_send_i;
        data[t] = _send_j = x;
    }
}

#if defined(CLASSIC)
/**
 * Resets the stream counters and marks the crypt state as initialized.
 */
void AuthCrypt::Init()
{
    _send_i = _send_j = _recv_i = _recv_j = 0;
    _initialized = true;
}

/**
 * Stores the session key material used for header encryption and decryption.
 */
void AuthCrypt::SetKey(uint8* key, size_t len)
{
    _key.resize(len);
    std::copy(key, key + len, _key.begin());
}
#else
/**
 * Resets the stream counters and marks the crypt state as initialized.
 */
void AuthCrypt::Init(BigNumber* K)
{
    uint8* key = new uint8[SHA_DIGEST_LENGTH];
    uint8 recvSeed[SEED_KEY_SIZE] = { 0x38, 0xA7, 0x83, 0x15, 0xF8, 0x92, 0x25, 0x30, 0x71, 0x98, 0x67, 0xB1, 0x8C, 0x4, 0xE2, 0xAA };
    HMACSHA1 recvHash(SEED_KEY_SIZE, (uint8*)recvSeed);
    recvHash.UpdateBigNumber(K);
    recvHash.Finalize();
    memcpy(key, recvHash.GetDigest(), SHA_DIGEST_LENGTH);
    _key.resize(SHA_DIGEST_LENGTH);
    std::copy(key, key + SHA_DIGEST_LENGTH, _key.begin());
    delete[] key;

    _send_i = _send_j = _recv_i = _recv_j = 0;
    _initialized = true;
}
#endif

/**
 * Destroys the authentication crypt helper.
 */
AuthCrypt::~AuthCrypt()
{
}
