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



#include "ObjectMgr.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "ProgressBar.h"
#include "Util.h"
#include "World.h"
#include "AuctionHouseBot/AhBotSystemOwner.h"
#include "LivingWorldAnchorPolicy.h"
#include "MotionGenerators/MotionMaster.h"
#include "Policies/Singleton.h"
#include "SQLStorages.h"
#include "MapManager.h"
#include "ObjectGuid.h"
#include "ScriptMgr.h"
#include "SpellMgr.h"
#include "Group.h"
#include "Transports.h"
#include "Language.h"
#include "PoolManager.h"
#include "GameEventMgr.h"
#include "Chat.h"
#include "MapPersistentStateMgr.h"
#include "SpellAuras.h"
#include "GossipDef.h"
#include "Mail.h"
#include "Formulas.h"
#include "InstanceData.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "DisableMgr.h"
#include "ItemEnchantmentMgr.h"

/**
 * @brief Loads the list of reserved player names.
 */
void ObjectMgr::LoadReservedPlayersNames()
{
    m_ReservedNames.clear();                                // need for reload case

    QueryResult* result = WorldDatabase.Query("SELECT `name` FROM `reserved_name`");

    uint32 count = 0;

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        // AH bot forged system owner: always reserve its name. In-memory only and
        // run on every load/reload, so a wiped or never-seeded `reserved_name`
        // table cannot free the name for players (enforcement is via IsReservedName,
        // which reads m_ReservedNames).
        std::wstring wsys;
        if (Utf8toWStr(AHBOT_SYSTEM_OWNER_NAME, wsys))
        {
            wstrToLower(wsys);
            if (m_ReservedNames.insert(wsys).second)
            {
                ++count;
            }
        }
        sLog.outString(">> Loaded %u reserved player names", count);
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    Field* fields;
    do
    {
        bar.step();
        fields = result->Fetch();
        std::string name = fields[0].GetCppString();

        std::wstring wstr;
        if (!Utf8toWStr(name, wstr))
        {
            sLog.outError("Table `reserved_name` have invalid name: %s", name.c_str());
            continue;
        }

        wstrToLower(wstr);

        m_ReservedNames.insert(wstr);
        ++count;
    }
    while (result->NextRow());

    delete result;

    // AH bot forged system owner: always reserve its name. In-memory only and
    // run on every load/reload, so a wiped or never-seeded `reserved_name`
    // table cannot free the name for players (enforcement is via IsReservedName,
    // which reads m_ReservedNames).
    std::wstring wsys;
    if (Utf8toWStr(AHBOT_SYSTEM_OWNER_NAME, wsys))
    {
        wstrToLower(wsys);
        if (m_ReservedNames.insert(wsys).second)
        {
            ++count;
        }
    }

    sLog.outString(">> Loaded %u reserved player names", count);
    sLog.outString();
}

/**
 * @brief Checks whether a player name is reserved.
 *
 * @param name The player name.
 * @return true if the name is reserved; otherwise, false.
 */
bool ObjectMgr::IsReservedName(const std::string& name) const
{
    std::wstring wstr;
    if (!Utf8toWStr(name, wstr))
    {
        return false;
    }

    wstrToLower(wstr);

    return m_ReservedNames.find(wstr) != m_ReservedNames.end();
}

enum LanguageType
{
    LT_BASIC_LATIN    = 0x0000,
    LT_EXTENDEN_LATIN = 0x0001,
    LT_CYRILLIC       = 0x0002,
    LT_EAST_ASIA      = 0x0004,
    LT_ANY            = 0xFFFF
};

/**
 * @brief Determines the allowed language family for the current realm.
 *
 * @param create True when validating character creation names; otherwise false.
 * @return LanguageType The realm language mask to apply.
 */
static LanguageType GetRealmLanguageType(bool create)
{
    switch (sWorld.getConfig(CONFIG_UINT32_REALM_ZONE))
    {
        case REALM_ZONE_UNKNOWN:                            // any language
        case REALM_ZONE_DEVELOPMENT:
        case REALM_ZONE_TEST_SERVER:
        case REALM_ZONE_QA_SERVER:
            return LT_ANY;
        case REALM_ZONE_UNITED_STATES:                      // extended-Latin
        case REALM_ZONE_OCEANIC:
        case REALM_ZONE_LATIN_AMERICA:
        case REALM_ZONE_ENGLISH:
        case REALM_ZONE_GERMAN:
        case REALM_ZONE_FRENCH:
        case REALM_ZONE_SPANISH:
            return LT_EXTENDEN_LATIN;
        case REALM_ZONE_KOREA:                              // East-Asian
        case REALM_ZONE_TAIWAN:
        case REALM_ZONE_CHINA:
            return LT_EAST_ASIA;
        case REALM_ZONE_RUSSIAN:                            // Cyrillic
            return LT_CYRILLIC;
        default:
            return create ? LT_BASIC_LATIN : LT_ANY;        // basic-Latin at create, any at login
    }
}

/**
 * @brief Validates a wide-character string against realm language rules.
 *
 * @param wstr The string to validate.
 * @param strictMask The explicit language mask.
 * @param numericOrSpace True if digits or spaces are allowed.
 * @param create True when validating character creation names.
 * @return true if the string is valid; otherwise false.
 */
bool isValidString(const std::wstring &wstr, uint32 strictMask, bool numericOrSpace, bool create = false)
{
    if (strictMask == 0)                                    // any language, ignore realm
    {
        if (isExtendedLatinString(wstr, numericOrSpace))
        {
            return true;
        }
        if (isCyrillicString(wstr, numericOrSpace))
        {
            return true;
        }
        if (isEastAsianString(wstr, numericOrSpace))
        {
            return true;
        }
        return false;
    }

    if (strictMask & 0x2)                                   // realm zone specific
    {
        LanguageType lt = GetRealmLanguageType(create);
        if (lt & LT_EXTENDEN_LATIN)
        {
            if (isExtendedLatinString(wstr, numericOrSpace))
            {
                return true;
            }
        }

        if (lt & LT_CYRILLIC)
        {
            if (isCyrillicString(wstr, numericOrSpace))
            {
                return true;
            }
        }

        if (lt & LT_EAST_ASIA)
        {
            if (isEastAsianString(wstr, numericOrSpace))
            {
                return true;
            }
        }
    }

    if (strictMask & 0x1)                                   // basic Latin
    {
        if (isBasicLatinString(wstr, numericOrSpace))
        {
            return true;
        }
    }

    return false;
}

/**
 * @brief Validates a player name against length and language rules.
 *
 * @param name The player name to validate.
 * @param create true when validating at character creation time.
 * @return The player-name validation result code.
 */
uint8 ObjectMgr::CheckPlayerName(const std::string& name, bool create)
{
    std::wstring wname;
    if (!Utf8toWStr(name, wname))
    {
        return CHAR_NAME_INVALID_CHARACTER;
    }

    if (wname.size() > MAX_PLAYER_NAME)
    {
        return CHAR_NAME_TOO_LONG;
    }

    uint32 minName = sWorld.getConfig(CONFIG_UINT32_MIN_PLAYER_NAME);
    if (wname.size() < minName)
    {
        return CHAR_NAME_TOO_SHORT;
    }

    uint32 strictMask = sWorld.getConfig(CONFIG_UINT32_STRICT_PLAYER_NAMES);
    if (!isValidString(wname, strictMask, false, create))
    {
        return CHAR_NAME_MIXED_LANGUAGES;
    }

    return CHAR_NAME_SUCCESS;
}

/**
 * @brief Validates a charter name against configured length and language rules.
 *
 * @param name The charter name to validate.
 * @return true if the charter name is valid; otherwise, false.
 */
bool ObjectMgr::IsValidCharterName(const std::string& name)
{
    std::wstring wname;
    if (!Utf8toWStr(name, wname))
    {
        return false;
    }

    if (wname.size() > MAX_CHARTER_NAME)
    {
        return false;
    }

    uint32 minName = sWorld.getConfig(CONFIG_UINT32_MIN_CHARTER_NAME);
    if (wname.size() < minName)
    {
        return false;
    }

    uint32 strictMask = sWorld.getConfig(CONFIG_UINT32_STRICT_CHARTER_NAMES);

    return isValidString(wname, strictMask, true);
}

/**
 * @brief Validates a pet name against configured length and language rules.
 *
 * @param name The pet name to validate.
 * @return The pet-name validation result code.
 */
PetNameInvalidReason ObjectMgr::CheckPetName(const std::string& name)
{
    std::wstring wname;
    if (!Utf8toWStr(name, wname))
    {
        return PET_NAME_INVALID;
    }

    if (wname.size() > MAX_PET_NAME)
    {
        return PET_NAME_TOO_LONG;
    }

    uint32 minName = sWorld.getConfig(CONFIG_UINT32_MIN_PET_NAME);
    if (wname.size() < minName)
    {
        return PET_NAME_TOO_SHORT;
    }

    uint32 strictMask = sWorld.getConfig(CONFIG_UINT32_STRICT_PET_NAMES);
    if (!isValidString(wname, strictMask, false))
    {
        return PET_NAME_MIXED_LANGUAGES;
    }

    return PET_NAME_SUCCESS;
}
