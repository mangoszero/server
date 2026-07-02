#pragma once

#include "../actions/GenericActions.h"

namespace ai
{
    class CastDemonSkinAction : public CastBuffSpellAction {
        public:
            CastDemonSkinAction(PlayerbotAI* ai) : CastBuffSpellAction(ai, "demon skin") {}
    };

    class CastDemonArmorAction : public CastBuffSpellAction
    {
        public:
            CastDemonArmorAction(PlayerbotAI* ai) : CastBuffSpellAction(ai, "demon armor") {}
    };

    BEGIN_RANGED_SPELL_ACTION(CastShadowBoltAction, "shadow bolt")
    END_SPELL_ACTION()

    BEGIN_RANGED_SPELL_ACTION(CastIncinerateAction, "incinerate")
    END_SPELL_ACTION()

    class CastDrainSoulAction : public CastSpellAction
    {
        public:
            CastDrainSoulAction(PlayerbotAI* ai) : CastSpellAction(ai, "drain soul") {}
            virtual bool isUseful()
            {
                return AI_VALUE2(uint8, "item count", "soul shard") < 2 &&
                       AI_VALUE2(bool, "spell cast useful", spell);
            }
    };

    class CastDrainManaAction : public CastSpellAction
    {
        public:
            CastDrainManaAction(PlayerbotAI* ai) : CastSpellAction(ai, "drain mana") {}
    };

    class CastDrainLifeAction : public CastSpellAction
    {
        public:
            CastDrainLifeAction(PlayerbotAI* ai) : CastSpellAction(ai, "drain life") {}
    };

    class CastCurseOfAgonyAction : public CastDebuffSpellAction
    {
        public:
            CastCurseOfAgonyAction(PlayerbotAI* ai) : CastDebuffSpellAction(ai, "curse of agony") {}
    };

    class CastCurseOfWeaknessAction : public CastDebuffSpellAction
    {
        public:
            CastCurseOfWeaknessAction(PlayerbotAI* ai) : CastDebuffSpellAction(ai, "curse of weakness") {}
    };

    class CastCorruptionAction : public CastDebuffSpellAction
    {
        public:
            CastCorruptionAction(PlayerbotAI* ai) : CastDebuffSpellAction(ai, "corruption") {}
    };

    class CastCorruptionOnAttackerAction : public CastDebuffSpellOnAttackerAction
    {
        public:
            CastCorruptionOnAttackerAction(PlayerbotAI* ai) : CastDebuffSpellOnAttackerAction(ai, "corruption") {}
    };

    class CastSummonVoidwalkerAction : public CastBuffSpellAction
    {
        public:
            CastSummonVoidwalkerAction(PlayerbotAI* ai) : CastBuffSpellAction(ai, "summon voidwalker") {}
    };

    class CastSummonImpAction : public CastBuffSpellAction
    {
        public:
            CastSummonImpAction(PlayerbotAI* ai) : CastBuffSpellAction(ai, "summon imp") {}
    };

    class CastCreateHealthstoneAction : public CastBuffSpellAction
    {
        public:
            CastCreateHealthstoneAction(PlayerbotAI* ai) : CastBuffSpellAction(ai, "create healthstone") {}
    };

    class CastCreateFirestoneAction : public CastBuffSpellAction
    {
        public:
            CastCreateFirestoneAction(PlayerbotAI* ai) : CastBuffSpellAction(ai, "create firestone") {}
            virtual bool isUseful();
    };

    class CastCreateSpellstoneAction : public CastBuffSpellAction
    {
        public:
            CastCreateSpellstoneAction(PlayerbotAI* ai) : CastBuffSpellAction(ai, "create spellstone") {}
            virtual bool isUseful();
    };

    class EquipSpellstoneAction : public Action
    {
        public:
            EquipSpellstoneAction(PlayerbotAI* ai) : Action(ai, "equip spellstone") {}
            virtual bool Execute(Event event);
            virtual bool isUseful();
    };

    class CastBanishAction : public CastBuffSpellAction
    {
        public:
            CastBanishAction(PlayerbotAI* ai) : CastBuffSpellAction(ai, "banish on cc") {}
            virtual Value<Unit*>* GetTargetValue()
            {
                return context->GetValue<Unit*>("cc target", "banish");
            }

            virtual bool Execute(Event event) { return ai->CastSpell("banish", GetTarget()); }
    };

    class CastRainOfFireAction : public CastSpellAction
    {
        public:
            CastRainOfFireAction(PlayerbotAI* ai) : CastSpellAction(ai, "rain of fire") {}
            virtual bool isUseful();
    };

    class CastImmolateAction : public CastDebuffSpellAction
    {
        public:
            CastImmolateAction(PlayerbotAI* ai) : CastDebuffSpellAction(ai, "immolate") {}
    };

    class CastConflagrateAction : public CastSpellAction
    {
        public:
            CastConflagrateAction(PlayerbotAI* ai) : CastSpellAction(ai, "conflagrate") {}
    };

    class CastSeedOfCorruptionAction : public CastDebuffSpellAction
    {
        public:
            CastSeedOfCorruptionAction(PlayerbotAI* ai) : CastDebuffSpellAction(ai, "seed of corruption") {}
    };

    BEGIN_RANGED_SPELL_ACTION(CastShadowfuryAction, "shadowfury")
    END_SPELL_ACTION()

    class CastFearAction : public CastDebuffSpellAction
    {
        public:
            CastFearAction(PlayerbotAI* ai) : CastDebuffSpellAction(ai, "fear") {}
            virtual bool isUseful()
            {
                return CastDebuffSpellAction::isUseful() &&
                    (!ai->HasStrategy("cautious") || !ai->GetGroupTank(bot));
            }
    };

    class CastFearOnCcAction : public CastBuffSpellAction
    {
        public:
            CastFearOnCcAction(PlayerbotAI* ai) : CastBuffSpellAction(ai, "fear on cc") {}
            virtual Value<Unit*>* GetTargetValue()
            {
                return context->GetValue<Unit*>("cc target", "fear");
            }

            virtual bool Execute(Event event) { return ai->CastSpell("fear", GetTarget()); }
            virtual bool isUseful()
            {
                return CastBuffSpellAction::isUseful() &&
                    (!ai->HasStrategy("cautious") || !ai->GetGroupTank(bot));
            }
    };

    class CastLifeTapAction: public CastSpellAction
    {
        public:
            CastLifeTapAction(PlayerbotAI* ai) : CastSpellAction(ai, "life tap") {}
            virtual string GetTargetName()
            {
                return "self target";
            }

            virtual bool isUseful()
            {
                return AI_VALUE2(uint8, "health", "self target") > sPlayerbotAIConfig.lowHealth;
            }
    };
}
