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
 * server configuration from INI format files using the ACE configuration
 * framework.
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
#include <ace/Configuration_Import_Export.h>

#include "Policies/Singleton.h"

INSTANTIATE_SINGLETON_1(Config);

/**
 * @brief Search all sections for a configuration value
 * @param mConf Configuration heap to search
 * @param name Key name to find
 * @param result Output string for value
 * @return true if found, false otherwise
 *
 * Searches through all sections in the INI file to find the specified
 * key. Returns the first match found (sections are enumerated in order).
 */
static bool GetValueHelper(ACE_Configuration_Heap* mConf, const char* name, ACE_TString& result)
{
    if (!mConf)
    {
        return false;
    }

    ACE_TString section_name;
    ACE_Configuration_Section_Key section_key;
    ACE_Configuration_Section_Key root_key = mConf->root_section();

    int i = 0;
    while (mConf->enumerate_sections(root_key, i, section_name) == 0)
    {
        mConf->open_section(root_key, section_name.c_str(), 0, section_key);
        if (mConf->get_string_value(section_key, name, result) == 0)
        {
            return true;
        }
        ++i;
    }

    return false;
}

/**
 * @brief Construct Config singleton
 *
 * Initializes with no loaded configuration. Use SetSource() to load a file.
 */
Config::Config()
    : mConf(NULL)
{
}

/**
 * @brief Destroy Config singleton
 *
 * Cleans up the ACE configuration heap.
 */
Config::~Config()
{
    delete mConf;
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
    delete mConf;
    mConf = new ACE_Configuration_Heap;

    if (mConf->open() == 0)
    {
        ACE_Ini_ImpExp config_importer(*mConf);
        if (config_importer.import_config(mFilename.c_str()) == 0)
        {
            return true;
        }
    }

    delete mConf;
    mConf = NULL;
    return false;
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
    ACE_TString val;
    return GetValueHelper(mConf, name, val) ? val.c_str() : def;
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
    ACE_TString val;
    if (!GetValueHelper(mConf, name, val))
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
    ACE_TString val;
    return GetValueHelper(mConf, name, val) ? atoi(val.c_str()) : def;
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
    ACE_TString val;
    return GetValueHelper(mConf, name, val) ? (float)atof(val.c_str()) : def;
}
