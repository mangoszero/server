#include "botpch.h"
#include "../../playerbot.h"
#include "../actions/GenericActions.h"
#include "HunterActions.h"
#include "../../PlayerbotFactory.h"

using namespace ai;

bool CastSerpentStingAction::isUseful()
{
    return AI_VALUE2(uint8, "health", "current target") > 50;
}

bool CastViperStingAction::isUseful()
{
    return AI_VALUE2(uint8, "mana", "self target") < 50 && AI_VALUE2(uint8, "mana", "current target") >= 30;
}

bool CastAspectOfTheCheetahAction::isUseful()
{
    return !ai->HasAnyAuraOf(GetTarget(), "aspect of the cheetah", "aspect of the pack", NULL);
}

Value<Unit*>* CastFreezingTrap::GetTargetValue()
{
    return context->GetValue<Unit*>("cc target", "freezing trap");
}

bool CastRevivePetAction::isPossible()
{
    if (bot->GetPet())
    {
        return CastBuffSpellAction::isPossible();
    }
    PetDatabaseStatus status = Pet::GetStatusFromDB(bot);
    return status == PET_DB_DEAD || status == PET_DB_NO_PET;
}

bool CastRevivePetAction::Execute(Event event)
{
    if (!bot->GetPet() && Pet::GetStatusFromDB(bot) == PET_DB_NO_PET)
    {
        PlayerbotFactory factory(bot, bot->getLevel());
        factory.InitPet();
        return true;
    }
    return CastBuffSpellAction::Execute(event);
}

bool CastIntimidationAction::isUseful()
{
    return CastSpellAction::isUseful() && AI_VALUE(Unit*, "pet target") != NULL;
}

bool FeedPetAction::isUseful()
{
    Unit* pet = GetTarget();
    if (!pet || !pet->IsAlive())
    {
        return false;
    }

    // Pet already has the Feed Pet Effect aura
    if (pet->HasAura(SPELL_ID_FEED_PET_EFFECT))
    {
        return false;
    }

    uint32 spellId = AI_VALUE2(uint32, "spell id", "feed pet");
    return spellId && AI_VALUE2(Item*, "item for spell", spellId);
}

bool HunterMeleeAction::isUseful()
{
    // Only swing if enemy is already in our face AND targeting us.
    //  Perhaps in the future a ranged/melee hunter strategy would be nice.
    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || !target->IsAlive())
    {
        return false;
    }
    bool victim = target->getVictim() == bot;
    float dist = AI_VALUE2(float, "distance", "current target");
    return victim && dist <= ATTACK_DISTANCE;
}

bool HunterMeleeAction::Execute(Event event)
{
    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target)
    {
        return false;
    }

    // Report what Attack actually DID, rather than always claiming success.
    //
    // Unit::Attack returns true only when it starts or switches to melee, and false when
    // this bot is already melee-attacking this same target. That distinction is what stops
    // this action starving the reposition above it: a successful action ends the engine
    // tick, so an unconditional true meant the hunter re-won every tick, white-swung
    // forever and never backed out to shoot again -- the mirror image of the bug where the
    // reposition starved melee.
    //
    // Auto-attack keeps swinging on its own once started, so one successful engage is all
    // this needs; from the next tick it declines and "hunter ensure ranged position" gets
    // its turn. Engage, then open range, which is what a hunter is supposed to do.
    return bot->Attack(target, true);
}
