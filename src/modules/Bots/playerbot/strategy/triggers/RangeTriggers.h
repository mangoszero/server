#pragma once
#include "../Trigger.h"
#include "../../PlayerbotAIConfig.h"

namespace ai
{
    class EnemyTooCloseForSpellTrigger : public Trigger {
        public:
            EnemyTooCloseForSpellTrigger(PlayerbotAI* ai) : Trigger(ai, "enemy too close for spell"), lastNoAggroFlee_(0) {}
            virtual bool IsActive()
            {
                Unit* target = AI_VALUE(Unit*, "current target");
                if (!target || AI_VALUE2(float, "distance", "current target") > sPlayerbotAIConfig.spellDistance / 2)
                {
                    return false;
                }

                if (AI_VALUE2(bool, "has aggro", "current target"))
                {
                    lastNoAggroFlee_ = 0;
                    return true;
                }

                // No aggro: apply cooldown.
                time_t now = time(nullptr);
                if (lastNoAggroFlee_ == 0 ||
                   (now - lastNoAggroFlee_) >= NO_AGGRO_FLEE_COOLDOWN)
                {
                    lastNoAggroFlee_ = now;
                    return true;
                }
                if(now - lastNoAggroFlee_ < NO_AGGRO_FLEE_ACTIVE)
                {
                    return true;
                }
                return false;
            }
        private:
            time_t lastNoAggroFlee_;
            static const time_t NO_AGGRO_FLEE_ACTIVE = 5;
            static const time_t NO_AGGRO_FLEE_COOLDOWN = 11;
    };

    class EnemyTooCloseForSpellNoAggroTrigger : public Trigger {
        public:
            EnemyTooCloseForSpellNoAggroTrigger(PlayerbotAI* ai) : Trigger(ai, "enemy too close for spell no aggro") {}
            virtual bool IsActive()
            {
                Unit* target = AI_VALUE(Unit*, "current target");
                if (!target || AI_VALUE2(float, "distance", "current target") > sPlayerbotAIConfig.tooCloseDistance)
                {
                    return false;
                }

                return AI_VALUE(uint8, "my attacker count") == 0;
            }
    };

    class EnemyTooCloseForMeleeTrigger : public Trigger {
        public:
            EnemyTooCloseForMeleeTrigger(PlayerbotAI* ai) : Trigger(ai, "enemy too close for melee", 5) {}
            virtual bool IsActive()
            {
                Unit* target = AI_VALUE(Unit*, "current target");
                return target && AI_VALUE2(float, "distance", "current target") <= sPlayerbotAIConfig.contactDistance;
            }
    };

    class OutOfRangeTrigger : public Trigger {
        public:
            OutOfRangeTrigger(PlayerbotAI* ai, string name, float distance) : Trigger(ai, name)
            {
                this->distance = distance;
            }
            virtual bool IsActive()
            {
                Unit* target = AI_VALUE(Unit*, GetTargetName());
                return target && AI_VALUE2(float, "distance", GetTargetName()) > distance;
            }
            virtual string GetTargetName()
            {
                return "current target";
            }

        protected:
            float distance;
    };

    class EnemyOutOfMeleeTrigger : public OutOfRangeTrigger
    {
        public:
            EnemyOutOfMeleeTrigger(PlayerbotAI* ai) : OutOfRangeTrigger(ai, "enemy out of melee range", sPlayerbotAIConfig.meleeDistance) {}
    };

    class EnemyOutOfSpellRangeTrigger : public OutOfRangeTrigger
    {
        public:
            EnemyOutOfSpellRangeTrigger(PlayerbotAI* ai) : OutOfRangeTrigger(ai, "enemy out of spell range", sPlayerbotAIConfig.spellDistance) {}
    };

    class PartyMemberToHealOutOfSpellRangeTrigger : public OutOfRangeTrigger
    {
        public:
            PartyMemberToHealOutOfSpellRangeTrigger(PlayerbotAI* ai) : OutOfRangeTrigger(ai, "party member to heal out of spell range", sPlayerbotAIConfig.spellDistance) {}
            virtual string GetTargetName()
            {
                return "party member to heal";
            }
    };

    class FarFromMasterTrigger : public Trigger {
        public:
            FarFromMasterTrigger(PlayerbotAI* ai, string name = "far from master", float distance = 12.0f, int checkInterval = 1) : Trigger(ai, name, checkInterval), distance(distance) {}

            virtual bool IsActive()
            {
                return AI_VALUE2(float, "distance", "master target") > distance;
            }

        private:
            float distance;
    };

    class OutOfReactRangeTrigger : public FarFromMasterTrigger
    {
        public:
            OutOfReactRangeTrigger(PlayerbotAI* ai) : FarFromMasterTrigger(ai, "out of react range", sPlayerbotAIConfig.reactDistance / 2, 10) {}
    };
}
