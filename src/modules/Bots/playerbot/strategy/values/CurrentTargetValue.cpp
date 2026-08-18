#include "botpch.h"
#include "ObjectLookup.h"
#include "../../playerbot.h"
#include "CurrentTargetValue.h"
#include "PossibleTargetsValue.h"

using namespace ai;

// "Current target" is consulted several times per tick by triggers and actions alike.
// Cache only the vmap raycast; concealment can change combat ownership synchronously and
// must be checked on every read.
#define CURRENT_TARGET_LOS_CACHE_MS 250

bool CurrentTargetValue::IsBotOrPetAttacking(Unit const* target) const
{
    // A null target is nobody's victim. Without this, getVictim() returning NULL for an
    // idle bot would compare equal and claim an attack that does not exist.
    if (!target)
    {
        return false;
    }

    if (bot->getVictim() == target)
    {
        return true;
    }

    Pet* pet = bot->GetPet();
    return pet && pet->getVictim() == target;
}

bool CurrentTargetValue::IsConcealed(Unit const* target) const
{
    return target && target->GetVisibility() != VISIBILITY_ON;
}

bool CurrentTargetValue::MustDisengageFromConcealedTarget(Unit const* target) const
{
    return target && bot->IsAlive() &&
        m_concealmentTarget == target->GetObjectGuid() &&
        m_targetContextRevision == ai->GetTargetContextRevision() &&
        !m_concealmentWasAccepted && IsConcealed(target) && IsBotOrPetAttacking(target);
}

Unit* CurrentTargetValue::Get()
{

    if (selection.IsEmpty())
    {
        return NULL;
    }

    Unit* unit = ObjectLookup::GetUnit(*bot, selection);
    if (!unit)
    {
        return NULL;
    }

    uint32 targetContextRevision = ai->GetTargetContextRevision();
    if (m_concealmentTarget != selection || m_targetContextRevision != targetContextRevision)
    {
        m_concealmentTarget = selection;
        m_targetContextRevision = targetContextRevision;
        m_concealmentWasAccepted = false;
    }

    if (!bot->IsAlive() || ai->GetState() != BOT_STATE_COMBAT ||
        !IsBotOrPetAttacking(unit))
    {
        // A positive pre-attack detection is valid only for this live attack. Death,
        // an engine transition, or a real disengagement removes that authorization.
        m_concealmentWasAccepted = false;
    }

    // Unit::IsVisibleForOrDetect treats any attacker as proof that stealth is broken.
    // If this bot (or its pet) is that attacker, accepting the answer would make the
    // attack prove its own target valid forever. Keep returning NULL until DropTargetAction
    // removes the live attack; a later read in this same tick must not undo the decision.
    if (MustDisengageFromConcealedTarget(unit))
    {
        return NULL;
    }

    if (!PossibleTargetsValue::IsVisibleForBot(bot, unit))
    {
        return NULL;
    }

    if (!IsConcealed(unit))
    {
        m_concealmentWasAccepted = false;
    }
    else if (bot->IsAlive() && ai->GetState() == BOT_STATE_COMBAT &&
        !IsBotOrPetAttacking(unit))
    {
        // Remember only a positive detection made before our own attack could satisfy
        // Unit::IsVisibleForOrDetect's attacker shortcut.
        m_concealmentWasAccepted = true;
    }

    uint32 now = getMSTime();
    if (m_losTarget != selection ||
        getMSTimeDiff(m_losChecked, now) >= CURRENT_TARGET_LOS_CACHE_MS)
    {
        m_losTarget = selection;
        m_losChecked = now;
        m_targetInLos = bot->IsWithinLOSInMap(unit);
    }

    return m_targetInLos ? unit : NULL;
}

void CurrentTargetValue::Set(Unit* target)
{
    ObjectGuid newSelection = target ? target->GetObjectGuid() : ObjectGuid();
    uint32 targetContextRevision = ai->GetTargetContextRevision();
    bool sameObservation = target && selection == newSelection &&
        m_concealmentTarget == newSelection &&
        m_targetContextRevision == targetContextRevision;

    selection = newSelection;

    // Always invalidate LOS, even for the same GUID. Preserve only a still-live
    // concealment observation so Set() cannot erase a transition later in the same tick.
    m_losTarget = ObjectGuid();
    m_losChecked = 0;
    m_targetInLos = false;

    if (!sameObservation || !bot->IsAlive() || ai->GetState() != BOT_STATE_COMBAT ||
        !IsBotOrPetAttacking(target))
    {
        m_concealmentTarget = newSelection;
        m_targetContextRevision = targetContextRevision;
        m_concealmentWasAccepted = target && bot->IsAlive() &&
            ai->GetState() == BOT_STATE_COMBAT && IsConcealed(target) &&
            !IsBotOrPetAttacking(target) &&
            PossibleTargetsValue::IsVisibleForBot(bot, target);
    }
    else if (!IsConcealed(target))
    {
        // Do not preserve a detection authorization across a visible interval. If the same
        // unit conceals itself again before Get(), that transition must be caught.
        m_concealmentWasAccepted = false;
    }
}
