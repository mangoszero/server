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

#ifndef _AUTH_SHA1_H
#define _AUTH_SHA1_H

#include "Common/Common.h"
#include <openssl/evp.h>
#include <openssl/sha.h>       // SHA_DIGEST_LENGTH
#include <string>

class BigNumber;

/**
 * @brief SHA-1 hash computation for cryptographic operations
 *
 * Sha1Hash provides SHA-1 (Secure Hash Algorithm 1) hash computation
 * using OpenSSL. Used for password hashing, session key generation,
 * and data integrity verification in World of Warcraft authentication.
 */
class Sha1Hash
{
    public:
        /**
         * @brief Constructor - initializes SHA-1 context
         */
        Sha1Hash();
        /**
         * @brief Destructor
         */
        ~Sha1Hash();

        /**
         * @brief Copy constructor - duplicates the digest context
         *
         * Value semantics are load-bearing: realmd passes a finalized
         * Sha1Hash to AuthSocket::SendProof() by value.
         */
        Sha1Hash(const Sha1Hash& other);
        /**
         * @brief Copy assignment - duplicates the digest context
         */
        Sha1Hash& operator=(const Sha1Hash& other);

        /**
         * @brief Update hash with multiple BigNumbers (variadic)
         * @param bn0 First BigNumber to add
         * @param ... Additional BigNumbers (NULL terminated)
         */
        void UpdateBigNumbers(BigNumber* bn0, ...);

        /**
         * @brief Update hash with raw data
         * @param dta Pointer to data
         * @param len Length of data
         */
        void UpdateData(const uint8* dta, int len);
        /**
         * @brief Update hash with string data
         * @param str String to add to hash
         */
        void UpdateData(const std::string& str);

        /**
         * @brief Initialize/reset the SHA-1 context
         */
        void Initialize();
        /**
         * @brief Finalize the hash computation
         */
        void Finalize();

        /**
         * @brief Get the computed digest
         * @return Pointer to digest buffer (20 bytes)
         */
        uint8* GetDigest(void) { return mDigest; };
        /**
         * @brief Get the digest length
         * @return SHA_DIGEST_LENGTH (20 bytes)
         */
        int GetLength(void) { return SHA_DIGEST_LENGTH; };

    private:
        EVP_MD_CTX* mC; /**< OpenSSL SHA-1 context */
        uint8 mDigest[SHA_DIGEST_LENGTH]{ 0 }; /**< Computed hash digest */
};
#endif
