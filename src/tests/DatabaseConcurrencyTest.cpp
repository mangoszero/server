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

#include "TestHarness.h"
#include "Database/QueryResult.h"
#include "Database/Database.h"

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

namespace
{
class FakeConnection final : public SqlConnection
{
    public:
        explicit FakeConnection(Database& database)
            : SqlConnection(database)
        {
        }

        bool Initialize(const char*) override { return true; }

        QueryResult* Query(const char*) override
        {
            Enter();
            queryEntered.store(true);
            if (coordinateEscape)
            {
                while (!escapeAttempting.load())
                    std::this_thread::yield();
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(20));
            }
            else
            {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(1));
            }
            Leave();
            return nullptr;
        }

        QueryNamedResult* QueryNamed(const char*) override
        {
            return nullptr;
        }

        bool Execute(const char*) override { return true; }

        unsigned long escape_string(
            char* to, const char* from,
            unsigned long length) override
        {
            Enter();
            for (unsigned long i = 0; i < length; ++i)
                to[i] = from[i];
            to[length] = '\0';
            Leave();
            return length;
        }

        void Enter()
        {
            if (active.fetch_add(1) != 0)
                overlap.store(true);
        }

        void Leave()
        {
            active.fetch_sub(1);
        }

        std::atomic<int> active{0};
        std::atomic<bool> overlap{false};
        std::atomic<bool> queryEntered{false};
        std::atomic<bool> escapeAttempting{false};
        bool coordinateEscape = false;
};

class FakeDatabase final : public Database
{
    public:
        FakeDatabase()
        {
            m_connection = new FakeConnection(*this);
            m_pQueryConnections.push_back(m_connection);
            m_nQueryConnPoolSize = 1;
        }

        FakeConnection& Connection() { return *m_connection; }

    protected:
        SqlConnection* CreateConnection() override
        {
            return new FakeConnection(*this);
        }

    private:
        FakeConnection* m_connection = nullptr;
};
}

TEST(Database_queries_serialize_on_connection_lock)
{
    FakeDatabase database;
    std::vector<std::thread> threads;
    for (unsigned i = 0; i < 8; ++i)
    {
        threads.emplace_back([&database]()
        {
            for (unsigned query = 0; query < 3; ++query)
                database.PQuery("SELECT %u", query);
        });
    }
    for (std::thread& thread : threads)
        thread.join();

    CHECK(!database.Connection().overlap.load());
}

TEST(Database_escape_shares_connection_zero_lock)
{
    FakeDatabase database;
    FakeConnection& connection = database.Connection();
    connection.coordinateEscape = true;

    std::thread query([&database]() { database.PQuery("SELECT 1"); });
    std::thread escape([&database, &connection]()
    {
        while (!connection.queryEntered.load())
            std::this_thread::yield();
        connection.escapeAttempting.store(true);
        std::string value = "account'name";
        database.escape_string(value);
    });

    query.join();
    escape.join();
    CHECK(!connection.overlap.load());
}
