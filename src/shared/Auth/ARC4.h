/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2025 MaNGOS <https://www.getmangos.eu>
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

#ifndef _AUTH_SARC4_H
#define _AUTH_SARC4_H

#include <openssl/evp.h>
#include "Common/Common.h"
#include "OpenSSLProvider.h"

/**
 * @brief ARC4 encryption/decryption cipher implementation
 *
 * ARC4 is a stream cipher that uses a key to generate a keystream.
 * This class provides ARC4 encryption and decryption functionality using OpenSSL.
 *
 * @note RC4 lives in OpenSSL's legacy provider. Loading it is the daemon's
 * job, done once at startup (see OpenSSLProviderManager in mangosd's main());
 * an ARC4 instance does not load it, so constructing one before that point
 * will fail to initialize the cipher.
 */
class ARC4
{
    public:
        /**
         * @brief Constructor with key length specification
         * @param len Length of the key in bytes
         */
        ARC4(uint8 len);
        /**
         * @brief Constructor with seed data
         * @param seed Pointer to the seed/key data
         * @param len Length of the seed data in bytes
         */
        ARC4(uint8 *seed, uint8 len);
        /**
         * @brief Destructor
         */
        ~ARC4();
        /**
         * @brief Initialize the cipher with seed data
         * @param seed Pointer to the seed/key data
         */
        void Init(uint8 *seed);
        /**
         * @brief Update/encrypt data using the cipher
         * @param len Length of the data to process
         * @param data Pointer to the data to encrypt/decrypt
         */
        void UpdateData(int len, uint8 *data);
    private:
        OpenSSLCipherContext m_cipherContext;        /**< RAII cipher context */
};

#endif
