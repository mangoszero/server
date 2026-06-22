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

/**
 * @file ServiceDatabase.h
 * @brief The child's OWN read-only world-DB connection.
 *
 * The ah-service child opens its own @c DatabaseType (mangosd's
 * @c WorldDatabase global is untouched). This connection is strictly
 * READ-ONLY: the child must never issue a write/INSERT/UPDATE/DELETE. The
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

    private:
        DatabaseType m_worldDatabase; ///< Child-owned read-only world DB
        bool m_initialized;           ///< True once Init() succeeded

        // Non-copyable: owns a live DB connection pool.
        ServiceDatabase(const ServiceDatabase&);
        ServiceDatabase& operator=(const ServiceDatabase&);
};

#endif // AH_SERVICE_SERVICE_DATABASE_H
