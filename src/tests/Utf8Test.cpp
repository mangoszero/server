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

// Pins the four utf8cpp entry points the server actually calls, because the library is
// a vendored dependency that gets bumped and the failure mode is not a compile error.
//
// Every string that reaches these functions arrives over a socket -- a character name, a
// guild name, a chat line -- so the hostile path is the point of the exercise: truncated
// sequences, overlong encodings, lone continuation bytes, and a surrogate half smuggled
// in as three bytes. Rejection must be the worst outcome.

#include "TestHarness.h"

#include "utf8.h"

#include <cstdint>
#include <string>
#include <vector>

namespace
{
    // Written as byte escapes rather than as literals: a source file that is Windows-1252
    // in places cannot be trusted to carry a UTF-8 literal through an editor unharmed.
    const std::string ASCII    = "Thrall";
    const std::string LATIN    = "\xC3\x89l\xC3\xA9onore";              // Éléonore
    const std::string CJK      = "\xE6\x9A\xB4\xE9\xA3\x8E";            // 暴风
    const std::string ASTRAL   = "\xF0\x9F\x90\x89";                    // U+1F409, 4 bytes

    size_t CodePoints(const std::string& s)
    {
        return size_t(utf8::distance(s.c_str(), s.c_str() + s.size()));
    }

    // The round trip the server performs on every name it stores or compares.
    std::string RoundTrip(const std::string& in)
    {
        // A code point outside the BMP occupies TWO utf16 units, so a buffer sized by
        // the code-point count is too small and the count is not the wide length.
        // utf8to16's RETURN VALUE is the only thing that knows where the wide string
        // ends -- which is exactly what Util.cpp relies on.
        std::vector<uint16_t> wide(in.size() + 1, 0);
        auto wend = utf8::utf8to16(in.c_str(), in.c_str() + in.size(), wide.begin());

        std::string out(in.size() * 2 + 1, '\0');
        auto oend = utf8::utf16to8(wide.begin(), wend, out.begin());
        out.resize(size_t(oend - out.begin()));
        return out;
    }
}

TEST(Utf8_DistanceCountsCodePointsNotBytes)
{
    CHECK_EQ(CodePoints(ASCII), size_t(6));
    CHECK_EQ(ASCII.size(), size_t(6));

    // Nine bytes, eight characters: the two accented ones are two bytes each.
    CHECK_EQ(LATIN.size(), size_t(10));
    CHECK_EQ(CodePoints(LATIN), size_t(8));

    CHECK_EQ(CJK.size(), size_t(6));
    CHECK_EQ(CodePoints(CJK), size_t(2));

    // ONE code point in four bytes. A length check that counted bytes would let a name
    // four times over the limit through, and one that counted utf16 units would still
    // be wrong here.
    CHECK_EQ(ASTRAL.size(), size_t(4));
    CHECK_EQ(CodePoints(ASTRAL), size_t(1));
}

TEST(Utf8_RoundTripsThroughUtf16)
{
    CHECK_STR(RoundTrip(ASCII), ASCII);
    CHECK_STR(RoundTrip(LATIN), LATIN);
    CHECK_STR(RoundTrip(CJK), CJK);
    CHECK_STR(RoundTrip(ASTRAL), ASTRAL);
    CHECK_STR(RoundTrip(ASCII + LATIN + CJK + ASTRAL), ASCII + LATIN + CJK + ASTRAL);
    CHECK_STR(RoundTrip(std::string()), std::string());
}

TEST(Utf8_FindInvalidRejectsHostileInput)
{
    struct Bad { const char* bytes; size_t len; const char* what; };
    const Bad cases[] = {
        { "\xC3",                 1, "truncated two-byte sequence" },
        { "\xE6\x9A",             2, "truncated three-byte sequence" },
        { "\xF0\x9F\x90",         3, "truncated four-byte sequence" },
        { "\x80",                 1, "lone continuation byte" },
        { "\xC0\xAF",             2, "overlong encoding of '/'" },
        { "\xE0\x80\xAF",         3, "overlong three-byte encoding" },
        { "\xED\xA0\x80",         3, "UTF-16 surrogate half, invalid in UTF-8" },
        { "\xF5\x80\x80\x80",     4, "code point above U+10FFFF" },
        { "Thrall\xC3",           7, "valid prefix, truncated tail" },
    };

    for (const Bad& c : cases)
    {
        const std::string s(c.bytes, c.len);
        const char* end = s.c_str() + s.size();
        const char* bad = utf8::find_invalid(s.c_str(), end);
        if (bad == end)
        {
            testing::ReportFailure(__FILE__, __LINE__,
                std::string("accepted invalid input: ") + c.what);
        }
    }
}

TEST(Utf8_FindInvalidAcceptsValidInput)
{
    const std::string good[] = { ASCII, LATIN, CJK, ASTRAL,
                                 ASCII + CJK + ASTRAL, std::string() };
    for (const std::string& s : good)
    {
        const char* end = s.c_str() + s.size();
        CHECK(utf8::find_invalid(s.c_str(), end) == end);
    }
}
