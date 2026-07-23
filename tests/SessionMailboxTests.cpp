#include "TestSupport.hpp"

#include "SessionMailbox.h"

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

namespace
{
std::unique_ptr<WorldPacket> Packet(uint16 opcode, uint8 value)
{
    auto packet = std::make_unique<WorldPacket>(opcode, 1);
    *packet << value;
    return packet;
}

void mailboxTransfersPacketsInFifoOrder()
{
    SessionMailbox mailbox;
    CHECK(mailbox.Enqueue(Packet(1, 0x11)));
    CHECK(mailbox.Enqueue(Packet(2, 0x22)));

    WorldPacket* raw = nullptr;
    CHECK(mailbox.Next(raw));
    std::unique_ptr<WorldPacket> first(raw);
    CHECK(first->GetOpcode() == 1);
    CHECK((*first)[0] == 0x11);

    CHECK(mailbox.Next(raw));
    std::unique_ptr<WorldPacket> second(raw);
    CHECK(second->GetOpcode() == 2);
    CHECK((*second)[0] == 0x22);
    CHECK(!mailbox.Next(raw));
}

void closedMailboxRejectsNewOwnership()
{
    SessionMailbox mailbox;
    mailbox.Close();

    CHECK(mailbox.IsClosed());
    CHECK(!mailbox.Enqueue(Packet(3, 0x33)));
    WorldPacket* raw = nullptr;
    CHECK(!mailbox.Next(raw));
}

void closeRacingProducersLeavesNoPostClosePackets()
{
    SessionMailbox mailbox;
    std::atomic<bool> start{false};
    std::vector<std::thread> producers;
    for (unsigned producer = 0; producer < 4; ++producer)
    {
        producers.emplace_back([&mailbox, &start, producer]()
        {
            while (!start.load())
                std::this_thread::yield();
            for (unsigned packet = 0; packet < 200; ++packet)
                mailbox.Enqueue(Packet(uint16(producer + 1), uint8(packet)));
        });
    }

    start.store(true);
    mailbox.Close();
    for (std::thread& producer : producers)
        producer.join();

    WorldPacket* raw = nullptr;
    CHECK(!mailbox.Next(raw));
    CHECK(!mailbox.Enqueue(Packet(9, 0x99)));
}

void detachedRegistryRouteCannotReachItsReplacement()
{
    auto oldMailbox = std::make_shared<SessionMailbox>();
    std::shared_ptr<SessionMailbox> retainedDelivery = oldMailbox;
    oldMailbox->Close();

    auto replacement = std::make_shared<SessionMailbox>();
    CHECK(!retainedDelivery->Enqueue(Packet(4, 0x44)));
    CHECK(replacement->Enqueue(Packet(5, 0x55)));

    WorldPacket* raw = nullptr;
    CHECK(replacement->Next(raw));
    std::unique_ptr<WorldPacket> packet(raw);
    CHECK(packet->GetOpcode() == 5);
    CHECK(!replacement->Next(raw));
}

void closeDrainsResidualPackets()
{
    SessionMailbox mailbox;
    CHECK(mailbox.Enqueue(Packet(7, 0x77)));
    CHECK(mailbox.Enqueue(Packet(8, 0x88)));
    mailbox.Close();

    WorldPacket* raw = nullptr;
    CHECK(mailbox.IsClosed());
    CHECK(!mailbox.Next(raw));
    CHECK(!mailbox.Enqueue(Packet(9, 0x99)));
}
}

int main()
{
    mailboxTransfersPacketsInFifoOrder();
    closedMailboxRejectsNewOwnership();
    closeRacingProducersLeavesNoPostClosePackets();
    detachedRegistryRouteCannotReachItsReplacement();
    closeDrainsResidualPackets();
    return mangos::test::failures == 0 ? 0 : 1;
}
