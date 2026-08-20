#pragma once

#include <cstdint>

namespace ai
{
    enum OutOfRangeMover
    {
        OUT_OF_RANGE_MOVER_NONE,        ///< follow the ActionNode alternatives
        OUT_OF_RANGE_MOVER_REACH_SPELL, ///< push "reach spell::<spell id>"
        OUT_OF_RANGE_MOVER_REACH_MELEE  ///< push "reach melee"
    };

    // Decision core of CastSpellAction::getImpossiblePrerequisites, kept free of
    // engine and world state so the test suite can pin the table directly.
    //
    // The rules, and why each one is load-bearing:
    //
    // - An unresolved spell (id 0) MUST return NONE. Many action names in this
    //   module are intent labels heading an ActionNode cascade ("mage armor" steps
    //   down to Frost Armor, "judgement of wisdom" to the real Judgement); pushing
    //   a mover instead of following the alternatives severs the cascade. That is
    //   the regression documented at length in GenericTriggers.cpp -- do not
    //   weaken this rule.
    // - Only plain current-target actions move. Anything with an overridden
    //   target value (enemy healer, party member, self) keeps the old behaviour.
    // - distance <= range means the action is impossible for a non-range reason
    //   (mana, cooldown, shapeshift); moving cannot fix those. The converse is not
    //   provable from here: a bot that does close in may still fail for one of
    //   those reasons, because Spell::CheckCast tests range before power and
    //   caster auras. That residue is accepted -- the failed cast just falls
    //   through to its alternatives on the next evaluation.
    // - A ranged policy walks to the spell's own range; a melee policy (range
    //   pinned to ATTACK_DISTANCE) reuses the ordinary "reach melee" mover.
    inline OutOfRangeMover ChooseOutOfRangeMover(uint32_t spellId, bool targetIsCurrentTarget,
                                                 float distance, float range, float attackDistance)
    {
        if (!spellId)
        {
            return OUT_OF_RANGE_MOVER_NONE;
        }
        if (!targetIsCurrentTarget)
        {
            return OUT_OF_RANGE_MOVER_NONE;
        }
        if (distance <= range)
        {
            return OUT_OF_RANGE_MOVER_NONE;
        }
        if (range > attackDistance)
        {
            return OUT_OF_RANGE_MOVER_REACH_SPELL;
        }
        return OUT_OF_RANGE_MOVER_REACH_MELEE;
    }
}
