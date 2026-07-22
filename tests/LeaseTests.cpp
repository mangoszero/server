#include "TestSupport.hpp"
#include "Threading/LeasedPtr.h"

#include <chrono>
#include <future>
#include <type_traits>

namespace
{
using namespace std::chrono_literals;

struct Probe
{
    int value = 7;
};

using ProbePtr = LeasedPtr<Probe>;
using ProbeLease = ProbePtr::Lease;

static_assert(!std::is_copy_constructible_v<ProbeLease>);
static_assert(!std::is_copy_assignable_v<ProbeLease>);
static_assert(std::is_move_constructible_v<ProbeLease>);
static_assert(std::is_move_assignable_v<ProbeLease>);

void detachAndWaitBlocksForActiveLease()
{
    Probe probe;
    ProbePtr pointer;
    pointer.publish(&probe);
    ProbeLease lease = pointer.acquire();
    CHECK(lease && lease.get() == &probe && lease->value == 7);

    auto detached = std::async(std::launch::async, [&] { pointer.detachAndWait(); });
    while (pointer.acquire())
    {
        // Once acquire fails, detachAndWait has unpublished the pointer and is
        // deterministically waiting for the original lease below.
    }
    CHECK(detached.wait_for(0s) == std::future_status::timeout);

    lease = {};
    CHECK(detached.wait_for(5s) == std::future_status::ready);
    detached.get();
    CHECK(!pointer.acquire());
}

void movingTransfersOneActiveUse()
{
    Probe probe;
    ProbePtr pointer;
    pointer.publish(&probe);

    ProbeLease original = pointer.acquire();
    ProbeLease moved = std::move(original);
    CHECK(!original && moved && moved.get() == &probe);

    pointer.detach();
    auto drained = std::async(std::launch::async, [&] { pointer.waitForNoLeases(); });
    CHECK(drained.wait_for(0s) == std::future_status::timeout);

    ProbeLease assigned;
    assigned = std::move(moved);
    CHECK(!moved && assigned);
    assigned = {};
    CHECK(drained.wait_for(5s) == std::future_status::ready);
    drained.get();
}

void detachDoesNotWaitOnItsCallingLease()
{
    Probe probe;
    ProbePtr pointer;
    pointer.publish(&probe);
    ProbeLease lease = pointer.acquire();

    auto closeCapableCallback = [&] { pointer.detach(); };
    closeCapableCallback();
    CHECK(lease && !pointer.acquire());

    lease = {};
    auto drained = std::async(std::launch::async, [&] { pointer.waitForNoLeases(); });
    CHECK(drained.wait_for(5s) == std::future_status::ready);
    drained.get();
}
}

int main()
{
    detachAndWaitBlocksForActiveLease();
    movingTransfersOneActiveUse();
    detachDoesNotWaitOnItsCallingLease();
    return mangos::test::failures == 0 ? 0 : 1;
}
