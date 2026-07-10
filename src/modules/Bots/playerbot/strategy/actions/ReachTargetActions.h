#pragma once

#include "../Action.h"
#include "MovementActions.h"
#include "GenericSpellActions.h"
#include "../../PlayerbotAIConfig.h"

namespace ai
{
    class ReachTargetAction : public MovementAction
    {
        public:
            ReachTargetAction(PlayerbotAI* ai, string name, float distance) : MovementAction(ai, name)
            {
                this->distance = distance;
            }
            virtual bool Execute(Event event)
            {
                return MoveTo(AI_VALUE(Unit*, "current target"), distance);
            }
            virtual bool isUseful()
            {
                return AI_VALUE2(float, "distance", "current target") > distance;
            }

            virtual string GetTargetName()
            {
                return "current target";
            }

        protected:
            float distance;
    };

    class CastReachTargetSpellAction : public CastSpellAction
    {
        public:
            CastReachTargetSpellAction(PlayerbotAI* ai, string spell, float distance) : CastSpellAction(ai, spell)
            {
                this->distance = distance;
            }
            virtual bool isUseful()
            {
                return AI_VALUE2(float, "distance", "current target") > distance;
            }

        protected:
            float distance;
    };

    class ReachMeleeAction : public ReachTargetAction
    {
        public:
            ReachMeleeAction(PlayerbotAI* ai) : ReachTargetAction(ai, "reach melee", sPlayerbotAIConfig.meleeDistance) {}

            virtual bool isUseful()
            {
                return AI_VALUE2(float, "distance", "current target") >
                    distance + sPlayerbotAIConfig.contactDistance + bot->GetObjectBoundingRadius();
            }
    };

    class ReachSapAction : public MovementAction
    {
        public:
            ReachSapAction(PlayerbotAI* ai) : MovementAction(ai, "reach sap") {}

            virtual bool Execute(Event event)
            {
                Unit* target = AI_VALUE(Unit*, "current target");
                if (!target)
                {
                    return false;
                }

                float tx = target->GetPositionX();
                float ty = target->GetPositionY();
                float tz = target->GetPositionZ();
                float ori = target->GetOrientation();

                float behindX = tx - cos(ori) * 4.0f;
                float behindY = ty - sin(ori) * 4.0f;

                float distBehind = bot->GetDistance(behindX, behindY, tz);
                if (distBehind <= sPlayerbotAIConfig.contactDistance)
                {
                    return false;
                }

                float bx = bot->GetPositionX();
                float by = bot->GetPositionY();
                float dx = bx - tx;
                float dy = by - ty;
                float dot = dx * cos(ori) + dy * sin(ori);

                if (dot > 0.0f)
                {
                    float cross = dx * sin(ori) - dy * cos(ori);
                    float flankAngle = ori + (cross > 0.0f ? M_PI / 2.0f : -M_PI / 2.0f);
                    float flankX = tx + cos(flankAngle) * 4.0f;
                    float flankY = ty + sin(flankAngle) * 4.0f;

                    float distFlank = bot->GetDistance(flankX, flankY, tz);
                    if (distFlank < sPlayerbotAIConfig.contactDistance * 3.0f)
                    {
                        return MoveTo(target->GetMapId(), behindX, behindY, tz, true);
                    }
                    return MoveTo(target->GetMapId(), flankX, flankY, tz, true);
                }
                else
                {
                    return MoveTo(target->GetMapId(), behindX, behindY, tz, true);
                }
            }

            virtual bool isUseful()
            {
                Unit* target = AI_VALUE(Unit*, "current target");
                if (!target || !target->IsAlive() || !ai->HasAura("stealth", bot))
                {
                    return false;
                }

                float tx = target->GetPositionX();
                float ty = target->GetPositionY();
                float tz = target->GetPositionZ();
                float ori = target->GetOrientation();

                float behindX = tx - cos(ori) * 4.0f;
                float behindY = ty - sin(ori) * 4.0f;

                return bot->GetDistance(behindX, behindY, tz) > sPlayerbotAIConfig.contactDistance;
            }
    };

    class BackOffAction : public ReachTargetAction
    {
        public:
            BackOffAction(PlayerbotAI* ai) : ReachTargetAction(ai, "back off", sPlayerbotAIConfig.meleeDistance) {}

            virtual bool isUseful()
            {
                return AI_VALUE2(float, "distance", "current target") <
                        distance + sPlayerbotAIConfig.contactDistance + bot->GetObjectBoundingRadius();
            }
    };

    class ReachSpellAction : public ReachTargetAction
    {
        public:
            ReachSpellAction(PlayerbotAI* ai) : ReachTargetAction(ai, "reach spell", sPlayerbotAIConfig.spellDistance) {}
    };

    class ReachShootRangeAction : public ReachTargetAction
    {
        public:
            ReachShootRangeAction(PlayerbotAI* ai) : ReachTargetAction(ai, "reach shoot range", sPlayerbotAIConfig.spellDistance)
            {
                string spell = GetShootSpell(bot);
                if (spell.length() > 0)
                {
                    distance = AI_VALUE2(float, "spell range", spell);
                }
            }
            virtual bool isUseful();
            virtual void Reset();
            static string GetShootSpell(Player *bot);
    };
}
