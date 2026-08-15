/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
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
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "TestHarness.h"

#include "WardenEnforcementPolicy.h"
#include "WardenServer.h"

#include <initializer_list>
#include <vector>

namespace
{
warden::WardenEvidenceBatch Batch(warden::CheckPlanPurpose purpose,
    std::initializer_list<warden::WardenEvidence> evidence)
{
    warden::WardenEvidenceBatch batch;
    batch.requestId = 7;
    batch.purpose = purpose;
    batch.evidence.assign(evidence.begin(), evidence.end());
    return batch;
}

warden::MemEvidence Mem(uint32 checkId, warden::MemOutcome outcome)
{
    return {7, checkId, outcome};
}

std::vector<warden::WardenPolicyDecision> Confirm(
    warden::WardenEnforcementMode mode,
    warden::MemOutcome first, warden::MemOutcome second)
{
    warden::WardenEnforcementPolicy policy(mode);
    std::vector<warden::WardenPolicyDecision> const firstDecisions =
        policy.EvaluateBatch(Batch(warden::CheckPlanPurpose::Recurring,
            {Mem(1566, first)}));
    if (firstDecisions.size() != 1u)
        return {};
    return policy.EvaluateBatch(Batch(
        warden::CheckPlanPurpose::Confirmation,
        {Mem(1566, second)}));
}
}

TEST(WardenEnforcementPolicy_clean_memory_evidence_needs_no_action)
{
    warden::WardenEnforcementPolicy policy(
        warden::WardenEnforcementMode::KickAndBan);
    auto const decisions = policy.EvaluateBatch(Batch(
        warden::CheckPlanPurpose::Initial,
        {Mem(1107, warden::MemOutcome::Match)}));

    CHECK(decisions.empty());
}

TEST(WardenEnforcementPolicy_first_negative_only_queues_confirmation)
{
    for (warden::MemOutcome outcome :
        {warden::MemOutcome::ByteMismatch,
            warden::MemOutcome::Unavailable})
    {
        warden::WardenEnforcementPolicy policy(
            warden::WardenEnforcementMode::KickAndBan);
        auto const decisions = policy.EvaluateBatch(Batch(
            warden::CheckPlanPurpose::Initial, {Mem(1566, outcome)}));

        REQUIRE(decisions.size() == 1u);
        CHECK(decisions[0].action ==
            warden::WardenPolicyAction::QueueConfirmation);
        CHECK_EQ(decisions[0].checkId, uint32(1566));
        CHECK(decisions[0].outcome == outcome);
    }
}

TEST(WardenEnforcementPolicy_matching_confirmation_clears_the_candidate)
{
    warden::WardenEnforcementPolicy policy(
        warden::WardenEnforcementMode::KickAndBan);
    REQUIRE(policy.EvaluateBatch(Batch(warden::CheckPlanPurpose::Initial,
        {Mem(1566, warden::MemOutcome::ByteMismatch)})).size() == 1u);

    auto const cleared = policy.EvaluateBatch(Batch(
        warden::CheckPlanPurpose::Confirmation,
        {Mem(1566, warden::MemOutcome::Match)}));
    REQUIRE(cleared.size() == 1u);
    CHECK(cleared[0].action ==
        warden::WardenPolicyAction::ConfirmationCleared);
    CHECK_EQ(cleared[0].checkId, uint32(1566));

    auto const repeated = policy.EvaluateBatch(Batch(
        warden::CheckPlanPurpose::Recurring,
        {Mem(1566, warden::MemOutcome::Unavailable)}));
    REQUIRE(repeated.size() == 1u);
    CHECK(repeated[0].action ==
        warden::WardenPolicyAction::QueueConfirmation);
}

TEST(WardenEnforcementPolicy_either_cross_outcome_second_negative_confirms)
{
    auto const mismatchThenUnavailable = Confirm(
        warden::WardenEnforcementMode::KickAndBan,
        warden::MemOutcome::ByteMismatch,
        warden::MemOutcome::Unavailable);
    REQUIRE(mismatchThenUnavailable.size() == 1u);
    CHECK(mismatchThenUnavailable[0].action ==
        warden::WardenPolicyAction::PersistAndKick);
    CHECK(mismatchThenUnavailable[0].outcome ==
        warden::MemOutcome::Unavailable);

    auto const unavailableThenMismatch = Confirm(
        warden::WardenEnforcementMode::KickAndBan,
        warden::MemOutcome::Unavailable,
        warden::MemOutcome::ByteMismatch);
    REQUIRE(unavailableThenMismatch.size() == 1u);
    CHECK(unavailableThenMismatch[0].action ==
        warden::WardenPolicyAction::PersistAndKick);
    CHECK(unavailableThenMismatch[0].outcome ==
        warden::MemOutcome::ByteMismatch);
}

TEST(WardenEnforcementPolicy_observe_mode_confirms_without_persistence_or_kick)
{
    auto const decisions = Confirm(warden::WardenEnforcementMode::Observe,
        warden::MemOutcome::ByteMismatch,
        warden::MemOutcome::ByteMismatch);

    REQUIRE(decisions.size() == 1u);
    CHECK(decisions[0].action ==
        warden::WardenPolicyAction::ConfirmedObservation);
    CHECK_EQ(decisions[0].checkId, uint32(1566));
}

TEST(WardenEnforcementPolicy_both_enforcing_modes_persist_and_kick)
{
    for (warden::WardenEnforcementMode mode :
        {warden::WardenEnforcementMode::Kick,
            warden::WardenEnforcementMode::KickAndBan})
    {
        auto const decisions = Confirm(mode,
            warden::MemOutcome::Unavailable,
            warden::MemOutcome::Unavailable);
        REQUIRE(decisions.size() == 1u);
        CHECK(decisions[0].action ==
            warden::WardenPolicyAction::PersistAndKick);
        CHECK_EQ(decisions[0].checkId, uint32(1566));
    }
}

TEST(WardenEnforcementPolicy_non_memory_anomalies_remain_observation_only)
{
    warden::WardenEnforcementPolicy policy(
        warden::WardenEnforcementMode::KickAndBan);
    auto const decisions = policy.EvaluateBatch(Batch(
        warden::CheckPlanPurpose::Initial,
        {
            warden::TimingEvidence{7, warden::TimingOutcome::Unstable, 1},
            warden::MpqEvidence{7, 1, warden::MpqOutcome::DigestMismatch},
            warden::LuaEvidence{7, 2, warden::LuaOutcome::TextMismatch}
        }));

    CHECK(decisions.empty());
}

TEST(WardenEnforcementPolicy_multiple_negatives_are_ordered_and_deduplicated)
{
    warden::WardenEnforcementPolicy policy(
        warden::WardenEnforcementMode::KickAndBan);
    auto const decisions = policy.EvaluateBatch(Batch(
        warden::CheckPlanPurpose::Recurring,
        {
            Mem(1107, warden::MemOutcome::ByteMismatch),
            Mem(1566, warden::MemOutcome::Unavailable),
            Mem(1107, warden::MemOutcome::Unavailable),
            Mem(827, warden::MemOutcome::Match)
        }));

    REQUIRE(decisions.size() == 2u);
    CHECK(decisions[0].action ==
        warden::WardenPolicyAction::QueueConfirmation);
    CHECK_EQ(decisions[0].checkId, uint32(1107));
    CHECK(decisions[1].action ==
        warden::WardenPolicyAction::QueueConfirmation);
    CHECK_EQ(decisions[1].checkId, uint32(1566));
}

TEST(WardenEnforcementPolicy_malformed_confirmation_never_becomes_an_incident)
{
    for (warden::WardenEnforcementMode mode :
        {warden::WardenEnforcementMode::Observe,
            warden::WardenEnforcementMode::Kick,
            warden::WardenEnforcementMode::KickAndBan})
    {
        warden::WardenEnforcementPolicy policy(mode);
        auto const decisions = policy.EvaluateBatch(Batch(
            warden::CheckPlanPurpose::Confirmation,
            {Mem(9999, warden::MemOutcome::ByteMismatch)}));

        REQUIRE(decisions.size() == 1u);
        CHECK(decisions[0].action ==
            (mode == warden::WardenEnforcementMode::Observe ?
                warden::WardenPolicyAction::None :
                warden::WardenPolicyAction::Kick));
        CHECK(decisions[0].action !=
            warden::WardenPolicyAction::PersistAndKick);
    }
}

TEST(WardenEnforcementPolicy_lifecycle_failure_kicks_only_enforcing_modes)
{
    warden::WardenLifecycleEvent const failed
    {
        warden::WardenState::Failed,
        warden::WardenFailure::DeadlineExpired,
        0
    };

    warden::WardenEnforcementPolicy observe(
        warden::WardenEnforcementMode::Observe);
    CHECK(observe.EvaluateLifecycle(failed).action ==
        warden::WardenPolicyAction::None);

    warden::WardenEnforcementPolicy kick(
        warden::WardenEnforcementMode::Kick);
    CHECK(kick.EvaluateLifecycle(failed).action ==
        warden::WardenPolicyAction::Kick);

    warden::WardenEnforcementPolicy ban(
        warden::WardenEnforcementMode::KickAndBan);
    CHECK(ban.EvaluateLifecycle(failed).action ==
        warden::WardenPolicyAction::Kick);

    warden::WardenLifecycleEvent const ready
    {
        warden::WardenState::ModuleReady,
        warden::WardenFailure::None,
        0
    };
    CHECK(ban.EvaluateLifecycle(ready).action ==
        warden::WardenPolicyAction::None);
}
