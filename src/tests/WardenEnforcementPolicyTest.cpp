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
#include <utility>
#include <vector>

namespace
{
warden::WardenEvidence Evidence(uint32 checkId, warden::WardenCheckType type,
    warden::WardenEvidenceClass evidenceClass,
    warden::WardenCheckOutcome outcome)
{
    return {7, checkId, type, evidenceClass, outcome, 0};
}

warden::WardenEvidenceBatch Batch(warden::CheckPlanPurpose purpose,
    std::initializer_list<warden::WardenEvidence> evidence)
{
    warden::WardenEvidenceBatch batch;
    batch.requestId = 7;
    batch.purpose = purpose;
    batch.evidence.assign(evidence.begin(), evidence.end());
    return batch;
}

std::vector<warden::WardenPolicyDecision> Confirm(
    warden::WardenEnforcementMode mode, warden::WardenEvidence first,
    warden::WardenEvidence second)
{
    warden::WardenEnforcementPolicy policy(mode);
    std::vector<warden::WardenPolicyDecision> const queued =
        policy.EvaluateBatch(Batch(warden::CheckPlanPurpose::Recurring,
            {first}));
    if (queued.size() != 1u || queued[0].action !=
            warden::WardenPolicyAction::QueueConfirmation)
        return {};
    return policy.EvaluateBatch(Batch(
        warden::CheckPlanPurpose::Confirmation, {second}));
}
}

TEST(WardenEvidence_shared_confirmation_and_disposition_predicates)
{
    using warden::WardenCheckOutcome;
    using warden::WardenCheckType;
    using warden::WardenConfirmedDisposition;
    using warden::WardenEnforcementMode;
    using warden::WardenEvidenceClass;

    warden::WardenEvidence integrity = Evidence(1, WardenCheckType::Mpq,
        WardenEvidenceClass::IntegrityInvariant,
        WardenCheckOutcome::Mismatch);
    CHECK(warden::NeedsConfirmation(integrity));
    CHECK(warden::ClassifyConfirmedEvidence(WardenEnforcementMode::Observe,
        integrity) == WardenConfirmedDisposition::Audit);
    CHECK(warden::ClassifyConfirmedEvidence(WardenEnforcementMode::Kick,
        integrity) == WardenConfirmedDisposition::Incident);

    integrity.outcome = WardenCheckOutcome::Unavailable;
    CHECK(warden::NeedsConfirmation(integrity));
    CHECK(warden::ClassifyConfirmedEvidence(
        WardenEnforcementMode::KickAndBan, integrity) ==
        WardenConfirmedDisposition::Audit);
    integrity.outcome = WardenCheckOutcome::Match;
    CHECK(!warden::NeedsConfirmation(integrity));
    CHECK(warden::ClassifyConfirmedEvidence(WardenEnforcementMode::Kick,
        integrity) == WardenConfirmedDisposition::Cleared);

    warden::WardenEvidence corroboration = Evidence(2,
        WardenCheckType::Lua, WardenEvidenceClass::Corroboration,
        WardenCheckOutcome::Mismatch);
    CHECK(warden::ClassifyConfirmedEvidence(WardenEnforcementMode::Kick,
        corroboration) == WardenConfirmedDisposition::Audit);

    warden::WardenEvidence timing = Evidence(65536,
        WardenCheckType::Timing, WardenEvidenceClass::ProtocolHealth,
        WardenCheckOutcome::Stable);
    CHECK(!warden::NeedsConfirmation(timing));
    CHECK(warden::ClassifyConfirmedEvidence(WardenEnforcementMode::Kick,
        timing) == WardenConfirmedDisposition::Invalid);

    warden::WardenEvidence illegal = Evidence(2, WardenCheckType::Lua,
        WardenEvidenceClass::ThreatSignature, WardenCheckOutcome::Mismatch);
    CHECK(!warden::NeedsConfirmation(illegal));
    CHECK(warden::ClassifyConfirmedEvidence(WardenEnforcementMode::Kick,
        illegal) == WardenConfirmedDisposition::Invalid);
}

TEST(WardenEnforcementPolicy_first_anomaly_only_queues_confirmation)
{
    warden::WardenEnforcementPolicy policy(
        warden::WardenEnforcementMode::KickAndBan);
    std::vector<warden::WardenPolicyDecision> const decisions =
        policy.EvaluateBatch(Batch(warden::CheckPlanPurpose::Initial,
        {
            Evidence(1, warden::WardenCheckType::Mpq,
                warden::WardenEvidenceClass::IntegrityInvariant,
                warden::WardenCheckOutcome::Mismatch),
            Evidence(2, warden::WardenCheckType::Lua,
                warden::WardenEvidenceClass::Corroboration,
                warden::WardenCheckOutcome::Unavailable),
            Evidence(1566, warden::WardenCheckType::Mem,
                warden::WardenEvidenceClass::ThreatSignature,
                warden::WardenCheckOutcome::Mismatch),
            Evidence(1566, warden::WardenCheckType::Mem,
                warden::WardenEvidenceClass::ThreatSignature,
                warden::WardenCheckOutcome::Unavailable),
            Evidence(65536, warden::WardenCheckType::Timing,
                warden::WardenEvidenceClass::ProtocolHealth,
                warden::WardenCheckOutcome::Unstable)
        }));

    REQUIRE(decisions.size() == 3u);
    CHECK_EQ(decisions[0].checkId, uint32(1));
    CHECK_EQ(decisions[1].checkId, uint32(2));
    CHECK_EQ(decisions[2].checkId, uint32(1566));
    for (warden::WardenPolicyDecision const& decision : decisions)
        CHECK(decision.action ==
            warden::WardenPolicyAction::QueueConfirmation);
}

TEST(WardenEnforcementPolicy_matching_confirmation_clears_every_check_type)
{
    struct Case
    {
        uint32 id;
        warden::WardenCheckType type;
        warden::WardenEvidenceClass evidenceClass;
    };
    Case const cases[] =
    {
        {1, warden::WardenCheckType::Mpq,
            warden::WardenEvidenceClass::IntegrityInvariant},
        {2, warden::WardenCheckType::Lua,
            warden::WardenEvidenceClass::Corroboration},
        {1566, warden::WardenCheckType::Mem,
            warden::WardenEvidenceClass::ThreatSignature}
    };

    for (Case const& test : cases)
    {
        std::vector<warden::WardenPolicyDecision> const decisions = Confirm(
            warden::WardenEnforcementMode::KickAndBan,
            Evidence(test.id, test.type, test.evidenceClass,
                warden::WardenCheckOutcome::Mismatch),
            Evidence(test.id, test.type, test.evidenceClass,
                warden::WardenCheckOutcome::Match));
        REQUIRE(decisions.size() == 1u);
        CHECK(decisions[0].action ==
            warden::WardenPolicyAction::ConfirmationCleared);
        CHECK_EQ(decisions[0].checkId, test.id);
    }
}

TEST(WardenEnforcementPolicy_routes_confirmed_actionable_mismatch_by_mode)
{
    for (warden::WardenEnforcementMode mode :
        {warden::WardenEnforcementMode::Observe,
            warden::WardenEnforcementMode::Kick,
            warden::WardenEnforcementMode::KickAndBan})
    {
        for (auto const& identity :
            {std::pair<warden::WardenCheckType,
                warden::WardenEvidenceClass>{warden::WardenCheckType::Mpq,
                    warden::WardenEvidenceClass::IntegrityInvariant},
             {warden::WardenCheckType::Mem,
                    warden::WardenEvidenceClass::IntegrityInvariant},
             {warden::WardenCheckType::Mem,
                    warden::WardenEvidenceClass::ThreatSignature}})
        {
            warden::WardenEvidence const mismatch = Evidence(1107,
                identity.first, identity.second,
                warden::WardenCheckOutcome::Mismatch);
            std::vector<warden::WardenPolicyDecision> const decisions =
                Confirm(mode, mismatch, mismatch);
            REQUIRE(decisions.size() == 1u);
            CHECK(decisions[0].action ==
                (mode == warden::WardenEnforcementMode::Observe ?
                    warden::WardenPolicyAction::PersistAudit :
                    warden::WardenPolicyAction::PersistAndKick));
        }
    }
}

TEST(WardenEnforcementPolicy_routes_corroboration_and_unavailable_to_audit)
{
    struct Case
    {
        uint32 id;
        warden::WardenCheckType type;
        warden::WardenEvidenceClass evidenceClass;
    };
    Case const cases[] =
    {
        {1, warden::WardenCheckType::Mpq,
            warden::WardenEvidenceClass::Corroboration},
        {2, warden::WardenCheckType::Lua,
            warden::WardenEvidenceClass::Corroboration},
        {1135, warden::WardenCheckType::Mem,
            warden::WardenEvidenceClass::Corroboration}
    };

    for (warden::WardenEnforcementMode mode :
        {warden::WardenEnforcementMode::Observe,
            warden::WardenEnforcementMode::Kick,
            warden::WardenEnforcementMode::KickAndBan})
    {
        for (Case const& test : cases)
        {
            for (warden::WardenCheckOutcome outcome :
                {warden::WardenCheckOutcome::Mismatch,
                    warden::WardenCheckOutcome::Unavailable})
            {
                warden::WardenEvidence const evidence = Evidence(test.id,
                    test.type, test.evidenceClass, outcome);
                std::vector<warden::WardenPolicyDecision> const decisions =
                    Confirm(mode, evidence, evidence);
                REQUIRE(decisions.size() == 1u);
                CHECK(decisions[0].action ==
                    warden::WardenPolicyAction::PersistAudit);
            }
        }
    }
}

TEST(WardenEnforcementPolicy_cross_outcomes_use_the_confirmed_result)
{
    warden::WardenEvidence mismatch = Evidence(1566,
        warden::WardenCheckType::Mem,
        warden::WardenEvidenceClass::ThreatSignature,
        warden::WardenCheckOutcome::Mismatch);
    warden::WardenEvidence unavailable = mismatch;
    unavailable.outcome = warden::WardenCheckOutcome::Unavailable;

    auto decisions = Confirm(warden::WardenEnforcementMode::KickAndBan,
        mismatch, unavailable);
    REQUIRE(decisions.size() == 1u);
    CHECK(decisions[0].action == warden::WardenPolicyAction::PersistAudit);
    CHECK(decisions[0].outcome == warden::WardenCheckOutcome::Unavailable);

    decisions = Confirm(warden::WardenEnforcementMode::KickAndBan,
        unavailable, mismatch);
    REQUIRE(decisions.size() == 1u);
    CHECK(decisions[0].action ==
        warden::WardenPolicyAction::PersistAndKick);
    CHECK(decisions[0].outcome == warden::WardenCheckOutcome::Mismatch);
}

TEST(WardenEnforcementPolicy_suppresses_confirmed_audits_per_outcome)
{
    warden::WardenEnforcementPolicy policy(
        warden::WardenEnforcementMode::Kick);
    warden::WardenEvidence mismatch = Evidence(2,
        warden::WardenCheckType::Lua,
        warden::WardenEvidenceClass::Corroboration,
        warden::WardenCheckOutcome::Mismatch);
    warden::WardenEvidence unavailable = mismatch;
    unavailable.outcome = warden::WardenCheckOutcome::Unavailable;

    REQUIRE(policy.EvaluateBatch(Batch(warden::CheckPlanPurpose::Recurring,
        {mismatch})).size() == 1u);
    auto audited = policy.EvaluateBatch(Batch(
        warden::CheckPlanPurpose::Confirmation, {mismatch}));
    REQUIRE(audited.size() == 1u);
    CHECK(audited[0].action == warden::WardenPolicyAction::PersistAudit);
    CHECK(policy.EvaluateBatch(Batch(warden::CheckPlanPurpose::Recurring,
        {mismatch})).empty());

    REQUIRE(policy.EvaluateBatch(Batch(warden::CheckPlanPurpose::Recurring,
        {unavailable})).size() == 1u);
    audited = policy.EvaluateBatch(Batch(
        warden::CheckPlanPurpose::Confirmation, {unavailable}));
    REQUIRE(audited.size() == 1u);
    CHECK(audited[0].action == warden::WardenPolicyAction::PersistAudit);
}

TEST(WardenEnforcementPolicy_match_is_not_suppressed_and_can_clear_new_pending)
{
    warden::WardenEnforcementPolicy policy(
        warden::WardenEnforcementMode::Kick);
    warden::WardenEvidence mismatch = Evidence(2,
        warden::WardenCheckType::Lua,
        warden::WardenEvidenceClass::Corroboration,
        warden::WardenCheckOutcome::Mismatch);
    REQUIRE(policy.EvaluateBatch(Batch(warden::CheckPlanPurpose::Recurring,
        {mismatch})).size() == 1u);
    auto audited = policy.EvaluateBatch(Batch(
        warden::CheckPlanPurpose::Confirmation, {mismatch}));
    REQUIRE(audited.size() == 1u);
    REQUIRE(audited[0].action == warden::WardenPolicyAction::PersistAudit);

    warden::WardenEvidence match = mismatch;
    match.outcome = warden::WardenCheckOutcome::Match;
    CHECK(policy.EvaluateBatch(Batch(warden::CheckPlanPurpose::Recurring,
        {match})).empty());

    warden::WardenEvidence unavailable = mismatch;
    unavailable.outcome = warden::WardenCheckOutcome::Unavailable;
    REQUIRE(policy.EvaluateBatch(Batch(warden::CheckPlanPurpose::Recurring,
        {unavailable})).size() == 1u);
    auto cleared = policy.EvaluateBatch(Batch(
        warden::CheckPlanPurpose::Confirmation, {match}));
    REQUIRE(cleared.size() == 1u);
    CHECK(cleared[0].action ==
        warden::WardenPolicyAction::ConfirmationCleared);
}

TEST(WardenEnforcementPolicy_outcome_flip_flood_emits_one_audit)
{
    warden::WardenEnforcementPolicy policy(
        warden::WardenEnforcementMode::Kick);
    warden::WardenEvidence mismatch = Evidence(1566,
        warden::WardenCheckType::Mem,
        warden::WardenEvidenceClass::ThreatSignature,
        warden::WardenCheckOutcome::Mismatch);
    warden::WardenEvidence unavailable = mismatch;
    unavailable.outcome = warden::WardenCheckOutcome::Unavailable;

    for (uint32 cycle = 0; cycle < 2; ++cycle)
    {
        auto queued = policy.EvaluateBatch(Batch(
            warden::CheckPlanPurpose::Recurring, {mismatch}));
        REQUIRE(queued.size() == 1u);
        CHECK(queued[0].action ==
            warden::WardenPolicyAction::QueueConfirmation);
        auto confirmed = policy.EvaluateBatch(Batch(
            warden::CheckPlanPurpose::Confirmation, {unavailable}));
        REQUIRE(confirmed.size() == 1u);
        CHECK(confirmed[0].action == (cycle == 0 ?
            warden::WardenPolicyAction::PersistAudit :
            warden::WardenPolicyAction::None));
    }
}

TEST(WardenEnforcementPolicy_confirmation_contract_checks_id_type_and_class)
{
    for (warden::WardenEnforcementMode mode :
        {warden::WardenEnforcementMode::Observe,
            warden::WardenEnforcementMode::Kick})
    {
        warden::WardenEvidence const pending = Evidence(1566,
            warden::WardenCheckType::Mem,
            warden::WardenEvidenceClass::ThreatSignature,
            warden::WardenCheckOutcome::Mismatch);
        for (uint32 mutation = 0; mutation < 4; ++mutation)
        {
            warden::WardenEnforcementPolicy policy(mode);
            REQUIRE(policy.EvaluateBatch(Batch(
                warden::CheckPlanPurpose::Recurring, {pending})).size() == 1u);
            warden::WardenEvidence wrong = pending;
            if (mutation == 0)
                wrong.checkId = 9999;
            else if (mutation == 1)
                wrong.checkType = warden::WardenCheckType::Mpq;
            else if (mutation == 2)
                wrong.evidenceClass =
                    warden::WardenEvidenceClass::IntegrityInvariant;
            else
            {
                wrong.checkType = warden::WardenCheckType::Timing;
                wrong.evidenceClass =
                    warden::WardenEvidenceClass::ProtocolHealth;
                wrong.outcome = warden::WardenCheckOutcome::Stable;
            }

            auto decisions = policy.EvaluateBatch(Batch(
                warden::CheckPlanPurpose::Confirmation, {wrong}));
            REQUIRE(decisions.size() == 1u);
            CHECK(decisions[0].action ==
                (mode == warden::WardenEnforcementMode::Observe ?
                    warden::WardenPolicyAction::None :
                    warden::WardenPolicyAction::Kick));
            CHECK(decisions[0].action !=
                warden::WardenPolicyAction::PersistAndKick);
        }
    }
}

TEST(WardenEnforcementPolicy_confirmation_contract_requires_exactly_one_item)
{
    warden::WardenEvidence const evidence = Evidence(1566,
        warden::WardenCheckType::Mem,
        warden::WardenEvidenceClass::ThreatSignature,
        warden::WardenCheckOutcome::Mismatch);
    for (warden::WardenEnforcementMode mode :
        {warden::WardenEnforcementMode::Observe,
            warden::WardenEnforcementMode::Kick})
    {
        for (warden::WardenEvidenceBatch const& malformed :
            {Batch(warden::CheckPlanPurpose::Confirmation, {}),
                Batch(warden::CheckPlanPurpose::Confirmation,
                    {evidence, evidence})})
        {
            warden::WardenEnforcementPolicy policy(mode);
            auto decisions = policy.EvaluateBatch(malformed);
            REQUIRE(decisions.size() == 1u);
            CHECK(decisions[0].action ==
                (mode == warden::WardenEnforcementMode::Observe ?
                    warden::WardenPolicyAction::None :
                    warden::WardenPolicyAction::Kick));
        }
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
