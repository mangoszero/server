/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2022 MaNGOS <https://getmangos.eu>
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

#include "Util.h"
#include "Timer.h"

#include "utf8.h"
#include "RNGen.h"
#include "Log/Log.h"

#include <iomanip>

//////////////////////////////////////////////////////////////////////////
int32 irand(int32 min, int32 max)
{
    return RNG::instance()->rand_i(min,max);
}

uint32 urand(uint32 min, uint32 max)
{
    return RNG::instance()->rand_u(min,max);
}

float frand(float min, float max)
{
    return RNG::instance()->rand_f(min, max);
}

uint32 rand32()
{
    return RNG::instance()->rand();
}

double rand_norm(void)
{
    return RNG::instance()->rand_d(0.0, 1.0);
}

float rand_norm_f(void)
{
    return RNG::instance()->rand_f(0.0, 1.0);
}

double rand_chance(void)
{
    return RNG::instance()->rand_d(0.0, 100.0);
}

float rand_chance_f(void)
{
    return RNG::instance()->rand_f(0.0, 100.0);
}

Tokens StrSplit(const std::string& src, const std::string& sep)
{
    Tokens r;
    std::string s;
    for (std::string::const_iterator i = src.begin(); i != src.end(); ++i)
    {
        if (sep.find(*i) != std::string::npos)
        {
            if (s.length())
            {
                r.push_back(s);
            }
            s = "";
        }
        else
        {
            s += *i;
        }
    }
    if (s.length())
    {
        r.push_back(s);
    }
    return r;
}

uint32 GetUInt32ValueFromArray(Tokens const& data, uint16 index)
{
    if (index >= data.size())
    {
        return 0;
    }

    return (uint32)atoi(data[index].c_str());
}

float GetFloatValueFromArray(Tokens const& data, uint16 index)
{
    float result;
    uint32 temp = GetUInt32ValueFromArray(data, index);
    memcpy(&result, &temp, sizeof(result));

    return result;
}

void stripLineInvisibleChars(std::string& str)
{
    static std::string invChars = " \t\7\n";

    size_t wpos = 0;

    bool space = false;
    for (size_t pos = 0; pos < str.size(); ++pos)
    {
        if (invChars.find(str[pos]) != std::string::npos)
        {
            if (!space)
            {
                str[wpos++] = ' ';
                space = true;
            }
        }
        else
        {
            if (wpos != pos)
            {
                str[wpos++] = str[pos];
            }
            else
            {
                ++wpos;
            }
            space = false;
        }
    }

    if (wpos < str.size())
    {
        str.erase(wpos, str.size());
    }
}

std::tm localtime_r(const time_t& time)
{
    std::tm tm_snapshot;
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32__))
    localtime_s(&tm_snapshot, &time);
#else
    localtime_r(&time, &tm_snapshot); // POSIX
#endif
    return tm_snapshot;
}

std::string secsToTimeString(time_t timeInSecs, TimeFormat timeFormat, bool hoursOnly)
{
    time_t secs    = timeInSecs % MINUTE;
    time_t minutes = timeInSecs % HOUR / MINUTE;
    time_t hours   = timeInSecs % DAY  / HOUR;
    time_t days    = timeInSecs / DAY;

    std::ostringstream ss;
    if (days)
    {
        ss << days;
        if (timeFormat == TimeFormat::Numeric)
        {
            ss << ":";
        }
        else if (timeFormat == TimeFormat::ShortText)
        {
            ss << "d";
        }
        else // if (timeFormat == TimeFormat::FullText)
        {
            if (days == 1)
            {
                ss << " Day ";
            }
            else
            {
                ss << " Days ";
            }
        }
    }

    if (hours || hoursOnly)
    {
        ss << hours;
        if (timeFormat == TimeFormat::Numeric)
        {
            ss << ":";
        }
        else if (timeFormat == TimeFormat::ShortText)
        {
            ss << "h";
        }
        else // if (timeFormat == TimeFormat::FullText)
        {
            if (hours <= 1)
            {
                ss << " Hour ";
            }
            else
            {
                ss << " Hours ";
            }
        }
    }

    if (!hoursOnly)
    {
        ss << minutes;
        if (timeFormat == TimeFormat::Numeric)
        {
            ss << ":";
        }
        else if (timeFormat == TimeFormat::ShortText)
        {
            ss << "m";
        }
        else // if (timeFormat == TimeFormat::FullText)
        {
            if (minutes == 1)
            {
                ss << " Minute ";
            }
            else
            {
                ss << " Minutes ";
            }
        }
    }
    else
    {
        if (timeFormat == TimeFormat::Numeric)
        {
            ss << "0:";
        }
    }

    if (secs || (!days && !hours && !minutes))
    {
        ss << std::setw(2) << std::setfill('0') << secs;
        if (timeFormat == TimeFormat::ShortText)
        {
            ss << "s";
        }
        else if (timeFormat == TimeFormat::FullText)
        {
            if (secs <= 1)
            {
                ss << " Second.";
            }
            else
            {
                ss << " Seconds.";
            }
        }
    }
    else
    {
        if (timeFormat == TimeFormat::Numeric)
        {
            ss << "00";
        }
    }

    return ss.str();
}

uint32 TimeStringToSecs(const std::string& timestring)
{
    uint32 secs       = 0;
    uint32 buffer     = 0;
    uint32 multiplier = 0;

    for (std::string::const_iterator itr = timestring.begin(); itr != timestring.end(); ++itr)
    {
        if (isdigit(*itr))
        {
            buffer *= 10;
            buffer += (*itr) - '0';
        }
        else
        {
            switch (*itr)
            {
                case 'd': multiplier = DAY;     break;
                case 'h': multiplier = HOUR;    break;
                case 'm': multiplier = MINUTE;  break;
                case 's': multiplier = 1;       break;
                default : return 0;                         // bad format
            }
            buffer *= multiplier;
            secs += buffer;
            buffer = 0;
        }
    }

    return secs;
}

std::string TimeToTimestampStr(time_t t)
{
    tm aTm = localtime_r(t);
    //       YYYY   year
    //       MM     month (2 digits 01-12)
    //       DD     day (2 digits 01-31)
    //       HH     hour (2 digits 00-23)
    //       MM     minutes (2 digits 00-59)
    //       SS     seconds (2 digits 00-59)
    char buf[20];
    snprintf(buf, 20, "%04d-%02d-%02d_%02d-%02d-%02d", aTm.tm_year + 1900, aTm.tm_mon + 1, aTm.tm_mday, aTm.tm_hour, aTm.tm_min, aTm.tm_sec);
    return std::string(buf);
}

/// Check if the string is a valid ip address representation
bool IsIPAddress(char const* ipaddress)
{
    if (!ipaddress)
    {
        return false;
    }

    // Let the big boys do it.
    // Drawback: all valid ip address formats are recognized e.g.: 12.23,121234,0xABCD)
    return ACE_OS::inet_addr(ipaddress) != INADDR_NONE;
}

std::string GetAddressString(ACE_INET_Addr const& addr)
{
    char buf[ACE_MAX_FULLY_QUALIFIED_NAME_LEN + 16];
    addr.addr_to_string(buf, ACE_MAX_FULLY_QUALIFIED_NAME_LEN + 16);
    return buf;
}

bool IsIPAddrInNetwork(ACE_INET_Addr const& net, ACE_INET_Addr const& addr, ACE_INET_Addr const& subnetMask)
{
    uint32 mask = subnetMask.get_ip_address();
    if ((net.get_ip_address() & mask) == (addr.get_ip_address() & mask))
    {
        return true;
    }
    return false;
}

/// create PID file
uint32 CreatePIDFile(const std::string& filename)
{
    FILE* pid_file = fopen(filename.c_str(), "w");
    if (pid_file == NULL)
    {
        return 0;
    }

#ifdef WIN32
    DWORD pid = GetCurrentProcessId();
#else
    pid_t pid = getpid();
#endif

    fprintf(pid_file, "%d", pid);
    fclose(pid_file);

    return (uint32)pid;
}

size_t utf8length(std::string& utf8str)
{
    try
    {
        return utf8::distance(utf8str.c_str(), utf8str.c_str() + utf8str.size());
    }
    catch (std::exception)
    {
        utf8str = "";
        return 0;
    }
}

void utf8truncate(std::string& utf8str, size_t len)
{
    try
    {
        size_t wlen = utf8::distance(utf8str.c_str(), utf8str.c_str() + utf8str.size());
        if (wlen <= len)
        {
            return;
        }

        std::wstring wstr;
        wstr.resize(wlen);
        utf8::utf8to16(utf8str.c_str(), utf8str.c_str() + utf8str.size(), &wstr[0]);
        wstr.resize(len);
        char* oend = utf8::utf16to8(wstr.c_str(), wstr.c_str() + wstr.size(), &utf8str[0]);
        utf8str.resize(oend - (&utf8str[0]));               // remove unused tail
    }
    catch (std::exception)
    {
        utf8str = "";
    }
}

size_t utf8limit(std::string& utf8str, size_t bytes)
{
    if (utf8str.size() > bytes)
    {
        try
        {
            auto end = (utf8str.cbegin() + bytes);
            auto itr = utf8::find_invalid(utf8str.cbegin(), end);

            // Fix UTF8 if it was corrupted by bytes truncated
            if (itr != end)
                bytes = std::distance(utf8str.cbegin(), itr);

            utf8str.resize(bytes);
            utf8str.shrink_to_fit();

            return bytes;
        }
        catch (const std::exception&)
        {
            utf8str = "";
        }
    }

    return 0;
}

bool Utf8toWStr(char const* utf8str, size_t csize, wchar_t* wstr, size_t& wsize)
{
    try
    {
        size_t len = utf8::distance(utf8str, utf8str + csize);
        if (len > wsize)
        {
            if (wsize > 0)
            {
                wstr[0] = L'\0';
            }
            wsize = 0;
            return false;
        }

        wsize = len;
        utf8::utf8to16(utf8str, utf8str + csize, wstr);
        wstr[len] = L'\0';
    }
    catch (std::exception)
    {
        if (wsize > 0)
        {
            wstr[0] = L'\0';
        }
        wsize = 0;
        return false;
    }

    return true;
}

bool Utf8toWStr(const std::string& utf8str, std::wstring& wstr)
{
    try
    {
        size_t len = utf8::distance(utf8str.c_str(), utf8str.c_str() + utf8str.size());
        wstr.resize(len);

        if (len)
        {
            utf8::utf8to16(utf8str.c_str(), utf8str.c_str() + utf8str.size(), &wstr[0]);
        }
    }
    catch (std::exception)
    {
        wstr = L"";
        return false;
    }

    return true;
}

bool WStrToUtf8(wchar_t* wstr, size_t size, std::string& utf8str)
{
    try
    {
        std::string utf8str2;
        utf8str2.resize(size * 4);                          // allocate for most long case

        char* oend = utf8::utf16to8(wstr, wstr + size, &utf8str2[0]);
        utf8str2.resize(oend - (&utf8str2[0]));             // remove unused tail
        utf8str = utf8str2;
    }
    catch (std::exception)
    {
        utf8str = "";
        return false;
    }

    return true;
}

bool WStrToUtf8(std::wstring wstr, std::string& utf8str)
{
    try
    {
        std::string utf8str2;
        utf8str2.resize(wstr.size() * 4);                   // allocate for most long case

        char* oend = utf8::utf16to8(wstr.c_str(), wstr.c_str() + wstr.size(), &utf8str2[0]);
        utf8str2.resize(oend - (&utf8str2[0]));             // remove unused tail
        utf8str = utf8str2;
    }
    catch (std::exception)
    {
        utf8str = "";
        return false;
    }

    return true;
}

typedef wchar_t const* const* wstrlist;

bool utf8ToConsole(const std::string& utf8str, std::string& conStr)
{
#if PLATFORM == PLATFORM_WINDOWS
    std::wstring wstr;
    if (!Utf8toWStr(utf8str, wstr))
    {
        return false;
    }

    conStr.resize(wstr.size());
    CharToOemBuffW(&wstr[0], &conStr[0], wstr.size());
#else
    // not implemented yet
    conStr = utf8str;
#endif

    return true;
}

bool consoleToUtf8(const std::string& conStr, std::string& utf8str)
{
#if PLATFORM == PLATFORM_WINDOWS
    std::wstring wstr;
    wstr.resize(conStr.size());
    OemToCharBuffW(&conStr[0], &wstr[0], conStr.size());

    return WStrToUtf8(wstr, utf8str);
#else
    // not implemented yet
    utf8str = conStr;
    return true;
#endif

}

bool Utf8FitTo(const std::string& str, std::wstring search)
{
    std::wstring temp;

    if (!Utf8toWStr(str, temp))
    {
        return false;
    }

    // converting to lower case
    wstrToLower(temp);

    if (temp.find(search) == std::wstring::npos)
    {
        return false;
    }

    return true;
}

void vutf8printf(FILE* out, const char* str, va_list* ap)
{
#if PLATFORM == PLATFORM_WINDOWS
    char temp_buf[32 * 1024];
    wchar_t wtemp_buf[32 * 1024];

    size_t temp_len = vsnprintf(temp_buf, 32 * 1024, str, *ap);

    size_t wtemp_len = 32 * 1024 - 1;
    Utf8toWStr(temp_buf, temp_len, wtemp_buf, wtemp_len);

    CharToOemBuffW(&wtemp_buf[0], &temp_buf[0], wtemp_len + 1);
    fprintf(out, "%s", temp_buf);
#else
    vfprintf(out, str, *ap);
#endif

}

void hexEncodeByteArray(uint8* bytes, uint32 arrayLen, std::string& result)
{
    std::ostringstream ss;
    for (uint32 i = 0; i < arrayLen; ++i)
    {
        for (uint8 j = 0; j < 2; ++j)
        {
            unsigned char nibble = 0x0F & (bytes[i] >> ((1 - j) * 4));
            char encodedNibble;
            if (nibble < 0x0A)
            {
                encodedNibble = '0' + nibble;
            }
            else
            {
                encodedNibble = 'A' + nibble - 0x0A;
            }
            ss << encodedNibble;
        }
    }
    result = ss.str();
}

std::string ByteArrayToHexStr(uint8 const* bytes, uint32 arrayLen, bool reverse /* = false */)
{
    int32 init = 0;
    int32 end = arrayLen;
    int8 op = 1;

    if (reverse)
    {
        init = arrayLen - 1;
        end = -1;
        op = -1;
    }

    std::ostringstream ss;
    for (int32 i = init; i != end; i += op)
    {
        char buffer[4];
        sprintf(buffer, "%02X", bytes[i]);
        ss << buffer;
    }

    return ss.str();
}

void HexStrToByteArray(std::string const& str, uint8* out, bool reverse /*= false*/)
{
    // string must have even number of characters
    if (str.length() & 1)
    {
        return;
    }

    int32 init = 0;
    int32 end = str.length();
    int8 op = 1;

    if (reverse)
    {
        init = str.length() - 2;
        end = -2;
        op = -1;
    }

    uint32 j = 0;
    for (int32 i = init; i != end; i += 2 * op)
    {
        char buffer[3] = { str[i], str[i + 1], '\0' };
        out[j++] = strtoul(buffer, NULL, 16);
    }
}

void utf8print(void* /*arg*/, const char* str)
{
#if PLATFORM == PLATFORM_WINDOWS
    wchar_t wtemp_buf[6000];
    size_t wtemp_len = 6000 - 1;
    if (!Utf8toWStr(str, strlen(str), wtemp_buf, wtemp_len))
    {
        return;
    }

    char temp_buf[6000];
    CharToOemBuffW(&wtemp_buf[0], &temp_buf[0], wtemp_len + 1);
    printf("%s", temp_buf);
#else
    printf("%s", str);
#endif

}

void utf8printf(FILE* out, const char* str, ...)
{
    va_list ap;
    va_start(ap, str);
    vutf8printf(out, str, &ap);
    va_end(ap);
}

int return_iCoreNumber()
{

#if defined(CLASSIC)
    return 0;
#elif defined(TBC)
    return 1;
#elif defined(WOTLK)
    return 2;
#elif defined(CATA)
    return 3;
#elif defined(MOP)
    return 4;
#elif defined(WOD)
    return 5;
#elif defined(LEGION)
    return 6;
#else
    return -1;
#endif

}

/// Print out the core banner
void print_banner()
{
    int iCoreNumber = return_iCoreNumber();
    switch (iCoreNumber)
    {
    case 0: // CLASSIC
        sLog.outString("<Ctrl-C> to stop.\n"
            "  __  __      _  _  ___  ___  ___        ____              \n"
            " |  \\/  |__ _| \\| |/ __|/ _ \\/ __|      /_  /___ _ _ ___   \n"
            " | |\\/| / _` | .` | (_ | (_) \\__ \\       / // -_) '_/ _ \\ \n"
            " |_|  |_\\__,_|_|\\_|\\___|\\___/|___/      /___\\___|_| \\___/\n"
            " Powered By MaNGOS Core\n"
            "__________________________________________________________\n"
            "\n"
            "Website/Forum/Wiki/Issue Tracker: https://www.getmangos.eu\n"
            "__________________________________________________________\n"
            "\n");
        break;
    case 1: // TBC
        sLog.outString("<Ctrl-C> to stop.\n"
            "  __  __      _  _  ___  ___  ___         ___             \n"
            " |  \\/  |__ _| \\| |/ __|/ _ \\/ __|       / _ \\ ___  ___  \n"
            " | |\\/| / _` | .` | (_ | (_) \\__ \\      | (_) |   \\/ -_) \n"
            " |_|  |_\\__,_|_|\\_|\\___|\\___/|___/       \\___/|_||_\\___|\n"
            " Powered By MaNGOS Core\n"
            " __________________________________________________________\n"
            "\n"
            " Website/Forum/Wiki/Issue Tracker: https://www.getmangos.eu\n"
            " __________________________________________________________\n"
            "\n");
        break;
    case 2: // WOTLK
        sLog.outString("<Ctrl-C> to stop.\n"
            "  __  __      _  _  ___  ___  ___       _____          \n"
            " |  \\/  |__ _| \\| |/ __|/ _ \\/ __|     |_   _|_ __ _____\n"
            " | |\\/| / _` | .` | (_ | (_) \\__ \\       | | \\ V  V / _ \\\n"
            " |_|  |_\\__,_|_|\\_|\\___|\\___/|___/       |_|  \\_/\\_/\\___/ \n"
            " Powered By MaNGOS Core\n"
            " __________________________________________________________\n"
            "\n"
            " Website/Forum/Wiki/Issue Tracker: https://www.getmangos.eu\n"
            " __________________________________________________________\n"
            "\n");
        break;
    case 3: // CATA
        sLog.outString("<Ctrl-C> to stop.\n"
            "  __  __      _  _  ___  ___  ___   _____ _         \n"
            " |  \\/  |__ _| \\| |/ __|/ _ \\/ __| |_   _| |_  _ _ ___ ___    \n"
            " | |\\/| / _` | .` | (_ | (_) \\__ \\   | | | ' \\| '_/ -_) -_)  \n"
            " |_|  |_\\__,_|_|\\_|\\___|\\___/|___/   |_| |_||_|_| \\___\\___| \n"
            " Powered By MaNGOS Core\n"
            " __________________________________________________________\n"
            "\n"
            " Website/Forum/Wiki/Issue Tracker: https://www.getmangos.eu\n"
            " __________________________________________________________\n"
            "\n");
        break;
    case 4: // MOP
        sLog.outString("<Ctrl-C> to stop.\n"
            "  __  __      _  _  ___  ___  ___     _____             \n"
            " |  \\/  |__ _| \\| |/ __|/ _ \\/ __|    | __|__ _  _ _ _  \n"
            " | |\\/| / _` | .` | (_ | (_) \\__ \\    | _/ _ \\ || | '_|\n"
            " |_|  |_\\__,_|_|\\_|\\___|\\___/|___/    |_|\\___/\\_,_|_| \n"
            " Powered By MaNGOS Core\n"
            " __________________________________________________________\n"
            "\n"
            " Website/Forum/Wiki/Issue Tracker: https://www.getmangos.eu\n"
            " __________________________________________________________\n"
            "\n");
        break;
    default:
        sLog.outString("<Ctrl-C> to stop.\n"
            "  __  __      _  _  ___  ___  ___                                \n"
            " |  \\/  |__ _| \\| |/ __|/ _ \\/ __|     We have a problem !   \n"
            " | |\\/| / _` | .` | (_ | (_) \\__ \\   Your version of MaNGOS  \n"
            " |_|  |_\\__,_|_|\\_|\\___|\\___/|___/   could not be detected   \n"
            " __________________________________________________________\n"
            "\n"
            " Website/Forum/Wiki/Issue Tracker: https://www.getmangos.eu\n"
            " __________________________________________________________\n"
            "\n");
        break;
    }
}
