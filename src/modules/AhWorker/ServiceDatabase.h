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
 */

#ifndef AH_SERVICE_SERVICE_DATABASE_H
#define AH_SERVICE_SERVICE_DATABASE_H

#include "Common.h"
#include "Database/DatabaseEnv.h"

#include <string>

/**
 * @file ServiceDatabase.h
 * @brief The child's OWN read-only world-DB connection.
 *
 * The ah-service child opens its own @c DatabaseType (mangosd's
 * @c WorldDatabase global is untouched). This connection is READ-ONLY BY
 * USAGE: the child only ever issues SELECTs and never INSERT/UPDATE/DELETE.
 * That is a code discipline, NOT an enforced credential restriction -- the
 * connection string is whatever @c WorldDatabaseInfo supplies. For real
 * enforcement (so a compromised child cannot write the DB directly and bypass
 * the mangosd executor authority) provision a dedicated SELECT-only MySQL
 * account for this connection; see doc/AuctionHouseBot.md "Security". The
 * connection string and pool size come from @c sConfig (the loaded
 * @c ah-service.conf) via the @c WorldDatabaseInfo /
 * @c WorldDatabaseConnections keys, the same format as mangosd.conf.
 *
 * Unlike mangosd's @c start_db this deliberately skips the strict
 * @c CheckDatabaseVersion: a read-only advisory child should be
 * version-tolerant. A single trivial connectivity query stands in instead.
 */
class ServiceDatabase
{
    public:
        ServiceDatabase();
        ~ServiceDatabase();

        /**
         * @brief Open the read-only world-DB connection.
         *
         * Reads @c WorldDatabaseInfo / @c WorldDatabaseConnections from
         * @c sConfig, initializes the connection pool, and runs one trivial
         * connectivity query.
         *
         * @return true on success, false on any failure (missing config,
         *         connect failure, or null connectivity result).
         */
        bool Init();

        /**
         * @brief Halt the DB delay thread and release the connection.
         *
         * Safe to call even if @c Init() was never called or failed.
         */
        void Shutdown();

        /**
         * @brief Access the underlying read-only world database.
         *
         * Callers must only issue SELECTs.
         */
        DatabaseType& World()
        {
            return m_worldDatabase;
        }

        /**
         * @brief The world database name (the 5th semicolon-delimited
         *        field of WorldDatabaseInfo). Empty until Init() succeeds.
         */
        const std::string& WorldDbName() const { return m_worldDbName; }

        /**
         * @brief The character database name (the 5th semicolon-delimited
         *        field of CharacterDatabaseInfo). Empty until
         *        InitCharacter() succeeds.
         */
        const std::string& CharDbName() const { return m_charDbName; }

        /**
         * @brief Open the read-only character-DB connection.
         *
         * Reads @c CharacterDatabaseInfo / @c CharacterDatabaseConnections
         * from @c sConfig, initializes the connection pool, and runs one
         * trivial connectivity query.  READ-ONLY -- never write.
         *
         * @return true on success, false on any failure.
         */
        bool InitCharacter();

        /**
         * @brief Access the underlying read-only character database.
         *
         * Callers must only issue SELECTs.
         */
        DatabaseType& Character()
        {
            return m_characterDatabase;
        }

        /**
         * @brief Resolve a character's low GUID from its name.
         *
         * Faithful reimplementation of
         * @c ObjectMgr::GetPlayerGuidByName(): the name is escaped with the
         * character DB's @c escape_string and looked up with
         * @c "SELECT guid FROM characters WHERE name = '<escaped>'". The
         * child cannot reach @c sObjectMgr, so this stands in for the bot's
         * GUID resolution. READ-ONLY.
         *
         * @param name Character name (verbatim from config; may be empty).
         * @return The character's low GUID, or 0 if the name is empty,
         *         not found, or the character DB is not initialized.
         */
        uint32 ResolveCharacterGuid(const std::string& name);

    private:
        /// Parse the last (5th) semicolon-delimited field from a dbstring.
        static std::string ParseDbName(const std::string& dbstring);

        DatabaseType m_worldDatabase;     ///< Child-owned read-only world DB
        bool         m_initialized;       ///< True once Init() succeeded
        std::string  m_worldDbName;       ///< World DB name from dbstring
        DatabaseType m_characterDatabase; ///< Child-owned read-only character DB
        bool         m_charInitialized;   ///< True once InitCharacter() succeeded
        std::string  m_charDbName;        ///< Character DB name from dbstring

        // Non-copyable: owns a live DB connection pool.
        ServiceDatabase(const ServiceDatabase&);
        ServiceDatabase& operator=(const ServiceDatabase&);
};

#endif // AH_SERVICE_SERVICE_DATABASE_H
