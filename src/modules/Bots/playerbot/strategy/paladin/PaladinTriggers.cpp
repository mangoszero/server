#include "botpch.h"
#include "../../playerbot.h"
#include "PaladinTriggers.h"
#include "PaladinActions.h"

using namespace ai;

bool SealTrigger::IsActive()
{
    Unit* target = GetTarget();
    return !ai->HasAura("seal of justice", target) &&
        !ai->HasAura("seal of command", target) &&
        !ai->HasAura("seal of vengeance", target) &&
        !ai->HasAura("seal of righteousness", target) &&
        !ai->HasAura("seal of light", target) &&
        !ai->HasAura("seal of wisdom", target) &&
        !ai->HasAura("seal of the crusader", target);
}

bool AuraTrigger::IsActive()
{
    Unit* target = GetTarget();
    return !ai->HasAura("devotion aura", target) &&
        !ai->HasAura("retribution aura", target) &&
        !ai->HasAura("concentration aura", target) &&
        !ai->HasAura("shadow resistance aura", target) &&
        !ai->HasAura("frost resistance aura", target) &&
        !ai->HasAura("fire resistance aura", target) &&
        !ai->HasAura("crusader aura", target);
}

bool CrusaderAuraTrigger::IsActive()
{
    Unit* target = GetTarget();
    return AI_VALUE2(bool, "mounted", "self target") && !ai->HasAura("crusader aura", target);
}

bool HolyWrathTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target)
        return false;
    uint32 ctype = target->GetCreatureType();
    if (ctype != CREATURE_TYPE_UNDEAD && ctype != CREATURE_TYPE_DEMON)
        return false;
    return AI_VALUE(uint8, "attacker count") >= 2;
}

bool ExorcismTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target)
        return false;
    uint32 ctype = target->GetCreatureType();
    return ctype == CREATURE_TYPE_UNDEAD || ctype == CREATURE_TYPE_DEMON;
}

bool BlessingOfFreedomTrigger::IsActive()
{
    Unit* bot = ai->GetBot();
    return bot->IsInRoots() || bot->HasAuraType(SPELL_AURA_MOD_DECREASE_SPEED);
}
