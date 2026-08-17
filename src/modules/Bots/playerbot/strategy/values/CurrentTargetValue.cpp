#include "botpch.h"
#include "ObjectLookup.h"
#include "../../playerbot.h"
#include "CurrentTargetValue.h"
#include "PossibleTargetsValue.h"

using namespace ai;

// How long a visibility answer stays good for. "Current target" is consulted several
// times per tick by triggers and actions alike, so neither detection nor its vmap raycast
// needs re-deciding at 20Hz. A quarter second of staleness costs nothing: the server
// re-checks visibility and line of sight when the spell is actually cast.
#define CURRENT_TARGET_VISIBILITY_CACHE_MS 250

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

    uint32 now = getMSTime();
    if (m_visibilityTarget != selection ||
        getMSTimeDiff(m_visibilityChecked, now) >= CURRENT_TARGET_VISIBILITY_CACHE_MS)
    {
        m_visibilityTarget = selection;
        m_visibilityChecked = now;
        m_targetVisible = PossibleTargetsValue::IsVisibleForBot(bot, unit) &&
            bot->IsWithinLOSInMap(unit);
    }

    return m_targetVisible ? unit : NULL;
}

void CurrentTargetValue::Set(Unit* target)
{
    selection = target ? target->GetObjectGuid() : ObjectGuid();

    // A new target must be judged on its own, not on the last one's answer.
    m_visibilityTarget = ObjectGuid();
    m_visibilityChecked = 0;
    m_targetVisible = false;
}
