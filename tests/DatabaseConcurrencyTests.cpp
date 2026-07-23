#include "TestSupport.hpp"

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

    bool Initialize(char const*) override { return true; }

    QueryResult* Query(char const*) override
    {
        Enter();
        queryEntered.store(true);
        if (coordinateEscape)
        {
            while (!escapeAttempting.load())
                std::this_thread::yield();
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        Leave();
        return nullptr;
    }

    QueryNamedResult* QueryNamed(char const*) override { return nullptr; }
    bool Execute(char const*) override { return true; }

    unsigned long escape_string(
        char* to, char const* from, unsigned long length) override
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
    FakeConnection* m_connection;
};

void concurrentQueriesUseTheConnectionLock()
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

void escapingSharesTheQueryConnectionLock()
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
}

int main()
{
    concurrentQueriesUseTheConnectionLock();
    escapingSharesTheQueryConnectionLock();
    return mangos::test::failures == 0 ? 0 : 1;
}
