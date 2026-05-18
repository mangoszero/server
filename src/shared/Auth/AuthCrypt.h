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

#ifndef MANGOS_H_AUTHCRYPT
#define MANGOS_H_AUTHCRYPT

#include "Common/Common.h"
#include <vector>

class BigNumber;

/**
 * @brief Authentication encryption/decryption for World of Warcraft protocol
 *
 * AuthCrypt handles the session key-based encryption and decryption of
 * World of Warcraft client-server packets using a modified version of ARC4.
 * It maintains separate encryption and decryption states for bidirectional
 * communication.
 */
class AuthCrypt
{
    public:
        /**
         * @brief Constructor - initializes the crypt object
         */
        AuthCrypt();
        /**
         * @brief Destructor
         */
        ~AuthCrypt();

        /**
         * @brief Initialize the encryption/decryption state
         */
        void Init();

        /**
         * @brief Set the session key for encryption/decryption
         * @param key Pointer to the session key data
         * @param len Length of the key in bytes
         */
        void SetKey(uint8* key, size_t len);

        /**
         * @brief Decrypt received data from client
         * @param data Pointer to data buffer to decrypt
         * @param len Length of data to decrypt
         */
        void DecryptRecv(uint8*, size_t);
        /**
         * @brief Encrypt data to send to client
         * @param data Pointer to data buffer to encrypt
         * @param len Length of data to encrypt
         */
        void EncryptSend(uint8*, size_t);

        /**
         * @brief Check if the crypt object is initialized
         * @return True if initialized, false otherwise
         */
        bool IsInitialized() { return _initialized; }

    private:
        std::vector<uint8> _key; /**< Session key for encryption */
        uint8 _send_i, _send_j, _recv_i, _recv_j; /**< ARC4 state variables for send/recv */
        bool _initialized; /**< Initialization status */
};
#endif
