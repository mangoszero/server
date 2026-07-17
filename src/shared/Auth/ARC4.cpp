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

/**
 * @file ARC4.cpp
 * @brief Implementation of ARC4 encryption algorithm using OpenSSL
 *
 * This file implements the ARC4 (Alleged RC4) stream cipher for use
 * in the MaNGOS authentication and session encryption system. ARC4
 * is used to encrypt/decrypt game traffic between the server and clients.
 *
 * The implementation uses OpenSSL's EVP interface for the cipher operations
 * and includes proper provider management for OpenSSL 3.x compatibility.
 */

#include "ARC4.h"
#include "OpenSSLProvider.h"
#include "Log/Log.h"

/**
 * @brief Construct ARC4 cipher with specified key length
 * @param len Key length in bytes
 *
 * Creates an ARC4 cipher context with the specified key length.
 * The key itself is not set in this constructor - use Init() to set it.
 *
 * @note Requires the OpenSSL legacy provider, which the daemon loads once at
 * startup; see the note on the class.
 */
ARC4::ARC4(uint8 len) : m_cipherContext()
{
    if (!m_cipherContext.IsValid())
    {
        sLog.outError("ARC4: Failed to create cipher context");
        return;
    }

    // RC4 is only supplied by the legacy provider, so this is where a daemon
    // that never loaded it finds out.
    if (EVP_EncryptInit_ex(m_cipherContext.Get(), EVP_rc4(), NULL, NULL, NULL) != 1)
    {
        sLog.outError("ARC4: Failed to initialize RC4 - is the OpenSSL legacy provider loaded?");
        return;
    }

    EVP_CIPHER_CTX_set_key_length(m_cipherContext.Get(), len);
}

/**
 * @brief Construct ARC4 cipher with initial key
 * @param seed Pointer to the key bytes
 * @param len Length of the key in bytes
 *
 * Creates an ARC4 cipher context and initializes it with the provided key.
 * The cipher is ready to use for encryption/decryption immediately after
 * construction.
 *
 * @note Requires the OpenSSL legacy provider, which the daemon loads once at
 * startup; see the note on the class.
 */
ARC4::ARC4(uint8 *seed, uint8 len) : m_cipherContext()
{
    if (!m_cipherContext.IsValid())
    {
        sLog.outError("ARC4: Failed to create cipher context");
        return;
    }

    // RC4 is only supplied by the legacy provider, so this is where a daemon
    // that never loaded it finds out.
    if (EVP_EncryptInit_ex(m_cipherContext.Get(), EVP_rc4(), NULL, NULL, NULL) != 1)
    {
        sLog.outError("ARC4: Failed to initialize RC4 - is the OpenSSL legacy provider loaded?");
        return;
    }

    EVP_CIPHER_CTX_set_key_length(m_cipherContext.Get(), len);
    EVP_EncryptInit_ex(m_cipherContext.Get(), NULL, NULL, seed, NULL);
}

/**
 * @brief Destructor for ARC4 cipher
 *
 * All cleanup is handled automatically by the OpenSSLCipherContext RAII wrapper.
 */
ARC4::~ARC4()
{
    // Cleanup is now handled automatically by RAII wrappers
}

/**
 * @brief Initialize or re-initialize the cipher with a new key
 * @param seed Pointer to the key bytes
 *
 * Sets or changes the encryption key for the ARC4 cipher.
 * This can be called multiple times to re-key the cipher.
 *
 * @note The key length must match the length specified in the constructor.
 */
void ARC4::Init(uint8 *seed)
{
    if (m_cipherContext.IsValid())
    {
        EVP_EncryptInit_ex(m_cipherContext.Get(), NULL, NULL, seed, NULL);
    }
}

/**
 * @brief Encrypt or decrypt data in-place
 * @param len Length of data to process in bytes
 * @param data Pointer to the data buffer (modified in-place)
 *
 * Processes data using the ARC4 stream cipher. Since ARC4 is a symmetric
 * stream cipher, the same operation is used for both encryption and
 * decryption. The data is modified in-place for efficiency.
 *
 * @warning The cipher must be initialized with a key before calling this.
 * @note The output length will always equal the input length for ARC4.
 */
void ARC4::UpdateData(int len, uint8 *data)
{
    if (!m_cipherContext.IsValid())
    {
        sLog.outError("ARC4: Invalid cipher context, cannot update data");
        return;
    }

    int outlen = 0;
    EVP_EncryptUpdate(m_cipherContext.Get(), data, &outlen, data, len);
    EVP_EncryptFinal_ex(m_cipherContext.Get(), data, &outlen);
}
