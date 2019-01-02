/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2019  MaNGOS project <https://getmangos.eu>
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

#ifndef MANGOS_H_UTIL
#define MANGOS_H_UTIL

#include "Common/Common.h"
#include <ace/Null_Mutex.h>
#include <ace/INET_Addr.h>

#include <string>
#include <vector>

/**
 * @brief
 *
 */
typedef std::vector<std::string> Tokens;

/**
 * @brief
 *
 * @param src
 * @param sep
 * @return Tokens
 */
Tokens StrSplit(const std::string& src, const std::string& sep);
/**
 * @brief
 *
 * @param data
 * @param index
 * @return uint32
 */
uint32 GetUInt32ValueFromArray(Tokens const& data, uint16 index);
/**
 * @brief
 *
 * @param data
 * @param index
 * @return float
 */
float GetFloatValueFromArray(Tokens const& data, uint16 index);

/**
 * @brief
 *
 * @param src
 */
void stripLineInvisibleChars(std::string& src);

/**
 * @brief
 *
 * @param timeInSecs
 * @param shortText
 * @param hoursOnly
 * @return std::string
 */
std::string secsToTimeString(time_t timeInSecs, bool shortText = false, bool hoursOnly = false);
/**
 * @brief
 *
 * @param timestring
 * @return uint32
 */
uint32 TimeStringToSecs(const std::string& timestring);
/**
 * @brief
 *
 * @param t
 * @return std::string
 */
std::string TimeToTimestampStr(time_t t);

/**
 * @brief
 *
 * @param secs
 * @return uint32
 */
inline uint32 secsToTimeBitFields(time_t secs)
{
    tm* lt = localtime(&secs);
    return (lt->tm_year - 100) << 24 | lt->tm_mon  << 20 | (lt->tm_mday - 1) << 14 | lt->tm_wday << 11 | lt->tm_hour << 6 | lt->tm_min;
}


/**
 * @brief Return a random number in the range min..max; (max-min) must be smaller than 32768.
 *
 * @param min
 * @param max
 * @return int32
 */
 int32 irand(int32 min, int32 max);

/**
 * @brief Return a random number in the range min..max (inclusive).
 *
 * For reliable results, the difference between max and min should be less than
 * RAND32_MAX.
 *
 * @param min
 * @param max
 * @return uint32
 */
 uint32 urand(uint32 min, uint32 max);

/**
 * @brief Return a random number in the range min..max (inclusive).
 *
 * @param min
 * @param max
 * @return float
 */
 float frand(float min, float max);

/**
 * @brief Return a random number in the range 0 .. RAND32_MAX.
 *
 * @return int32
 */
uint32 rand32();

/**
 * @brief Return a random double from 0.0 to 1.0 (exclusive).
 *
 * Floats support only 7 valid decimal digits. A double supports up to 15 valid
 * decimaldigits and is used internally (RAND32_MAX has 10 digits). With an FPU
 * there is usually no difference in performance between float and double.
 *
 * @return double
 */
 double rand_norm(void);

/**
 * @brief
 *
 * @return float
 */
 float rand_norm_f(void);

/**
 * @brief Return a random double from 0.0 to 99.9999999999999.
 *
 * Floats support only 7 valid decimal digits. A double supports up to 15 valid
 * decimal digits and is used internaly (RAND32_MAX has 10 digits). With an FPU
 * there is usually no difference in performance between float and double.
 *
 * @return double
 */
 double rand_chance(void);

/**
 * @brief
 *
 * @return float
 */
 float rand_chance_f(void);

/**
 * @brief Return true if a random roll gets above the given chance
 *
 * ie: if the given value is 0 the chance to succeed is also 0 which gives false
 * as the return value at all cases. On the other hand, giving 100 as the chance
 * will make sure that we always succeed
 *
 * @param chance How big the chance to succeed is. Value between 0.0f-100.0f
 * @return bool Return true if a random roll fits in the specified chance (range 0-100).
 */
inline bool roll_chance_f(float chance)
{
    return chance > rand_chance();
}

/**
 * @brief Return true if a random roll fits in the specified chance (range 0-100).
 *
 * @param chance
 * @return bool
 */
inline bool roll_chance_i(int chance)
{
    return chance > irand(0, 99);
}

/**
 * @brief
 *
 * @param var
 * @param val
 * @param apply
 */
inline void ApplyModUInt32Var(uint32& var, int32 val, bool apply)
{
    int32 cur = var;
    cur += (apply ? val : -val);
    if (cur < 0)
        { cur = 0; }
    var = cur;
}

/**
 * @brief
 *
 * @param var
 * @param val
 * @param apply
 */
inline void ApplyModFloatVar(float& var, float  val, bool apply)
{
    var += (apply ? val : -val);
    if (var < 0)
        { var = 0; }
}

/**
 * @brief
 *
 * @param var
 * @param val
 * @param apply
 */
inline void ApplyPercentModFloatVar(float& var, float val, bool apply)
{
    if (val == -100.0f)     // prevent set var to zero
        { val = -99.99f; }
    var *= (apply ? (100.0f + val) / 100.0f : 100.0f / (100.0f + val));
}

/**
 * @brief
 *
 * @param utf8str
 * @param wstr
 * @return bool
 */
bool Utf8toWStr(const std::string& utf8str, std::wstring& wstr);
/**
 * @brief
 *
 * @param utf8str
 * @param csize
 * @param wstr
 * @param wsize
 * @return bool
 */
bool Utf8toWStr(char const* utf8str, size_t csize, wchar_t* wstr, size_t& wsize);
/**
 * @brief
 *
 * @param utf8str
 * @param wstr
 * @param wsize
 * @return bool
 */
inline bool Utf8toWStr(const std::string& utf8str, wchar_t* wstr, size_t& wsize)
{
    return Utf8toWStr(utf8str.c_str(), utf8str.size(), wstr, wsize);
}

/**
 * @brief
 *
 * @param wstr
 * @param utf8str
 * @return bool
 */
bool WStrToUtf8(std::wstring wstr, std::string& utf8str);
/**
 * @brief
 *
 * @param wstr
 * @param size
 * @param utf8str
 * @return bool
 */
bool WStrToUtf8(wchar_t* wstr, size_t size, std::string& utf8str);

/**
 * @brief
 *
 * @param utf8str
 * @return size_t
 */
size_t utf8length(std::string& utf8str);                    // set string to "" if invalid utf8 sequence
/**
 * @brief
 *
 * @param utf8str
 * @param len
 */
void utf8truncate(std::string& utf8str, size_t len);

/**
 * @brief
 *
 * @param wchar
 * @return bool
 */
inline bool isBasicLatinCharacter(wchar_t wchar)
{
    if (wchar >= L'a' && wchar <= L'z')                     // LATIN SMALL LETTER A - LATIN SMALL LETTER Z
        { return true; }
    if (wchar >= L'A' && wchar <= L'Z')                     // LATIN CAPITAL LETTER A - LATIN CAPITAL LETTER Z
        { return true; }
    return false;
}

/**
 * @brief
 *
 * @param wchar
 * @return bool
 */
inline bool isExtendedLatinCharacter(wchar_t wchar)
{
    if (isBasicLatinCharacter(wchar))
        { return true; }
    if (wchar >= 0x00C0 && wchar <= 0x00D6)                 // LATIN CAPITAL LETTER A WITH GRAVE - LATIN CAPITAL LETTER O WITH DIAERESIS
        { return true; }
    if (wchar >= 0x00D8 && wchar <= 0x00DF)                 // LATIN CAPITAL LETTER O WITH STROKE - LATIN CAPITAL LETTER THORN
        { return true; }
    if (wchar == 0x00DF)                                    // LATIN SMALL LETTER SHARP S
        { return true; }
    if (wchar >= 0x00E0 && wchar <= 0x00F6)                 // LATIN SMALL LETTER A WITH GRAVE - LATIN SMALL LETTER O WITH DIAERESIS
        { return true; }
    if (wchar >= 0x00F8 && wchar <= 0x00FE)                 // LATIN SMALL LETTER O WITH STROKE - LATIN SMALL LETTER THORN
        { return true; }
    if (wchar >= 0x0100 && wchar <= 0x012F)                 // LATIN CAPITAL LETTER A WITH MACRON - LATIN SMALL LETTER I WITH OGONEK
        { return true; }
    if (wchar == 0x1E9E)                                    // LATIN CAPITAL LETTER SHARP S
        { return true; }
    return false;
}

/**
 * @brief
 *
 * @param wchar
 * @return bool
 */
inline bool isCyrillicCharacter(wchar_t wchar)
{
    if (wchar >= 0x0410 && wchar <= 0x044F)                 // CYRILLIC CAPITAL LETTER A - CYRILLIC SMALL LETTER YA
        { return true; }
    if (wchar == 0x0401 || wchar == 0x0451)                 // CYRILLIC CAPITAL LETTER IO, CYRILLIC SMALL LETTER IO
        { return true; }
    return false;
}

/**
 * @brief
 *
 * @param wchar
 * @return bool
 */
inline bool isEastAsianCharacter(wchar_t wchar)
{
    if (wchar >= 0x1100 && wchar <= 0x11F9)                 // Hangul Jamo
        { return true; }
    if (wchar >= 0x3041 && wchar <= 0x30FF)                 // Hiragana + Katakana
        { return true; }
    if (wchar >= 0x3131 && wchar <= 0x318E)                 // Hangul Compatibility Jamo
        { return true; }
    if (wchar >= 0x31F0 && wchar <= 0x31FF)                 // Katakana Phonetic Ext.
        { return true; }
    if (wchar >= 0x3400 && wchar <= 0x4DB5)                 // CJK Ideographs Ext. A
        { return true; }
    if (wchar >= 0x4E00 && wchar <= 0x9FC3)                 // Unified CJK Ideographs
        { return true; }
    if (wchar >= 0xAC00 && wchar <= 0xD7A3)                 // Hangul Syllables
        { return true; }
    if (wchar >= 0xFF01 && wchar <= 0xFFEE)                 // Halfwidth forms
        { return true; }
    return false;
}

/**
 * @brief
 *
 * @param c
 * @return bool
 */
inline bool isWhiteSpace(char c)
{
    return ::isspace(int(c)) != 0;
}

/**
 * @brief
 *
 * @param wchar
 * @return bool
 */
inline bool isNumeric(wchar_t wchar)
{
    return (wchar >= L'0' && wchar <= L'9');
}

/**
 * @brief
 *
 * @param c
 * @return bool
 */
inline bool isNumeric(char c)
{
    return (c >= '0' && c <= '9');
}

/**
 * @brief
 *
 * @param wchar
 * @return bool
 */
inline bool isNumericOrSpace(wchar_t wchar)
{
    return isNumeric(wchar) || wchar == L' ';
}

/**
 * @brief
 *
 * @param str
 * @return bool
 */
inline bool isNumeric(char const* str)
{
    for (char const* c = str; *c; ++c)
        if (!isNumeric(*c))
            { return false; }

    return true;
}

/**
 * @brief
 *
 * @param str
 * @return bool
 */
inline bool isNumeric(std::string const& str)
{
    for (std::string::const_iterator itr = str.begin(); itr != str.end(); ++itr)
        if (!isNumeric(*itr))
            { return false; }

    return true;
}

/**
 * @brief
 *
 * @param str
 * @return bool
 */
inline bool isNumeric(std::wstring const& str)
{
    for (std::wstring::const_iterator itr = str.begin(); itr != str.end(); ++itr)
        if (!isNumeric(*itr))
            { return false; }

    return true;
}

/**
 * @brief
 *
 * @param wstr
 * @param numericOrSpace
 * @return bool
 */
inline bool isBasicLatinString(const std::wstring &wstr, bool numericOrSpace)
{
    for (size_t i = 0; i < wstr.size(); ++i)
        if (!isBasicLatinCharacter(wstr[i]) && (!numericOrSpace || !isNumericOrSpace(wstr[i])))
            { return false; }
    return true;
}

/**
 * @brief
 *
 * @param wstr
 * @param numericOrSpace
 * @return bool
 */
inline bool isExtendedLatinString(const std::wstring &wstr, bool numericOrSpace)
{
    for (size_t i = 0; i < wstr.size(); ++i)
        if (!isExtendedLatinCharacter(wstr[i]) && (!numericOrSpace || !isNumericOrSpace(wstr[i])))
            { return false; }
    return true;
}

/**
 * @brief
 *
 * @param wstr
 * @param numericOrSpace
 * @return bool
 */
inline bool isCyrillicString(const std::wstring &wstr, bool numericOrSpace)
{
    for (size_t i = 0; i < wstr.size(); ++i)
        if (!isCyrillicCharacter(wstr[i]) && (!numericOrSpace || !isNumericOrSpace(wstr[i])))
            { return false; }
    return true;
}

/**
 * @brief
 *
 * @param wstr
 * @param numericOrSpace
 * @return bool
 */
inline bool isEastAsianString(const std::wstring &wstr, bool numericOrSpace)
{
    for (size_t i = 0; i < wstr.size(); ++i)
        if (!isEastAsianCharacter(wstr[i]) && (!numericOrSpace || !isNumericOrSpace(wstr[i])))
            { return false; }
    return true;
}

/**
 * @brief
 *
 * @param str
 */
inline void strToUpper(std::string& str)
{
    std::transform(str.begin(), str.end(), str.begin(), toupper);
}

/**
 * @brief
 *
 * @param str
 */
inline void strToLower(std::string& str)
{
    std::transform(str.begin(), str.end(), str.begin(), tolower);
}

/**
 * @brief
 *
 * @param wchar
 * @return wchar_t
 */
inline wchar_t wcharToUpper(wchar_t wchar)
{
    if (wchar >= L'a' && wchar <= L'z')                     // LATIN SMALL LETTER A - LATIN SMALL LETTER Z
        { return wchar_t(uint16(wchar) - 0x0020); }
    if (wchar == 0x00DF)                                    // LATIN SMALL LETTER SHARP S
        { return wchar_t(0x1E9E); }
    if (wchar >= 0x00E0 && wchar <= 0x00F6)                 // LATIN SMALL LETTER A WITH GRAVE - LATIN SMALL LETTER O WITH DIAERESIS
        { return wchar_t(uint16(wchar) - 0x0020); }
    if (wchar >= 0x00F8 && wchar <= 0x00FE)                 // LATIN SMALL LETTER O WITH STROKE - LATIN SMALL LETTER THORN
        { return wchar_t(uint16(wchar) - 0x0020); }
    if (wchar >= 0x0101 && wchar <= 0x012F)                 // LATIN SMALL LETTER A WITH MACRON - LATIN SMALL LETTER I WITH OGONEK (only %2=1)
    {
        if (wchar % 2 == 1)
            { return wchar_t(uint16(wchar) - 0x0001); }
    }
    if (wchar >= 0x0430 && wchar <= 0x044F)                 // CYRILLIC SMALL LETTER A - CYRILLIC SMALL LETTER YA
        { return wchar_t(uint16(wchar) - 0x0020); }
    if (wchar == 0x0451)                                    // CYRILLIC SMALL LETTER IO
        { return wchar_t(0x0401); }

    return wchar;
}

/**
 * @brief
 *
 * @param wchar
 * @return wchar_t
 */
inline wchar_t wcharToUpperOnlyLatin(wchar_t wchar)
{
    return isBasicLatinCharacter(wchar) ? wcharToUpper(wchar) : wchar;
}

/**
 * @brief
 *
 * @param wchar
 * @return wchar_t
 */
inline wchar_t wcharToLower(wchar_t wchar)
{
    if (wchar >= L'A' && wchar <= L'Z')                     // LATIN CAPITAL LETTER A - LATIN CAPITAL LETTER Z
        { return wchar_t(uint16(wchar) + 0x0020); }
    if (wchar >= 0x00C0 && wchar <= 0x00D6)                 // LATIN CAPITAL LETTER A WITH GRAVE - LATIN CAPITAL LETTER O WITH DIAERESIS
        { return wchar_t(uint16(wchar) + 0x0020); }
    if (wchar >= 0x00D8 && wchar <= 0x00DE)                 // LATIN CAPITAL LETTER O WITH STROKE - LATIN CAPITAL LETTER THORN
        { return wchar_t(uint16(wchar) + 0x0020); }
    if (wchar >= 0x0100 && wchar <= 0x012E)                 // LATIN CAPITAL LETTER A WITH MACRON - LATIN CAPITAL LETTER I WITH OGONEK (only %2=0)
    {
        if (wchar % 2 == 0)
            { return wchar_t(uint16(wchar) + 0x0001); }
    }
    if (wchar == 0x1E9E)                                    // LATIN CAPITAL LETTER SHARP S
        { return wchar_t(0x00DF); }
    if (wchar == 0x0401)                                    // CYRILLIC CAPITAL LETTER IO
        { return wchar_t(0x0451); }
    if (wchar >= 0x0410 && wchar <= 0x042F)                 // CYRILLIC CAPITAL LETTER A - CYRILLIC CAPITAL LETTER YA
        { return wchar_t(uint16(wchar) + 0x0020); }

    return wchar;
}

/**
 * @brief
 *
 * @param str
 */
inline void wstrToUpper(std::wstring& str)
{
    std::transform(str.begin(), str.end(), str.begin(), wcharToUpper);
}

/**
 * @brief
 *
 * @param str
 */
inline void wstrToLower(std::wstring& str)
{
    std::transform(str.begin(), str.end(), str.begin(), wcharToLower);
}

/**
 * @brief
 *
 * @param utf8str
 * @param conStr
 * @return bool
 */
bool utf8ToConsole(const std::string& utf8str, std::string& conStr);
/**
 * @brief
 *
 * @param conStr
 * @param utf8str
 * @return bool
 */
bool consoleToUtf8(const std::string& conStr, std::string& utf8str);
/**
 * @brief
 *
 * @param str
 * @param search
 * @return bool
 */
bool Utf8FitTo(const std::string& str, std::wstring search);
/**
 * @brief
 *
 * @param out
 * @param str...
 */
void utf8printf(FILE* out, const char* str, ...);
/**
 * @brief
 *
 * @param str
 */
void utf8print(void* /*arg*/, const char* str);
/**
 * @brief
 *
 * @param out
 * @param str
 * @param ap
 */
void vutf8printf(FILE* out, const char* str, va_list* ap);

/**
 * @brief
 *
 * @param ipaddress
 * @return bool
 */
bool IsIPAddress(char const* ipaddress);

/// Checks if address belongs to the a network with specified submask
bool IsIPAddrInNetwork(ACE_INET_Addr const& net, ACE_INET_Addr const& addr, ACE_INET_Addr const& subnetMask);

/// Transforms ACE_INET_Addr address into string format "dotted_ip:port"
std::string GetAddressString(ACE_INET_Addr const& addr);

/**
 * @brief
 *
 * @param filename
 * @return uint32
 */
uint32 CreatePIDFile(const std::string& filename);

/**
 * @brief
 *
 * @param bytes
 * @param arrayLen
 * @param result
 */
void hexEncodeByteArray(uint8* bytes, uint32 arrayLen, std::string& result);

std::string ByteArrayToHexStr(uint8 const* bytes, uint32 length, bool reverse = false);
void HexStrToByteArray(std::string const& str, uint8* out, bool reverse = false);

/**
* @brief Define iCoreNumber to be set for the currently defined core
*
* @return int
*/
int return_iCoreNumber();

/**
* @brief Display the startup banner
*/
void print_banner();
#endif
