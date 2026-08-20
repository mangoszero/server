#include "botpch.h"
#include "../../playerbot.h"
#include "GenericActions.h"
#include "OutOfRangeMoverPolicy.h"
#include "../values/SpellRangeValue.h"

using namespace ai;

bool CastSpellAction::Execute(Event event)
{
    return ai->CastSpell(spell, GetTarget());
}

void CastSpellAction::UpdateRange(uint32 spellId)
{
    // Subclasses that pin a policy range opted out via UseFixedRange. Otherwise
    // re-clamp only when the resolved id changed -- a spell learned after this
    // action was constructed, or a new rank -- so the steady-state cost is one
    // integer compare. "spell id" itself refreshes every five AI updates, which
    // bounds how long a stale range can survive.
    if (!rangeFollowsSpell || spellId == lastRangeSpellId)
    {
        return;
    }
    lastRangeSpellId = spellId;
    range = SpellRangeValue::EffectiveRange(spellId);
}

bool CastSpellAction::isPossible()
{
    uint32 spellId = AI_VALUE2(uint32, "spell id", spell);
    UpdateRange(spellId);

    if (AI_VALUE2(float, "distance", GetTargetName()) > range)
    {
        return false;
    }
    if (spellId)
    {
        const SpellEntry* spellInfo = sSpellStore.LookupEntry(spellId);
        if (spellInfo && IsAutoRepeatRangedSpell(spellInfo))
        {
            Spell* currentAutoRepeat = bot->GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL);
            if (currentAutoRepeat && currentAutoRepeat->m_spellInfo->ID == spellId)
            {
                return false; // Already casting this autorepeat spell
            }
        }
    }
    return ai->CanCastSpell(spell, GetTarget());
}

NextAction** CastSpellAction::getImpossiblePrerequisites()
{
    // Resolve the actual spell FIRST and return NULL when the name does not
    // resolve, so the Engine follows the ActionNode alternatives instead. Many
    // action names are intent labels heading a cascade ("mage armor",
    // "judgement of wisdom", "water shield") and a mover pushed for one of those
    // would sever the step-down that makes them work -- the exact regression
    // documented in GenericTriggers.cpp. The full rule set, and the reasoning
    // behind each rule, lives with ChooseOutOfRangeMover.
    uint32 spellId = AI_VALUE2(uint32, "spell id", spell);

    // Compare the target VALUE OBJECTS, not GetTargetName(): enemy-healer actions
    // override GetTargetValue() while still inheriting "current target" as their
    // target name, so a name comparison would wrongly include them.
    bool targetIsCurrentTarget = GetTargetValue() == context->GetValue<Unit*>("current target");

    UpdateRange(spellId);
    float distance = AI_VALUE2(float, "distance", GetTargetName());

    switch (ChooseOutOfRangeMover(spellId, targetIsCurrentTarget, distance, range, ATTACK_DISTANCE))
    {
        case OUT_OF_RANGE_MOVER_REACH_SPELL:
        {
            // The qualifier makes the mover close to THIS spell's DBC range rather
            // than the global spellDistance -- deliberately walking a ranged bot to
            // 19 (or 14) yards when a short-ranged spell wins selection. That extra
            // exposure is the accepted cost of being able to cast at all, and it
            // composes with the existing back-off rules: "enemy too close for
            // spell" fires at spellDistance / 2 = 15 yards, inside 19, so closing
            // does not oscillate.
            std::ostringstream mover;
            mover << "reach spell::" << spellId;
            return NextAction::array(0, new NextAction(mover.str()), NULL);
        }
        case OUT_OF_RANGE_MOVER_REACH_MELEE:
            return NextAction::array(0, new NextAction("reach melee"), NULL);
        default:
            return NULL;
    }
}

bool CastSpellAction::isUseful()
{
    return GetTarget() && AI_VALUE2(bool, "spell cast useful", spell);
}

bool CastAuraSpellAction::isUseful()
{
    return CastSpellAction::isUseful() && !ai->HasAura(spell, GetTarget());
}

bool CastDebuffSpellAction::isUseful()
{
    if (!CastAuraSpellAction::isUseful())
    {
        return false;
    }

    Unit* target = GetTarget();
    if (!target)
    {
        return true;
    }

    Player* bot = ai->GetBot();
    Group* group = bot->GetGroup();
    if (!group || group->GetTargetIcon(4) != target->GetObjectGuid())
    {
        return true;
    }

    Player* tank = ai->GetGroupTank(bot);
    if (!tank)
    {
        return true;
    }

    Unit* tankVictim = tank->getVictim();
    if (tankVictim && tankVictim->GetObjectGuid() == target->GetObjectGuid())
    {
        return true;
    }

    uint32 spellId = AI_VALUE2(uint32, "spell id", spell);
    if (spellId)
    {
        const SpellEntry* spellInfo = sSpellStore.LookupEntry(spellId);
        if (spellInfo)
        {
            for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
            {
                if (spellInfo->EffectAura[j] == SPELL_AURA_PERIODIC_DAMAGE ||
                    spellInfo->EffectAura[j] == SPELL_AURA_PERIODIC_DAMAGE_PERCENT)
                {
                    return false;
                }
            }
        }
    }

    return true;
}

bool CastEnchantItemAction::isUseful()
{
    if (!CastSpellAction::isUseful())
    {
        return false;
    }

    uint32 spellId = AI_VALUE2(uint32, "spell id", spell);
    return spellId && AI_VALUE2(Item*, "item for spell", spellId);
}

bool CastHealingSpellAction::isUseful()
{
    return CastAuraSpellAction::isUseful() && AI_VALUE2(uint8, "health", GetTargetName()) < (100 - estAmount);
}

bool CastAoeHealSpellAction::isUseful()
{
    return CastSpellAction::isUseful() && AI_VALUE2(uint8, "aoe heal", "medium") > 0;
}

Value<Unit*>* CurePartyMemberAction::GetTargetValue()
{
    return context->GetValue<Unit*>("party member to dispel", dispelType);
}

Value<Unit*>* BuffOnPartyAction::GetTargetValue()
{
    return context->GetValue<Unit*>("party member without aura", spell);
}
