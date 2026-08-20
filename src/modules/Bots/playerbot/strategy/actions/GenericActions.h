#pragma once

#include "../Action.h"
#include "GenericSpellActions.h"
#include "ReachTargetActions.h"
#include "ChooseTargetActions.h"
#include "MovementActions.h"

namespace ai
{
    class MeleeAction : public AttackAction
    {
        public:
            MeleeAction(PlayerbotAI* ai) : AttackAction(ai, "melee") {}

            virtual string GetTargetName()
            {
                return "current target";
            }
    };

    // Melee that only offers itself when the target is already in reach.
    //
    // Plain MeleeAction is deliberately ungated and stays that way -- for a warrior, rogue or
    // paladin, swinging is always the right terminal fallback. For a CASTER it is not. Attack()
    // does not move a player bot (chasing is creature-only in Unit::Attack), it returns true,
    // which ENDS the tick, and it flips the bot into the combat engine. So an out-of-mana priest
    // with a mob thirty yards away would "successfully" stand there auto-attacking something it
    // cannot reach -- and the combat engine has no drink action, so it would not sit down and
    // recover either. Gating on reach lets that bot fall through to flee or drink instead.
    //
    // The predicate is the exact complement of ReachMeleeAction::isUseful, so "must close" and
    // "close enough" cannot disagree.
    class MeleeInRangeAction : public AttackAction
    {
        public:
            MeleeInRangeAction(PlayerbotAI* ai) : AttackAction(ai, "melee in range") {}

            virtual string GetTargetName()
            {
                return "current target";
            }

            virtual bool isUseful()
            {
                if (!AttackAction::isUseful())
                {
                    return false;
                }

                return AI_VALUE2(float, "distance", "current target") <=
                    sPlayerbotAIConfig.meleeDistance + sPlayerbotAIConfig.contactDistance + bot->GetObjectBoundingRadius();
            }
    };
}
