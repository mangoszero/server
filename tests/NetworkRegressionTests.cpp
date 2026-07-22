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
    enum class Mode { FinalResponse, ExternalClose };

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
}

#endif

int main()
{
#ifdef _WIN32
    WSADATA wsa{};
    CHECK(WSAStartup(MAKEWORD(2, 2), &wsa) == 0);
    if (mangos::test::failures == 0)
    {
        CHECK(finalResponseDrainsBeforeEof());
        CHECK(closeTransitionRejectsLaterSends());
    }
    WSACleanup();
#else
    std::cout << "SKIP: IOCP close regressions require Windows\n";
#endif
    return mangos::test::failures == 0 ? 0 : 1;
}
