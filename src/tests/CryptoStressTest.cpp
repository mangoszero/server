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

#include <algorithm>
#include "TestHarness.h"

#include "Auth/ARC4.h"
#include "Auth/BigNumber.h"
#include "Auth/HMACSHA1.h"
#include "Auth/Md5.h"
#include "Auth/Sha1.h"

#include <cstring>
#include <random>
#include <string>
#include <vector>

/**
 * @file
 * @brief Known-answer and stress coverage for the crypto used on the wire.
 *
 * Every digest here was reimplemented during the OpenSSL 3.x migration: Sha1Hash
 * and HMACSHA1 moved from the deprecated low-level API to EVP, Md5Hash is new,
 * and a 474-line vendored MD5 was deleted. A reimplementation that is subtly
 * wrong does not fail to build and does not crash -- it produces a digest the
 * client rejects, which presents as "login does not work" with nothing in the
 * log. Known-answer vectors are the only thing that catches that.
 *
 * The vectors are from RFC 1321 (MD5), RFC 3174 (SHA-1) and RFC 2202 (HMAC).
 *
 * The session cipher's own fragmentation property lives in AuthCryptTest.cpp.
 */

namespace
{
    std::string ToHex(const uint8* data, size_t length)
    {
        static const char* digits = "0123456789abcdef";
        std::string out;
        out.reserve(length * 2);
        for (size_t i = 0; i < length; ++i)
        {
            out.push_back(digits[(data[i] >> 4) & 0x0F]);
            out.push_back(digits[data[i] & 0x0F]);
        }
        return out;
    }

    void CheckHex(const char* what, const std::string& got, const char* expected)
    {
        if (got != expected)
        {
            testing::ReportFailure(__FILE__, __LINE__,
                std::string(what) + ": got " + got + ", expected " + expected);
        }
    }
}

// ---------------------------------------------------------------------------
// Known answers
// ---------------------------------------------------------------------------

TEST(Crypto_md5_known_vectors)
{
    {
        Md5Hash h;
        h.Finalize();
        CheckHex("MD5(\"\")", ToHex(h.GetDigest(), 16),
                 "d41d8cd98f00b204e9800998ecf8427e");
    }
    {
        Md5Hash h;
        h.UpdateData(std::string("abc"));
        h.Finalize();
        CheckHex("MD5(\"abc\")", ToHex(h.GetDigest(), 16),
                 "900150983cd24fb0d6963f7d28e17f72");
    }
    {
        Md5Hash h;
        h.UpdateData(std::string("message digest"));
        h.Finalize();
        CheckHex("MD5(\"message digest\")", ToHex(h.GetDigest(), 16),
                 "f96b697d7cb7938d525a2f31aaf161d0");
    }
}

TEST(Crypto_sha1_known_vectors)
{
    {
        Sha1Hash h;
        h.Finalize();
        CheckHex("SHA1(\"\")", ToHex(h.GetDigest(), 20),
                 "da39a3ee5e6b4b0d3255bfef95601890afd80709");
    }
    {
        Sha1Hash h;
        h.UpdateData(std::string("abc"));
        h.Finalize();
        CheckHex("SHA1(\"abc\")", ToHex(h.GetDigest(), 20),
                 "a9993e364706816aba3e25717850c26c9cd0d89d");
    }
}

TEST(Crypto_hmac_sha1_rfc2202_vector)
{
    // RFC 2202 test case 1.
    uint8 key[20];
    std::memset(key, 0x0b, sizeof(key));

    HMACSHA1 hmac(sizeof(key), key);
    hmac.UpdateData(std::string("Hi There"));
    hmac.Finalize();

    CheckHex("HMAC-SHA1", ToHex(hmac.GetDigest(), 20),
             "b617318655057264e28bc0b6fb378c8ef146be00");
}

TEST(Crypto_incremental_hashing_matches_one_shot)
{
    // Feeding a digest in pieces must equal feeding it whole. This is the
    // property Warden and the login proof both rely on, since both build their
    // input from several appends.
    std::mt19937 rng(0x51A1u);
    std::uniform_int_distribution<int> byteDist(0, 255);
    std::uniform_int_distribution<size_t> chunkDist(1, 37);

    int mismatches = 0;

    for (int iteration = 0; iteration < 500; ++iteration)
    {
        std::vector<uint8> message(1 + (iteration % 400));
        for (uint8& b : message)
        {
            b = uint8(byteDist(rng));
        }

        Sha1Hash whole;
        whole.UpdateData(message.data(), int(message.size()));
        whole.Finalize();
        const std::string expectedSha = ToHex(whole.GetDigest(), 20);

        Sha1Hash pieces;
        size_t offset = 0;
        while (offset < message.size())
        {
            const size_t chunk = std::min(chunkDist(rng), message.size() - offset);
            pieces.UpdateData(&message[offset], int(chunk));
            offset += chunk;
        }
        pieces.Finalize();

        if (ToHex(pieces.GetDigest(), 20) != expectedSha)
        {
            ++mismatches;
        }

        Md5Hash wholeMd5;
        wholeMd5.UpdateData(message.data(), message.size());
        wholeMd5.Finalize();
        const std::string expectedMd5 = ToHex(wholeMd5.GetDigest(), 16);

        Md5Hash piecesMd5;
        offset = 0;
        while (offset < message.size())
        {
            const size_t chunk = std::min(chunkDist(rng), message.size() - offset);
            piecesMd5.UpdateData(&message[offset], chunk);
            offset += chunk;
        }
        piecesMd5.Finalize();

        if (ToHex(piecesMd5.GetDigest(), 16) != expectedMd5)
        {
            ++mismatches;
        }
    }

    CHECK_EQ(mismatches, 0);
}

// ---------------------------------------------------------------------------
// BigNumber under load
// ---------------------------------------------------------------------------

TEST(Crypto_bignumber_hex_round_trips)
{
    // SetHexStr/AsHexStr is how the session key crosses the database boundary
    // between realmd and the world server. A round-trip that loses a leading
    // zero is exactly the shape of the padding bug.
    std::mt19937 rng(0xB19Eu);
    std::uniform_int_distribution<int> nibble(0, 15);

    static const char* digits = "0123456789ABCDEF";
    int mismatches = 0;

    for (int iteration = 0; iteration < 2000; ++iteration)
    {
        std::string hex;
        hex.reserve(80);
        for (int i = 0; i < 80; ++i)
        {
            hex.push_back(digits[nibble(rng)]);
        }

        BigNumber n;
        n.SetHexStr(hex.c_str());

        // Fixed width, little-endian: SetBinary reverses its input, so it must
        // be fed the same byte order AsByteArray produces by default.
        const uint8* bytes = n.AsByteArray(40);

        BigNumber rebuilt;
        rebuilt.SetBinary(bytes, 40);

        if (std::string(n.AsHexStr()) != std::string(rebuilt.AsHexStr()))
        {
            ++mismatches;
        }
    }

    CHECK_EQ(mismatches, 0);
}

TEST(Crypto_bignumber_fixed_width_output_is_always_that_width)
{
    // Over many random values, AsByteArray(40) must always yield exactly 40
    // meaningful bytes with the value right-aligned. Roughly one value in 256
    // serialises short, so this sweep hits the padding path many times.
    int wrong = 0;

    for (int iteration = 0; iteration < 5000; ++iteration)
    {
        BigNumber n;
        n.SetRand(40 * 8);

        const uint8* le = n.AsByteArray(40);

        // Rebuilding from the fixed-width form must give the same number back.
        BigNumber rebuilt;
        rebuilt.SetBinary(le, 40);

        if (std::string(rebuilt.AsHexStr()) != std::string(n.AsHexStr()))
        {
            ++wrong;
        }
    }

    CHECK_EQ(wrong, 0);
}

TEST(Crypto_arc4_matches_the_published_vector)
{
    // The provider test next door asserts that RC4 can be FETCHED. That is not the same
    // as producing the right keystream: a cipher that initialises and encrypts to
    // something else still fetches perfectly, and the symptom is a client that connects
    // and then fails to decode a single packet.
    uint8 key[] = {'K', 'e', 'y'};
    uint8 data[] = {'P', 'l', 'a', 'i', 'n', 't', 'e', 'x', 't'};

    ARC4 rc4(key, static_cast<uint8>(sizeof(key)));
    rc4.UpdateData(sizeof(data), data);
    CHECK_HEX(data, sizeof(data), "bbf316e8d940af0ad3");
}

