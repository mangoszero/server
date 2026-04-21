#pragma once

#include "../actions/GenericActions.h"

namespace ai
{
    BEGIN_RANGED_SPELL_ACTION(CastHuntersMarkAction, "hunter's mark")
    END_SPELL_ACTION()

    BEGIN_RANGED_SPELL_ACTION(CastAutoShotAction, "auto shot")
    END_SPELL_ACTION()

    BEGIN_RANGED_SPELL_ACTION(CastArcaneShotAction, "arcane shot")
    END_SPELL_ACTION()

    BEGIN_RANGED_SPELL_ACTION(CastExplosiveShotAction, "explosive shot")
    END_SPELL_ACTION()

    BEGIN_RANGED_SPELL_ACTION(CastAimedShotAction, "aimed shot")
    END_SPELL_ACTION()

    BEGIN_RANGED_SPELL_ACTION(CastConcussiveShotAction, "concussive shot")
    END_SPELL_ACTION()

    BEGIN_RANGED_SPELL_ACTION(CastDistractingShotAction, "distracting shot")
    END_SPELL_ACTION()

    BEGIN_RANGED_SPELL_ACTION(CastMultiShotAction, "multi-shot")
    END_SPELL_ACTION()

    BEGIN_RANGED_SPELL_ACTION(CastVolleyAction, "volley")
    END_SPELL_ACTION()

    BEGIN_RANGED_SPELL_ACTION(CastSerpentStingAction, "serpent sting")
    virtual bool isUseful();
    END_SPELL_ACTION()

    BEGIN_RANGED_SPELL_ACTION(CastWyvernStingAction, "wyvern sting")
    END_SPELL_ACTION()

    BEGIN_RANGED_SPELL_ACTION(CastViperStingAction, "viper sting")
    virtual bool isUseful();
    END_SPELL_ACTION()

    BEGIN_RANGED_SPELL_ACTION(CastScorpidStingAction, "scorpid sting")
    END_SPELL_ACTION()

    class CastAspectOfTheHawkAction : public CastBuffSpellAction
    {
    public:
        CastAspectOfTheHawkAction(PlayerbotAI* ai) : CastBuffSpellAction(ai, "aspect of the hawk") {}
    };

    class CastAspectOfTheWildAction : public CastBuffSpellAction
    {
    public:
        CastAspectOfTheWildAction(PlayerbotAI* ai) : CastBuffSpellAction(ai, "aspect of the wild") {}
    };

    class CastAspectOfTheCheetahAction : public CastBuffSpellAction
    {
    public:
        CastAspectOfTheCheetahAction(PlayerbotAI* ai) : CastBuffSpellAction(ai, "aspect of the cheetah") {}
        virtual bool isUseful();
    };

    class CastAspectOfThePackAction : public CastBuffSpellAction
    {
    public:
        CastAspectOfThePackAction(PlayerbotAI* ai) : CastBuffSpellAction(ai, "aspect of the pack") {}
    };

    class CastCallPetAction : public CastBuffSpellAction
    {
    public:
        CastCallPetAction(PlayerbotAI* ai) : CastBuffSpellAction(ai, "call pet") {}
    };

    class CastMendPetAction : public CastAuraSpellAction
    {
    public:
        CastMendPetAction(PlayerbotAI* ai) : CastAuraSpellAction(ai, "mend pet") {}
        virtual string GetTargetName() { return "pet target"; }
    };

    class CastRevivePetAction : public CastBuffSpellAction
    {
    public:
        CastRevivePetAction(PlayerbotAI* ai) : CastBuffSpellAction(ai, "revive pet") {}
        virtual bool isPossible();
        virtual bool Execute(Event event);
    };

    class CastTrueshotAuraAction : public CastBuffSpellAction
    {
    public:
        CastTrueshotAuraAction(PlayerbotAI* ai) : CastBuffSpellAction(ai, "trueshot aura") {}
    };

    class CastFeignDeathAction : public CastBuffSpellAction
    {
    public:
        CastFeignDeathAction(PlayerbotAI* ai) : CastBuffSpellAction(ai, "feign death") {}
    };

    class CastRapidFireAction : public CastBuffSpellAction
    {
    public:
        CastRapidFireAction(PlayerbotAI* ai) : CastBuffSpellAction(ai, "rapid fire") {}
    };

    class CastFreezingTrap : public CastDebuffSpellAction
    {
    public:
        CastFreezingTrap(PlayerbotAI* ai) : CastDebuffSpellAction(ai, "freezing trap") {}
        virtual Value<Unit*>* GetTargetValue();
    };

    class CastWingClipAction : public CastMeleeSpellAction
    {
    public:
        CastWingClipAction(PlayerbotAI* ai) : CastMeleeSpellAction(ai, "wing clip") {}
        virtual bool isUseful()
        {
            Unit* target = GetTarget();
            return target && target->IsAlive() && CastMeleeSpellAction::isUseful() && !ai->HasAura(spell, target) && target->getVictim() == bot;
        }
    };

    class CastSerpentStingOnAttackerAction : public CastDebuffSpellOnAttackerAction
    {
    public:
        CastSerpentStingOnAttackerAction(PlayerbotAI* ai) : CastDebuffSpellOnAttackerAction(ai, "serpent sting") {}
    };

    BEGIN_MELEE_SPELL_ACTION(CastDisengageAction, "disengage")
    END_SPELL_ACTION()

    BEGIN_MELEE_SPELL_ACTION(CastImmolationTrapAction, "immolation trap")
    END_SPELL_ACTION()

    BEGIN_MELEE_SPELL_ACTION(CastFrostTrapAction, "frost trap")
    END_SPELL_ACTION()

    BEGIN_MELEE_SPELL_ACTION(CastExplosiveTrapAction, "explosive trap")
    END_SPELL_ACTION()

    BEGIN_RANGED_SPELL_ACTION(CastScatterShotAction, "scatter shot")
    END_SPELL_ACTION()

    class CastBestialWrathAction : public CastAuraSpellAction
    {
    public:
        CastBestialWrathAction(PlayerbotAI* ai) : CastAuraSpellAction(ai, "bestial wrath") {}
        virtual string GetTargetName() { return "pet target"; }
        virtual bool isUseful() { return CastAuraSpellAction::isUseful() && AI_VALUE(Unit*, "pet target") != NULL; }
    };

    class CastMongooseBiteAction : public CastMeleeSpellAction
    {
    public:
        CastMongooseBiteAction(PlayerbotAI* ai) : CastMeleeSpellAction(ai, "mongoose bite") {}
        virtual bool isPossible() { return bot->HasAuraState(AURA_STATE_DEFENSE) && CastMeleeSpellAction::isPossible(); }
    };

    class CastIntimidationAction : public CastSpellAction
    {
    public:
        CastIntimidationAction(PlayerbotAI* ai) : CastSpellAction(ai, "intimidation") {}
        virtual bool isUseful();
    };

    class HunterMeleeAction : public Action
    {
    public:
        HunterMeleeAction(PlayerbotAI* ai) : Action(ai, "hunter melee") {}
        virtual bool Execute(Event event);
        virtual bool isUseful();
    };

    class HunterEnsureRangedPositionAction : public MovementAction
    {
    private:
        const float minShootDistance = 10.0;
    public:
        HunterEnsureRangedPositionAction(PlayerbotAI* ai) : MovementAction(ai, "hunter ensure ranged position") {}

        virtual bool Execute(Event event)
        {
            Unit* target = AI_VALUE(Unit*, "current target");
            if(bot->GetDistance(target) > sPlayerbotAIConfig.spellDistance)
                return MoveTo(target, sPlayerbotAIConfig.spellDistance - 1.0);
            else
                return MoveTo(target, minShootDistance + 1.0);
        }
        virtual bool isUseful()
        {
            Unit* target = AI_VALUE(Unit*, "current target");
            if (!target || !target->IsAlive() || (target->getVictim() == bot)) return false;
            float distance = bot->GetDistance(target);
            return distance < minShootDistance || distance > sPlayerbotAIConfig.spellDistance;
        }
    };
}
