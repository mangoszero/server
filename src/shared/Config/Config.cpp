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

/**
 * @file Config.cpp
 * @brief INI configuration file parser and storage
 *
 * This file implements the Config singleton for reading and accessing
 * server configuration from INI format files.

 *
 * Features:
 * - INI file format parsing
 * - Section-based configuration organization
 * - Type-safe value retrieval with defaults
 * - Dynamic reload support
 * - Singleton access pattern
 *
 * Supported value types:
 * - String: Raw string values
 * - Bool: true/false, yes/no, 1/0 (case-insensitive)
 * - Int: 32-bit signed integers
 * - Float: Floating point values
 *
 * @see Config for the main configuration interface
 */

#include "Config.h"
#include "Policies/Singleton.h"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <utility>

namespace
{
    /// Strip leading and trailing spaces/tabs/CR.
    std::string Trim(const std::string& s)
    {
        const char* WS = " \t\r\n";

        const std::string::size_type first = s.find_first_not_of(WS);
        if (first == std::string::npos)
        {
            return std::string();
        }

        const std::string::size_type last = s.find_last_not_of(WS);
        return s.substr(first, last - first + 1);
    }

    /**
     * @brief Remove one layer of surrounding double quotes, if present.
     *
     * The config files quote any value that may contain separators — most importantly
     * the database connection strings, which are semicolon-delimited
     * ("127.0.0.1;3306;root;mangos;realmd"). That is also why nothing here treats ';'
     * as a comment introducer: it would cut those values in half.
     */
    std::string Unquote(const std::string& s)
    {
        if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
        {
            return s.substr(1, s.size() - 2);
        }

        return s;
    }
}

Config::Config()
    : mLoaded(false)
{
}

Config::~Config()
{
}

/**
 * @brief Look a key up across every section; the first section holding it wins.
 *
 * Look across sections in file order and
 * returned the first one that had the key.
 */
bool Config::GetValue(const char* name, std::string& result) const
{
    if (!mLoaded)
    {
        return false;
    }

    for (Sections::const_iterator section = mSections.begin(); section != mSections.end(); ++section)
    {
        SectionEntries::const_iterator entry = section->second.find(name);
        if (entry != section->second.end())
        {
            result = entry->second;
            return true;
        }
    }

    return false;
}

/**
 * @brief Set configuration source file
 * @param file Path to INI configuration file
 * @return true on successful load, false on error
 *
 * Sets the source file and immediately attempts to load it.
 * If loading fails, the Config object remains without valid data.
 */
bool Config::SetSource(const char* file)
{
    mFilename = file;

    return Reload();
}

/**
 * @brief Reload configuration from file
 * @return true on success, false on failure
 *
 * Reloads the configuration file, discarding any previous values.
 * On failure, the Config object is left without valid configuration.
 *
 * @note Thread safety: Caller must ensure no concurrent access during reload
 */
bool Config::Reload()
{
    mSections.clear();
    mLoaded = false;

    std::ifstream in(mFilename.c_str());
    if (!in.is_open())
    {
        return false;
    }

    // Keys that appear before any [section] header land in an unnamed leading section,
    // which keeps them reachable from GetValue() rather than silently dropped.
    mSections.push_back(std::make_pair(std::string(), SectionEntries()));

    std::string line;
    while (std::getline(in, line))
    {
        const std::string text = Trim(line);

        if (text.empty() || text[0] == '#' || text[0] == ';')
        {
            continue;
        }

        if (text[0] == '[')
        {
            const std::string::size_type close = text.find(']');
            if (close != std::string::npos)
            {
                mSections.push_back(std::make_pair(Trim(text.substr(1, close - 1)), SectionEntries()));
            }
            continue;
        }

        const std::string::size_type eq = text.find('=');
        if (eq == std::string::npos)
        {
            continue;   // not a key/value line; ignore it
        }

        const std::string key = Trim(text.substr(0, eq));
        if (key.empty())
        {
            continue;
        }

        // Everything after the '=' is the value: no inline-comment handling, because a
        // quoted value may legitimately contain '#' or ';'. The last assignment of a key
        // within a section wins.
        mSections.back().second[key] = Unquote(Trim(text.substr(eq + 1)));
    }

    mLoaded = true;
    return true;
}

/**
 * @brief Get string configuration value
 * @param name Key name to look up
 * @param def Default value if key not found
 * @return Configuration value or default
 *
 * Retrieves a string value from the configuration.
 * Returns default if key doesn't exist.
 */
std::string Config::GetStringDefault(const char* name, const char* def)
{
    std::string val;
    return GetValue(name, val) ? val : std::string(def);
}

/**
 * @brief Get boolean configuration value
 * @param name Key name to look up
 * @param def Default value if key not found or invalid
 * @return Configuration value or default
 *
 * Parses boolean values case-insensitively:
 * - True: "true", "TRUE", "yes", "YES", "1"
 * - False: all other values
 */
bool Config::GetBoolDefault(const char* name, bool def)
{
    std::string val;
    if (!GetValue(name, val))
    {
        return def;
    }

    const char* str = val.c_str();
    if (strcmp(str, "true") == 0 || strcmp(str, "TRUE") == 0 ||
        strcmp(str, "yes") == 0 || strcmp(str, "YES") == 0 ||
        strcmp(str, "1") == 0)
    {
        return true;
    }
    else
    {
        return false;
    }
}

/**
 * @brief Get integer configuration value
 * @param name Key name to look up
 * @param def Default value if key not found or invalid
 * @return Configuration value or default
 *
 * Parses value using atoi(). Returns default if key doesn't exist.
 */
int32 Config::GetIntDefault(const char* name, int32 def)
{
    std::string val;
    return GetValue(name, val) ? atoi(val.c_str()) : def;
}

/**
 * @brief Get float configuration value
 * @param name Key name to look up
 * @param def Default value if key not found or invalid
 * @return Configuration value or default
 *
 * Parses value using atof(). Returns default if key doesn't exist.
 */
float Config::GetFloatDefault(const char* name, float def)
{
    std::string val;
    return GetValue(name, val) ? (float)atof(val.c_str()) : def;
}
