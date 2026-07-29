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

#ifndef MANGOS_COMPAT_EXTRACTORS_AUTH_MD5_H
#define MANGOS_COMPAT_EXTRACTORS_AUTH_MD5_H

/**
 * @file
 * @brief TEMPORARY -- the old shared/Auth/md5.h API, backed by OpenSSL.
 *
 * vmapexport.cpp hashes a path string into a stable filename with the vendored
 * MD5 that used to live in shared/Auth (383 lines of RFC 1321, Aladdin
 * Enterprises, 1999). That copy is gone: the tree links OpenSSL everywhere, so
 * carrying a second implementation of the same digest bought nothing.
 *
 * Extractor_projects is shared with other cores and must not be modified here,
 * so rather than change its call site, the old three-call API is re-offered with
 * EVP underneath. Same name, same signatures, same 16 bytes out.
 *
 * The digest has no security role -- it only has to be deterministic, so that
 * the same path always yields the same extracted filename. That is the only
 * reason MD5 is acceptable at all, and the reason this shim is a straight
 * translation rather than an upgrade to a stronger hash: changing the digest
 * would change every generated filename.
 *
 * Visible to the extractor targets alone. Delete once the extractors call EVP
 * directly upstream.
 *
 * This is scaffolding with a known end date. The change this stands in for is
 * already written and queued as a pull request against the submodule's own
 * repository; once that merges, bump the submodule and delete this file and its
 * mangos_submodule_compat() call. Do not widen it to cover new divergence --
 * that is a sign the fix belongs upstream, not here.
 */

#include <openssl/evp.h>

#include <cstring>

typedef unsigned char md5_byte_t;
typedef unsigned int  md5_word_t;

/**
 * Stands in for the old md5_state_s. Holds an EVP context rather than the
 * raw ABCD state words; nothing outside these three calls ever looked inside.
 */
typedef struct md5_state_s
{
    EVP_MD_CTX* ctx;
} md5_state_t;

inline void mangos_md5_init(md5_state_t* pms)
{
    pms->ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(pms->ctx, EVP_md5(), nullptr);
}

inline void md5_append(md5_state_t* pms, const md5_byte_t* data, int nbytes)
{
    if (nbytes > 0)
    {
        EVP_DigestUpdate(pms->ctx, data, static_cast<size_t>(nbytes));
    }
}

inline void md5_finish(md5_state_t* pms, md5_byte_t digest[16])
{
    // EVP_DigestFinal_ex insists on a buffer it can write up to EVP_MAX_MD_SIZE
    // into, while callers of this API hand over exactly 16 bytes. MD5 produces
    // 16, but the contract is about the buffer, not the digest -- so finalise
    // into a full-size scratch buffer and copy the 16 out.
    unsigned char full[EVP_MAX_MD_SIZE];
    unsigned int  length = 0;

    EVP_DigestFinal_ex(pms->ctx, full, &length);
    EVP_MD_CTX_free(pms->ctx);
    pms->ctx = nullptr;

    std::memcpy(digest, full, length < 16u ? length : 16u);
}

#endif // MANGOS_COMPAT_EXTRACTORS_AUTH_MD5_H
