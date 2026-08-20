#include <string>
#include "botpch.h"
#include "../../playerbot.h"
#include "GenericTriggers.h"
#include "../../LootObjectStack.h"
#include "../../PlayerbotAIConfig.h"

using namespace ai;

bool LowManaTrigger::IsActive()
{
    return AI_VALUE2(bool, "has mana", "self target") && AI_VALUE2(uint8, "mana", "self target") < sPlayerbotAIConfig.lowMana;
}

bool LowManaHasAggroTrigger::IsActive()
{
    if (!AI_VALUE2(bool, "has mana", "self target") ||
        AI_VALUE2(uint8, "mana", "self target") >= sPlayerbotAIConfig.lowMana ||
        !AI_VALUE2(bool, "has aggro", "current target"))
    {
        return false;
    }

    time_t now = time(nullptr);
    if (lastLowManaFlee_ == 0 || (now - lastLowManaFlee_) >= LOW_MANA_FLEE_COOLDOWN)
    {
        lastLowManaFlee_ = now;
        return true;
    }
    return false;
}

bool MediumManaTrigger::IsActive()
{
    return AI_VALUE2(bool, "has mana", "self target") && AI_VALUE2(uint8, "mana", "self target") < sPlayerbotAIConfig.mediumMana;
}

bool ThirstyTrigger::IsActive()
{
    return ai->IsDrinking() || (AI_VALUE2(bool, "has mana", "self target") &&
            AI_VALUE2(uint8, "mana", "self target") < sPlayerbotAIConfig.thirstyMana &&
            !AI_VALUE2(bool, "swimming", "self target"));
}

bool RageAvailable::IsActive()
{
    return AI_VALUE2(uint8, "rage", "self target") >= amount;
}

bool EnergyAvailable::IsActive()
{
    return AI_VALUE2(uint8, "energy", "self target") >= amount;
}

bool ComboPointsAvailableTrigger::IsActive()
{
    return AI_VALUE2(uint8, "combo", "current target") >= amount;
}

bool LoseAggroTrigger::IsActive()
{
    return !AI_VALUE2(bool, "has aggro", "current target");
}

bool HasAggroTrigger::IsActive()
{
    return AI_VALUE2(bool, "has aggro", "current target");
}

bool PanicTrigger::IsActive()
{
    return AI_VALUE2(uint8, "health", "self target") < sPlayerbotAIConfig.criticalHealth &&
        (!AI_VALUE2(bool, "has mana", "self target") || AI_VALUE2(uint8, "mana", "self target") < sPlayerbotAIConfig.lowMana);
}

bool BuffTrigger::IsActive()
{
    Unit* target = GetTarget();
    return SpellTrigger::IsActive() &&
        !ai->HasAura(spell, target) &&
        (!AI_VALUE2(bool, "has mana", "self target") || AI_VALUE2(uint8, "mana", "self target") > sPlayerbotAIConfig.lowMana);
}

Value<Unit*>* BuffOnPartyTrigger::GetTargetValue()
{
    return context->GetValue<Unit*>("party member without aura", spell);
}

Value<Unit*>* DebuffOnAttackerTrigger::GetTargetValue()
{
    return context->GetValue<Unit*>("attacker without aura", spell);
}

bool HasThreatTrigger::IsActive()
{
    return !bot->getAttackers().empty();
}

bool NoAttackersTrigger::IsActive()
{
    return !AI_VALUE(Unit*, "current target") && AI_VALUE(uint8, "attacker count") > 0;
}

bool InvalidTargetTrigger::IsActive()
{
    return AI_VALUE2(bool, "invalid target", "current target");
}

bool NoTargetTrigger::IsActive()
{
    return !AI_VALUE(Unit*, "current target");
}

bool MyAttackerCountTrigger::IsActive()
{
    return AI_VALUE(uint8, "my attacker count") >= amount;
}

bool AoeTrigger::IsActive()
{
    return AI_VALUE(uint8, "attacker count") >= amount;
}

bool DebuffTrigger::IsActive()
{
    return BuffTrigger::IsActive() && AI_VALUE2(uint8, "health", "current target") > 25;
}

bool SpellTrigger::IsActive()
{
    // DO NOT gate this on whether the spell resolves. It was tried and reverted, and the
    // reason is architectural rather than incidental.
    //
    // A trigger's name is NOT an assertion that the spell exists or is trained. It is the
    // entry label of an ActionNode cascade that resolves an INTENT down to whatever this bot
    // can actually cast. MageBuffDpsStrategy shows the shape plainly: it triggers on
    // "mage armor" and fires the action "molten armor", and neither has to be castable --
    // the cascade steps down to Frost Armor, which a level 1 mage has. Refusing here severs
    // the entry point and the whole ladder below it goes with it.
    //
    // Three regressions were found from a single such gate, and only after review:
    //   - ShamanBuffManaStrategy reaches Lightning Shield through the WotLK "water shield",
    //     the only shield route for Elemental, Restoration and untalented shamans.
    //   - DpsPaladinStrategy reaches the real Judgement through "judgement of wisdom" then
    //     "judgement of light", which are debuff names and never castable. Gating stopped
    //     Retribution bots judging at all, while they kept maintaining a seal to judge.
    //   - MageBuffManaStrategy/MageBuffDpsStrategy key on "mage armor", learned at 34, so
    //     every mage below that lost its armor buff entirely.
    //
    // The last one generalises: any ladder that steps down from a higher-level spell breaks
    // for every bot below that level, so the exposure is open-ended rather than a fixed list
    // of post-1.12 names. Unresolvable names here are load-bearing scaffolding, not litter.
    // The cost of leaving them is one cached value lookup per evaluation, and CastSpellAction
    // ::isPossible -> CanCastSpell already refuses the cast itself.
    return GetTarget();
}

bool SpellCanBeCastTrigger::IsActive()
{
    Unit* target = GetTarget();
    return target && ai->CanCastSpell(spell, target);
}

bool RandomTrigger::IsActive()
{
    int vl  = rand() % (int)(1 + probability * 10 / sPlayerbotAIConfig.randomChangeMultiplier);
    return vl == 0;
}

bool AndTrigger::IsActive()
{
    return ls->IsActive() && rs->IsActive();
}

string AndTrigger::getName()
{
    std::string name(ls->getName());
    name = name + " and ";
    name = name + rs->getName();
    return name;
}

bool BoostTrigger::IsActive()
{
    return BuffTrigger::IsActive() && AI_VALUE(uint8, "balance") <= balance;
}

bool SnareTargetTrigger::IsActive()
{
    Unit* target = GetTarget();
    return DebuffTrigger::IsActive() && AI_VALUE2(bool, "moving", "current target") && !ai->HasAura(spell, target);
}

bool ItemCountTrigger::IsActive()
{
    return AI_VALUE2(uint8, "item count", item) < count;
}

bool InterruptSpellTrigger::IsActive()
{
    return SpellTrigger::IsActive() && ai->IsInterruptableSpellCasting(GetTarget(), getName());
}

bool HasAuraTrigger::IsActive()
{
    return ai->HasAura(getName(), GetTarget());
}

bool TankAoeTrigger::IsActive()
{
    if (!AI_VALUE(uint8, "attacker count"))
    {
        return false;
    }

    Unit* currentTarget = AI_VALUE(Unit*, "current target");
    if (!currentTarget)
    {
        return true;
    }

    Unit* tankTarget = AI_VALUE(Unit*, "tank target");
    if (!tankTarget || currentTarget == tankTarget)
    {
        return false;
    }

    return currentTarget->getVictim() == AI_VALUE(Unit*, "self target");
}

bool IsBehindTargetTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    return target && AI_VALUE2(bool, "behind", "current target");
}

bool IsNotFacingTargetTrigger::IsActive()
{
    return !AI_VALUE2(bool, "facing", "current target");
}

bool HasCcTargetTrigger::IsActive()
{
    return AI_VALUE(uint8, "attacker count") > 2 && AI_VALUE2(Unit*, "cc target", getName()) &&
        !AI_VALUE2(Unit*, "current cc target", getName());
}

bool NoMovementTrigger::IsActive()
{
    return !AI_VALUE2(bool, "moving", "self target");
}

bool NoPossibleTargetsTrigger::IsActive()
{
    list<ObjectGuid> targets = AI_VALUE(list<ObjectGuid>, "possible targets");
    return !targets.size();
}

bool NotLeastHpTargetActiveTrigger::IsActive()
{
    Unit* leastHp = AI_VALUE(Unit*, "least hp target");
    Unit* target = AI_VALUE(Unit*, "current target");
    return leastHp && target != leastHp;
}

bool NoTanksTargetActiveTrigger::IsActive()
{
    Unit* tanksTarget = AI_VALUE(Unit*, "dps tanks target");
    Unit* target = AI_VALUE(Unit*, "current target");
    return tanksTarget && target != tanksTarget;
}

bool EnemyPlayerIsAttacking::IsActive()
{
    Unit* enemyPlayer = AI_VALUE(Unit*, "enemy player target");
    Unit* target = AI_VALUE(Unit*, "current target");
    return enemyPlayer && target != enemyPlayer;
}

bool IsSwimmingTrigger::IsActive()
{
    return AI_VALUE2(bool, "swimming", "self target");
}

bool DrowningTrigger::IsActive()
{
    if (!bot->IsDrowning())
    {
        return false;
    }

    if (bot->HasAuraType(SPELL_AURA_WATER_BREATHING))
    {
        return false;
    }

    return true;
}

bool HasNearestAddsTrigger::IsActive()
{
    list<ObjectGuid> targets = AI_VALUE(list<ObjectGuid>, "nearest adds");
    return targets.size();
}

bool HasItemForSpellTrigger::IsActive()
{
    string spell = getName();
    uint32 spellId = AI_VALUE2(uint32, "spell id", spell);
    return spellId && AI_VALUE2(Item*, "item for spell", spellId);
}

bool TargetChangedTrigger::IsActive()
{
    Unit* oldTarget = context->GetValue<Unit*>("old target")->Get();
    Unit* target = context->GetValue<Unit*>("current target")->Get();
    return target && oldTarget != target;
}

Value<Unit*>* InterruptEnemyHealerTrigger::GetTargetValue()
{
    return context->GetValue<Unit*>("enemy healer target", spell);
}
