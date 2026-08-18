#pragma once

#include "../triggers/GenericTriggers.h"

namespace ai
{
    BEGIN_TRIGGER(HunterNoStingsActiveTrigger, Trigger)
    END_TRIGGER()

    class HunterAspectOfTheHawkTrigger : public BuffTrigger
    {
        public:
            HunterAspectOfTheHawkTrigger(PlayerbotAI* ai) : BuffTrigger(ai, "aspect of the hawk")
            {
                checkInterval = 1;
            }
    };

    class HunterAspectOfTheWildTrigger : public BuffTrigger
    {
        public:
            HunterAspectOfTheWildTrigger(PlayerbotAI* ai) : BuffTrigger(ai, "aspect of the wild")
            {
                checkInterval = 1;
            }
    };

    class HunterAspectOfThePackTrigger : public BuffTrigger
    {
        public:
            HunterAspectOfThePackTrigger(PlayerbotAI* ai) : BuffTrigger(ai, "aspect of the pack") {}

            virtual bool IsActive()
            {
                return BuffTrigger::IsActive() && !ai->HasAura("aspect of the cheetah", GetTarget());
            }
    };

    // Not BEGIN_TRIGGER: that passes only `ai`, leaving checkInterval at its default of
    // 1, and when the pet is not in world this trigger falls back to a character_pet
    // SELECT. Every tick, per petless hunter, on a map worker thread. Throttled to match
    // HuntersPetUnhappyTrigger below; the cheap in-world path is gated the same way, but
    // a bot taking a few seconds longer to notice it needs a pet costs nothing.
    class HuntersPetDeadTrigger : public Trigger
    {
        public:
            HuntersPetDeadTrigger(PlayerbotAI* ai) : Trigger(ai, "hunters pet dead", 300) {}
        public:
            virtual bool IsActive();
    };

    BEGIN_TRIGGER(HuntersPetLowHealthTrigger, Trigger)
    END_TRIGGER()

    class HuntersPetUnhappyTrigger : public Trigger
    {
        public:
            HuntersPetUnhappyTrigger(PlayerbotAI* ai) : Trigger(ai, "hunters pet unhappy", 300) {}
        public:
            virtual bool IsActive();
    };

    class BlackArrowTrigger : public DebuffTrigger
    {
        public:
            BlackArrowTrigger(PlayerbotAI* ai) : DebuffTrigger(ai, "black arrow") {}
    };

    class HuntersMarkTrigger : public DebuffTrigger
    {
        public:
            HuntersMarkTrigger(PlayerbotAI* ai) : DebuffTrigger(ai, "hunter's mark") {}
    };

    class FreezingTrapTrigger : public HasCcTargetTrigger
    {
        public:
            FreezingTrapTrigger(PlayerbotAI* ai) : HasCcTargetTrigger(ai, "freezing trap") {}
    };

    class RapidFireTrigger : public BoostTrigger
    {
        public:
            RapidFireTrigger(PlayerbotAI* ai) : BoostTrigger(ai, "rapid fire") {}
    };

    class BestialWrathTrigger : public BoostTrigger
    {
        public:
            BestialWrathTrigger(PlayerbotAI* ai) : BoostTrigger(ai, "bestial wrath") {}
    };

    class TrueshotAuraTrigger : public BuffTrigger
    {
        public:
            TrueshotAuraTrigger(PlayerbotAI* ai) : BuffTrigger(ai, "trueshot aura") {}
    };

    class SerpentStingOnAttackerTrigger : public DebuffOnAttackerTrigger
    {
        public:
            SerpentStingOnAttackerTrigger(PlayerbotAI* ai) : DebuffOnAttackerTrigger(ai, "serpent sting") {}
    };

    class FeignDeathTrigger : public Trigger
    {
        public:
            FeignDeathTrigger(PlayerbotAI* ai) : Trigger(ai, "has feign death", 1) {}

            virtual bool IsActive()
            {
                if (!bot->hasUnitState(UNIT_STAT_DIED))
                {
                    return false;
                }

                if (AI_VALUE(uint8, "attacker count") > 0)
                {
                    return false;
                }

                Unit::AuraList const& auras = bot->GetAurasByType(SPELL_AURA_FEIGN_DEATH);
                if (auras.empty())
                {
                    return false;
                }

                Aura* aura = auras.front();
                int32 maxDuration = aura->GetAuraMaxDuration();
                int32 remaining = aura->GetAuraDuration();

                if (maxDuration > 0)
                {
                    return (maxDuration - remaining) >= 5000;
                }

                return true;
            }
    };
}
