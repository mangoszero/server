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

#ifndef _AUTH_MD5_H
#define _AUTH_MD5_H

#include "Platform/Define.h"

#include <openssl/evp.h>
#include <openssl/md5.h>        // MD5_DIGEST_LENGTH

#include <cstddef>
#include <string>

/**
 * @brief MD5 over OpenSSL's EVP interface.
 *
 * MD5 is used here only where the Warden protocol requires it -- identifying a
 * client module by the digest of its compressed image, and checking the digest
 * the client reports back. It is not used for anything security-bearing that
 * could be moved to a stronger digest: the algorithm is fixed by what the
 * 3.3.5a client does.
 *
 * The point of this class is the *interface*, not the algorithm. The call sites
 * previously used OpenSSL's low-level MD5_Init/MD5_Update/MD5_Final, which
 * OpenSSL 3.0 deprecated; building those targets with OPENSSL_NO_DEPRECATED --
 * which is what mangos_openssl_strict asks for everywhere else in this tree --
 * deletes their declarations outright. Mirrors Sha1Hash so both digests are
 * reached the same way.
 */
class Md5Hash
{
    public:

        Md5Hash() : m_ctx(EVP_MD_CTX_new())
        {
            Initialize();
        }

        ~Md5Hash()
        {
            EVP_MD_CTX_free(m_ctx);
        }

        Md5Hash(const Md5Hash&) = delete;
        Md5Hash& operator=(const Md5Hash&) = delete;

        /// Reset, ready to hash a fresh message.
        void Initialize()
        {
            EVP_DigestInit_ex(m_ctx, EVP_md5(), nullptr);
        }

        void UpdateData(const uint8* data, size_t length)
        {
            EVP_DigestUpdate(m_ctx, data, length);
        }

        void UpdateData(const std::string& str)
        {
            UpdateData(reinterpret_cast<const uint8*>(str.c_str()), str.size());
        }

        /// Produce the digest. Call once, after the last UpdateData().
        void Finalize()
        {
            unsigned int length = 0;
            EVP_DigestFinal_ex(m_ctx, m_digest, &length);
        }

        /// The digest. Only meaningful after Finalize(); MD5_DIGEST_LENGTH bytes.
        uint8* GetDigest() { return m_digest; }

        static constexpr int DigestLength = MD5_DIGEST_LENGTH;

    private:

        EVP_MD_CTX* m_ctx;
        uint8       m_digest[MD5_DIGEST_LENGTH]{};
};

#endif
