/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
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

#include <string>
#include "TestHarness.h"
#include "Auth/OpenSSLProvider.h"

#include <charconv>
#include <openssl/crypto.h>
#include <openssl/evp.h>

TEST(OpenSSL_runtime_and_providers_are_major_three)
{
    OpenSSLProviderManager& manager =
        OpenSSLProviderManager::Instance();
    REQUIRE(manager.IsInitialized());

    unsigned runtimeMajor =
        unsigned((OpenSSL_version_num() >> 28) & 0x0f);
    CHECK_EQ(runtimeMajor, 3);

    auto checkProvider = [runtimeMajor](
        const OpenSSLProvider& provider)
    {
        std::string version = provider.Version();
        REQUIRE(!version.empty());
        std::size_t separator = version.find('.');
        REQUIRE(separator != std::string::npos);
        unsigned major = 0;
        std::from_chars_result result = std::from_chars(
            version.data(), version.data() + separator, major);
        CHECK(result.ec == std::errc{});
        CHECK(result.ptr == version.data() + separator);
        CHECK_EQ(major, runtimeMajor);
    };

    checkProvider(manager.GetLegacyProvider());
    checkProvider(manager.GetDefaultProvider());
}

TEST(OpenSSL_legacy_provider_supplies_rc4)
{
    REQUIRE(OpenSSLProviderManager::Instance().IsInitialized());
    EVP_CIPHER* rc4 = EVP_CIPHER_fetch(nullptr, "RC4", nullptr);
    CHECK(rc4 != nullptr);
    EVP_CIPHER_free(rc4);
}
