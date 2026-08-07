#include "botpch.h"
#include "ObjectLookup.h"
#include "../../playerbot.h"
#include "CurrentTargetValue.h"

using namespace ai;

// How long a line-of-sight answer stays good for. Every read used to be a fresh vmap
// raycast, and "current target" is consulted several times per tick by triggers and
// actions alike -- on the order of tens of thousands of raycasts a second across a fleet
// of 200. Sight of a target that has not changed does not need re-deciding at 20Hz, and a
// quarter second of staleness costs nothing: the server re-checks line of sight itself
// when the spell is actually cast.
#define CURRENT_TARGET_LOS_CACHE_MS 250

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
    if (m_losTarget != selection || getMSTimeDiff(m_losChecked, now) >= CURRENT_TARGET_LOS_CACHE_MS)
    {
        m_losTarget = selection;
        m_losChecked = now;
        m_inLos = bot->IsWithinLOSInMap(unit);
    }

    return m_inLos ? unit : NULL;
}

void CurrentTargetValue::Set(Unit* target)
{
    selection = target ? target->GetObjectGuid() : ObjectGuid();

    // A new target must be judged on its own, not on the last one's answer.
    m_losTarget = ObjectGuid();
    m_losChecked = 0;
    m_inLos = false;
}
