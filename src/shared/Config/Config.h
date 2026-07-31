/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
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
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Policies/Singleton.h>
#include "Platform/Define.h"

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

/**
 * @brief Manages configuration file loading and value retrieval
 *
 * Config class provides a singleton interface for reading and managing server configuration
 * settings from configuration files. It supports multiple data types (string, bool, int, float)
 * with default values and automatic caching.
 */
class Config
{
    public:
        /**
         * @brief Constructs a new Config instance
         *
         * Initializes the configuration manager with no configuration loaded.
         */
        Config();

        ~Config();

        /**
         * @brief Sets the source configuration file
         *
         * Loads configuration from the specified file. The file is parsed and stored
         * for subsequent value retrievals.
         *
         * @param file Path to the configuration file to load
         * @return bool True if file was loaded successfully, false otherwise
         */
        bool SetSource(const char* file);
        /**
         * @brief Reloads the configuration from the source file
         *
         * Refreshes all configuration values from the previously set source file.
         * Useful for applying configuration changes without restarting the server.
         *
         * @return bool True if reload was successful, false otherwise
         */
        bool Reload();

        /**
         * @brief Retrieves a string configuration value with default fallback
         *
         * Looks up a configuration parameter by name and returns its string value.
         * If the parameter is not found, returns the provided default value.
         *
         * @param name Configuration parameter name
         * @param def Default string value if parameter not found
         * @return std::string The configuration value or default value
         */
        std::string GetStringDefault(const char* name, const char* def);
        /**
         * @brief Retrieves a boolean configuration value with default fallback
         *
         * Looks up a configuration parameter by name and interprets it as a boolean.
         * If the parameter is not found, returns the provided default value.
         *
         * @param name Configuration parameter name
         * @param def Default boolean value if parameter not found (default: false)
         * @return bool The configuration value or default value
         */
        bool GetBoolDefault(const char* name, const bool def = false);
        /**
         * @brief Retrieves an integer configuration value with default fallback
         *
         * Looks up a configuration parameter by name and interprets it as a 32-bit integer.
         * If the parameter is not found, returns the provided default value.
         *
         * @param name Configuration parameter name
         * @param def Default integer value if parameter not found
         * @return int32 The configuration value or default value
         */
        int32 GetIntDefault(const char* name, const int32 def);
        /**
         * @brief Retrieves a 64-bit integer configuration value with default fallback
         *
         * @param name Key name to look up
         * @param def Default value if the key is absent or unparsable
         * @return int64 The configuration value or default value
         */
        int64 GetInt64Default(const char* name, const int64 def);
        /**
         * @brief Retrieves a floating-point configuration value with default fallback
         *
         * Looks up a configuration parameter by name and interprets it as a float.
         * If the parameter is not found, returns the provided default value.
         *
         * @param name Configuration parameter name
         * @param def Default floating-point value if parameter not found
         * @return float The configuration value or default value
         */
        float GetFloatDefault(const char* name, const float def);

        /**
         * @brief Gets the source configuration filename
         *
         * @return std::string The path to the currently loaded configuration file
         */
        std::string GetFilename() const { return mFilename; }

    private:

        /**
         * @brief Orders keys ignoring case, so lookup is case-insensitive.
         *
         * Config keys have never been consistently cased: the core reads
         * `RA.Secure` and `GMLogFile` while every shipped config since Release
         * 20 has written `Ra.Secure` and `GmLogFile`. That resolved for years,
         * only stopped when the config backend became an exact-match std::map.
         * Comparing case-insensitively restores it without touching either
         * spelling, and covers any other drift of the same kind.
         *
         * ASCII-only by design: config keys are ASCII identifiers, and a
         * locale-sensitive fold would make resolution depend on the run
         * environment.
         */
        struct KeyLess
        {
            bool operator()(const std::string& lhs,
                            const std::string& rhs) const
            {
                return std::lexicographical_compare(
                           lhs.begin(), lhs.end(), rhs.begin(), rhs.end(),
                           [](unsigned char a, unsigned char b)
                           {
                               return AsciiLower(a) < AsciiLower(b);
                           });
            }

            static unsigned char AsciiLower(unsigned char c)
            {
                return (c >= 'A' && c <= 'Z')
                       ? (unsigned char)(c - 'A' + 'a')
                       : c;
            }
        };

        /// One [section] and its key/value pairs. Within a section the last assignment
        /// of a key wins; across sections the first section holding the key wins (see
        /// GetValue). Sections are kept in file order so that resolution is stable.
        /// Keys compare case-insensitively; see KeyLess.
        typedef std::map<std::string, std::string, KeyLess> SectionEntries;
        typedef std::vector<std::pair<std::string, SectionEntries> > Sections;

        /// Look @p name up across every section, first match wins.
        bool GetValue(const char* name, std::string& result) const;

        /// Warn that two spellings in one section are the same setting.
        void WarnDuplicateKey(const std::string& section,
                              const std::string& kept,
                              const std::string& dropped) const;

        /// Warn where an earlier section answers a later section's key.
        void WarnShadowedKeys() const;

    private:

        std::string mFilename; /**< Path to the currently loaded configuration file */
        Sections    mSections; /**< Parsed contents of mFilename, in file order */
        bool        mLoaded;   /**< True once a file has been parsed successfully */
};

#define sConfig MaNGOS::Singleton<Config>::Instance()

#endif
