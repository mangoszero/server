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

#ifndef _AUTH_HMACSHA1_H
#define _AUTH_HMACSHA1_H

#include "Common/Common.h"
#include <openssl/hmac.h>
#include <openssl/sha.h>

class BigNumber;

#define SEED_KEY_SIZE 16

/**
 * @brief HMAC-SHA1 hash computation for authentication
 *
 * HMACSHA1 provides HMAC (Hash-based Message Authentication Code)
 * using SHA-1 hash algorithm. Used for secure authentication
 * and packet integrity verification in World of Warcraft protocol.
 */
class HMACSHA1
{
public:
    /**
     * @brief Constructor with seed initialization
     * @param len Length of the seed
     * @param seed Pointer to seed data
     */
    HMACSHA1(uint32 len, uint8 *seed);
    /**
     * @brief Destructor
     */
    ~HMACSHA1();
    /**
     * @brief Update hash with a BigNumber
     * @param bn BigNumber to add to hash
     */
    void UpdateBigNumber(BigNumber *bn);
    /**
     * @brief Update hash with raw data
     * @param data Pointer to data
     * @param length Length of data
     */
    void UpdateData(const uint8 *data, int length);
    /**
     * @brief Update hash with string data
     * @param str String to add to hash
     */
    void UpdateData(const std::string &str);
    /**
     * @brief Finalize the hash computation
     */
    void Finalize();
    /**
     * @brief Compute hash for a BigNumber
     * @param bn BigNumber to hash
     * @return Pointer to digest buffer
     */
    uint8 *ComputeHash(BigNumber *bn);
    /**
     * @brief Get the computed digest
     * @return Pointer to digest buffer
     */
    uint8 *GetDigest() { return (uint8*)m_digest; }
    /**
     * @brief Get the digest length
     * @return SHA_DIGEST_LENGTH (20 bytes)
     */
    int GetLength() { return SHA_DIGEST_LENGTH; }
private:
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    HMAC_CTX m_ctx; /**< OpenSSL HMAC context (pre-1.1.0) */
#else
    HMAC_CTX* m_ctx; /**< OpenSSL HMAC context (1.1.0+) */
#endif
    uint8 m_digest[SHA_DIGEST_LENGTH]; /**< Computed hash digest */
};
#endif
