#include "TestSupport.hpp"

#include "net/Server.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifdef _WIN32

namespace
{
using namespace std::chrono_literals;

class SocketHandle
{
public:
    explicit SocketHandle(SOCKET socket = INVALID_SOCKET) : m_socket(socket) {}
    ~SocketHandle()
    {
        if (m_socket != INVALID_SOCKET)
            closesocket(m_socket);
    }

    SocketHandle(SocketHandle const&) = delete;
    SocketHandle& operator=(SocketHandle const&) = delete;

    SocketHandle(SocketHandle&& other) noexcept : m_socket(other.m_socket)
    {
        other.m_socket = INVALID_SOCKET;
    }

    SocketHandle& operator=(SocketHandle&& other) noexcept
    {
        if (this != &other)
        {
            if (m_socket != INVALID_SOCKET)
                closesocket(m_socket);
            m_socket = other.m_socket;
            other.m_socket = INVALID_SOCKET;
        }
        return *this;
    }

    SOCKET get() const { return m_socket; }

private:
    SOCKET m_socket;
};

class SessionRegistry
{
public:
    void publish(std::shared_ptr<class LoopbackSession> session)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_sessions.push_back(std::move(session));
        }
        m_ready.notify_one();
    }

    std::shared_ptr<class LoopbackSession> take()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (!m_ready.wait_for(lock, 5s, [&] { return !m_sessions.empty(); }))
            return {};

        auto session = std::move(m_sessions.front());
        m_sessions.pop_front();
        return session;
    }

private:
    std::mutex m_mutex;
    std::condition_variable m_ready;
    std::deque<std::shared_ptr<class LoopbackSession>> m_sessions;
};

class LoopbackSession final : public net::ISession
{
public:
    enum class Mode { FinalResponse, ContractClose, ExternalClose };

    LoopbackSession(Mode mode, std::vector<uint8_t> finalResponse,
                    SessionRegistry* registry = nullptr)
        : m_mode(mode), m_finalResponse(std::move(finalResponse)), m_registry(registry)
    {
    }

    void setSender(net::Sender sender) override { m_sender = std::move(sender); }
    void setCloser(net::Closer closer) override { m_closer = std::move(closer); }

    std::vector<uint8_t> onConnect() override
    {
        if (m_registry)
            m_registry->publish(std::static_pointer_cast<LoopbackSession>(shared_from_this()));
        return {};
    }

    std::vector<uint8_t> onData(uint8_t const*, std::size_t) override
    {
        if (m_mode == Mode::FinalResponse)
        {
            m_sender(m_finalResponse.data(), m_finalResponse.size());
            requestClose();
        }
        else if (m_mode == Mode::ContractClose)
        {
            m_closed.store(true, std::memory_order_release);
            return m_finalResponse;
        }
        return {};
    }

    bool closed() const override { return m_closed.load(std::memory_order_acquire); }

    void send(std::vector<uint8_t> const& bytes) { m_sender(bytes.data(), bytes.size()); }

    void requestClose()
    {
        m_closed.store(true, std::memory_order_release);
        m_closer();
    }

private:
    Mode m_mode;
    std::vector<uint8_t> m_finalResponse;
    SessionRegistry* m_registry;
    net::Sender m_sender;
    net::Closer m_closer;
    std::atomic<bool> m_closed{false};
};

uint16_t reserveLoopbackPort()
{
    SocketHandle socket(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    if (socket.get() == INVALID_SOCKET)
        return 0;

    SOCKADDR_IN address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (bind(socket.get(), reinterpret_cast<SOCKADDR*>(&address), sizeof(address)) == SOCKET_ERROR)
        return 0;

    int length = sizeof(address);
    if (getsockname(socket.get(), reinterpret_cast<SOCKADDR*>(&address), &length) == SOCKET_ERROR)
        return 0;
    return ntohs(address.sin_port);
}

bool startServer(net::Server& server, net::SessionFactory factory, uint16_t& port)
{
    for (int attempt = 0; attempt < 5; ++attempt)
    {
        port = reserveLoopbackPort();
        if (port != 0 && server.start(port, factory, "127.0.0.1"))
            return true;
    }
    return false;
}

SocketHandle connectClient(uint16_t port)
{
    SOCKET socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket == INVALID_SOCKET)
        return SocketHandle{};

    DWORD timeout = 5000;
    int receiveBuffer = 4096;
    setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<char const*>(&timeout), sizeof(timeout));
    setsockopt(socket, SOL_SOCKET, SO_RCVBUF,
               reinterpret_cast<char const*>(&receiveBuffer), sizeof(receiveBuffer));

    SOCKADDR_IN address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);
    if (connect(socket, reinterpret_cast<SOCKADDR*>(&address), sizeof(address)) == SOCKET_ERROR)
    {
        closesocket(socket);
        return SocketHandle{};
    }
    return SocketHandle(socket);
}

bool readToEof(SOCKET socket, std::vector<uint8_t>& bytes, int& socketError)
{
    std::array<uint8_t, 8192> buffer{};
    while (true)
    {
        int const received = recv(socket, reinterpret_cast<char*>(buffer.data()),
                                  static_cast<int>(buffer.size()), 0);
        if (received == 0)
            return true;
        if (received == SOCKET_ERROR)
        {
            socketError = WSAGetLastError();
            return false;
        }
        bytes.insert(bytes.end(), buffer.begin(), buffer.begin() + received);
    }
}

std::vector<uint8_t> makeFinalPayload()
{
    std::vector<uint8_t> payload(1024 * 1024);
    for (std::size_t i = 0; i < payload.size(); ++i)
        payload[i] = static_cast<uint8_t>(i % 251);
    return payload;
}

bool finalResponseDrainsBeforeEof()
{
    std::vector<uint8_t> const payload = makeFinalPayload();
    net::Server server;
    uint16_t port = 0;
    if (!startServer(server,
            [payload] {
                return std::make_shared<LoopbackSession>(
                    LoopbackSession::Mode::FinalResponse, payload);
            }, port))
        return false;

    bool passed = true;
    for (int iteration = 0; iteration < 100 && passed; ++iteration)
    {
        SocketHandle client = connectClient(port);
        if (client.get() == INVALID_SOCKET)
        {
            passed = false;
            break;
        }

        uint8_t trigger = 1;
        if (send(client.get(), reinterpret_cast<char const*>(&trigger), 1, 0) != 1)
        {
            passed = false;
            break;
        }

        std::vector<uint8_t> received;
        int socketError = 0;
        bool const reachedEof = readToEof(client.get(), received, socketError);
        passed = reachedEof && received == payload;
        if (!passed)
            std::cerr << "final-response iteration=" << iteration
                      << " eof=" << reachedEof << " socketError=" << socketError
                      << " expectedBytes=" << payload.size()
                      << " actualBytes=" << received.size() << '\n';
    }

    server.stop();
    return passed;
}

bool closeTransitionRejectsLaterSends()
{
    SessionRegistry registry;
    net::Server server;
    uint16_t port = 0;
    if (!startServer(server,
            [&registry] {
                return std::make_shared<LoopbackSession>(
                    LoopbackSession::Mode::ExternalClose, std::vector<uint8_t>{}, &registry);
            }, port))
        return false;

    std::vector<uint8_t> const pre = {'P', 'R', 'E'};
    std::vector<uint8_t> const post = {'P', 'O', 'S', 'T'};
    bool passed = true;
    for (int iteration = 0; iteration < 100 && passed; ++iteration)
    {
        SocketHandle client = connectClient(port);
        std::shared_ptr<LoopbackSession> session = registry.take();
        if (client.get() == INVALID_SOCKET || !session)
        {
            passed = false;
            break;
        }

        std::promise<void> preReturned;
        std::promise<void> closeReturned;
        std::shared_future<void> closeSignal = closeReturned.get_future().share();
        std::thread producer([&] {
            session->send(pre);
            preReturned.set_value();
            closeSignal.wait();
            session->send(post);
        });

        preReturned.get_future().wait();
        session->requestClose();
        closeReturned.set_value();
        producer.join();

        std::vector<uint8_t> received;
        int socketError = 0;
        bool const reachedEof = readToEof(client.get(), received, socketError);
        passed = reachedEof && received == pre;
        if (!passed)
        {
            std::cerr << "close-race iteration=" << iteration
                      << " eof=" << reachedEof << " socketError=" << socketError
                      << " actualBytes=" << received.size() << " data=";
            for (uint8_t byte : received)
                std::cerr << static_cast<char>(byte);
            std::cerr << '\n';
        }
    }

    server.stop();
    return passed;
}

bool closedContractDrainsBeforeEof()
{
    std::vector<uint8_t> const payload = makeFinalPayload();
    net::Server server;
    uint16_t port = 0;
    if (!startServer(server,
            [payload] {
                return std::make_shared<LoopbackSession>(
                    LoopbackSession::Mode::ContractClose, payload);
            }, port))
        return false;

    SocketHandle client = connectClient(port);
    uint8_t trigger = 1;
    bool passed = client.get() != INVALID_SOCKET &&
                  send(client.get(), reinterpret_cast<char const*>(&trigger), 1, 0) == 1;
    if (passed)
    {
        std::vector<uint8_t> received;
        int socketError = 0;
        passed = readToEof(client.get(), received, socketError) && received == payload;
    }

    server.stop();
    return passed;
}

bool restartAcceptsConnections()
{
    SessionRegistry registry;
    net::Server server;
    auto factory = [&registry] {
        return std::make_shared<LoopbackSession>(
            LoopbackSession::Mode::ExternalClose, std::vector<uint8_t>{}, &registry);
    };

    uint16_t firstPort = 0;
    if (!startServer(server, factory, firstPort))
        return false;
    SocketHandle firstClient = connectClient(firstPort);
    bool const firstAccepted = firstClient.get() != INVALID_SOCKET && registry.take() != nullptr;
    server.stop();
    if (!firstAccepted)
        return false;

    uint16_t secondPort = 0;
    if (!startServer(server, factory, secondPort))
        return false;
    SocketHandle secondClient = connectClient(secondPort);
    bool const secondAccepted = secondClient.get() != INVALID_SOCKET && registry.take() != nullptr;
    server.stop();
    return secondAccepted;
}

bool outstandingOperationAccountingIsClosedByShutdown()
{
    net::OutstandingOperations operations;
    if (!operations.tryBegin() || operations.count() != 1)
        return false;
    operations.complete();
    if (operations.count() != 0)
        return false;

    if (!operations.tryBegin())
        return false;
    auto completion = std::async(std::launch::async, [&] { operations.complete(); });
    operations.waitForZero();
    completion.get();
    if (operations.count() != 0)
        return false;

    operations.stopSubmissions();
    return !operations.acceptingSubmissions() && !operations.tryBegin() &&
           operations.count() == 0;
}

bool shutdownDrainsPendingOperations()
{
    for (int iteration = 0; iteration < 50; ++iteration)
    {
        net::Server server;
        uint16_t port = 0;
        if (!startServer(server,
                [] {
                    return std::make_shared<LoopbackSession>(
                        LoopbackSession::Mode::ExternalClose, std::vector<uint8_t>{});
                }, port))
            return false;

        std::array<SocketHandle, 4> clients;
        for (SocketHandle& client : clients)
        {
            client = connectClient(port);
            if (client.get() == INVALID_SOCKET)
                return false;
        }

        auto stopped = std::async(std::launch::async, [&] { server.stop(); });
        if (stopped.wait_for(5s) != std::future_status::ready)
        {
            std::cerr << "shutdown iteration=" << iteration << " exceeded five seconds\n";
            return false;
        }
        stopped.get();
    }
    return true;
}
}

#endif

#ifndef _WIN32

#include "net/reactor/ReactorServer.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace
{
using namespace std::chrono_literals;

class PosixSocketHandle
{
public:
    explicit PosixSocketHandle(int fd = -1) : m_fd(fd) {}
    ~PosixSocketHandle() { if (m_fd >= 0) ::close(m_fd); }
    PosixSocketHandle(PosixSocketHandle const&) = delete;
    PosixSocketHandle& operator=(PosixSocketHandle const&) = delete;
    PosixSocketHandle(PosixSocketHandle&& other) noexcept : m_fd(other.m_fd)
    {
        other.m_fd = -1;
    }
    PosixSocketHandle& operator=(PosixSocketHandle&& other) noexcept
    {
        if (this != &other)
        {
            if (m_fd >= 0)
                ::close(m_fd);
            m_fd = other.m_fd;
            other.m_fd = -1;
        }
        return *this;
    }
    int get() const { return m_fd; }

private:
    int m_fd;
};

uint16_t reservePosixLoopbackPort()
{
    PosixSocketHandle socket(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    if (socket.get() < 0)
        return 0;

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (::bind(socket.get(), reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0)
        return 0;

    socklen_t length = sizeof(address);
    if (::getsockname(socket.get(), reinterpret_cast<sockaddr*>(&address), &length) < 0)
        return 0;
    return ntohs(address.sin_port);
}

PosixSocketHandle connectPosixClient(uint16_t port)
{
    int fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0)
        return PosixSocketHandle{};

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);
    if (::connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0)
    {
        ::close(fd);
        return PosixSocketHandle{};
    }
    return PosixSocketHandle(fd);
}

class CallbackRecorderSession final : public net::ISession
{
public:
    void setPeerAddress(std::string const& address) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_events.push_back("peer:" + address);
    }

    void setSender(net::Sender sender) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sender = std::move(sender);
    }

    std::vector<uint8_t> onConnect() override
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_events.push_back("connect");
        }
        m_changed.notify_all();
        return {};
    }

    std::vector<uint8_t> onData(uint8_t const*, std::size_t) override { return {}; }

    void onClose() override
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_events.push_back("close");
            ++m_closeCount;
            m_closed.store(true, std::memory_order_release);
        }
        m_changed.notify_all();
    }

    bool closed() const override { return m_closed.load(std::memory_order_acquire); }

    bool waitForEventCount(std::size_t count)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_changed.wait_for(lock, 5s, [&] { return m_events.size() >= count; });
    }

    std::vector<std::string> events() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_events;
    }

    int closeCount() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_closeCount;
    }

    void sendOneByte()
    {
        net::Sender sender;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            sender = m_sender;
        }
        uint8_t byte = 1;
        sender(&byte, 1);
    }

private:
    mutable std::mutex m_mutex;
    std::condition_variable m_changed;
    std::vector<std::string> m_events;
    net::Sender m_sender;
    std::atomic<bool> m_closed{false};
    int m_closeCount = 0;
};

struct RejectingPollerState
{
    void signalAccept()
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            acceptReady = true;
            ++epoch;
        }
        changed.notify_all();
    }

    std::mutex mutex;
    std::condition_variable changed;
    uint64_t epoch = 0;
    bool acceptReady = false;
    std::atomic<unsigned> created{0};
    std::atomic<unsigned> workerWakeCalls{0};
};

class RejectingPoller final : public net::Poller
{
public:
    RejectingPoller(std::shared_ptr<RejectingPollerState> state, bool acceptPoller)
        : m_state(std::move(state)), m_acceptPoller(acceptPoller) {}

    bool init() override { return true; }

    bool add(int, uint32_t, void* udata) override
    {
        if (!m_acceptPoller)
            return false;
        m_acceptUdata = udata;
        return true;
    }

    bool mod(int, uint32_t, void*) override { return true; }
    bool del(int) override { return true; }

    int wait(net::PollerEvent* out, int maxEvents) override
    {
        std::unique_lock<std::mutex> lock(m_state->mutex);
        m_state->changed.wait(lock, [&] {
            return m_state->epoch != m_seenEpoch ||
                   (m_acceptPoller && m_state->acceptReady);
        });
        m_seenEpoch = m_state->epoch;
        if (m_acceptPoller && m_state->acceptReady && maxEvents > 0)
        {
            m_state->acceptReady = false;
            out[0] = {m_acceptUdata, net::EvRead, false};
            return 1;
        }
        return 0;
    }

    void wake() override
    {
        if (!m_acceptPoller)
            m_state->workerWakeCalls.fetch_add(1, std::memory_order_relaxed);
        {
            std::lock_guard<std::mutex> lock(m_state->mutex);
            ++m_state->epoch;
        }
        m_state->changed.notify_all();
    }

    void shutdown() override { wake(); }
    char const* name() const override { return "rejecting-test-poller"; }

private:
    std::shared_ptr<RejectingPollerState> m_state;
    bool m_acceptPoller;
    void* m_acceptUdata = nullptr;
    uint64_t m_seenEpoch = 0;
};

bool reactorRejectedRegistrationRunsTeardown()
{
    auto state = std::make_shared<RejectingPollerState>();
    net::ReactorServer server([state] {
        bool const acceptPoller = state->created.fetch_add(1) == 0;
        return std::make_unique<RejectingPoller>(state, acceptPoller);
    });
    auto session = std::make_shared<CallbackRecorderSession>();

    uint16_t port = reservePosixLoopbackPort();
    if (port == 0 || !server.start(port, [session] { return session; }, "127.0.0.1"))
        return false;

    PosixSocketHandle client = connectPosixClient(port);
    if (client.get() < 0)
        return false;
    state->signalAccept();

    bool passed = session->waitForEventCount(3) &&
                  session->events() == std::vector<std::string>{"peer:127.0.0.1", "connect", "close"} &&
                  session->closeCount() == 1;
    if (passed)
    {
        unsigned const wakesBeforeSend = state->workerWakeCalls.load(std::memory_order_relaxed);
        session->sendOneByte();
        std::this_thread::sleep_for(20ms);
        passed = state->workerWakeCalls.load(std::memory_order_relaxed) == wakesBeforeSend;
    }

    server.stop();
    return passed && session->closeCount() == 1;
}

#ifdef MANGOS_USE_IO_URING
bool uringPublishesPeerBeforeConnect()
{
    net::Server server;
    auto session = std::make_shared<CallbackRecorderSession>();
    uint16_t port = reservePosixLoopbackPort();
    if (port == 0 || !server.start(port, [session] { return session; }, "127.0.0.1"))
        return false;

    PosixSocketHandle client = connectPosixClient(port);
    bool const connected = client.get() >= 0 && session->waitForEventCount(2);
    std::vector<std::string> const events = session->events();
    server.stop();
    return connected && events.size() >= 2 &&
           events[0] == "peer:127.0.0.1" && events[1] == "connect";
}
#endif
}

#endif

int main()
{
#ifdef _WIN32
    WSADATA wsa{};
    CHECK(WSAStartup(MAKEWORD(2, 2), &wsa) == 0);
    if (mangos::test::failures == 0)
    {
        CHECK(outstandingOperationAccountingIsClosedByShutdown());
        CHECK(finalResponseDrainsBeforeEof());
        CHECK(closeTransitionRejectsLaterSends());
        CHECK(closedContractDrainsBeforeEof());
        CHECK(restartAcceptsConnections());
        CHECK(shutdownDrainsPendingOperations());
    }
    WSACleanup();
#else
    CHECK(reactorRejectedRegistrationRunsTeardown());
#ifdef MANGOS_USE_IO_URING
    CHECK(uringPublishesPeerBeforeConnect());
#else
    std::cout << "SKIP: io_uring peer-order regression requires MANGOS_USE_IO_URING\n";
#endif
#endif
    return mangos::test::failures == 0 ? 0 : 1;
}
