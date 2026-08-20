#include <list>
#include "botpch.h"
#include "../../playerbot.h"
#include "CastTimeStrategy.h"
#include "../actions/GenericSpellActions.h"

using namespace ai;

float CastTimeMultiplier::GetValue(Action* action)
{
    if (action == NULL)
    {
        return 1.0f;
    }

    uint8 targetHealth = AI_VALUE2(uint8, "health", "current target");
    string name = action->getName();

    if (action->GetTarget() != AI_VALUE(Unit*, "current target"))
    {
        return 1.0f;
    }

    if (targetHealth < sPlayerbotAIConfig.lowHealth && dynamic_cast<CastSpellAction*>(action))
    {
        uint32 spellId = AI_VALUE2(uint32, "spell id", name);
        const SpellEntry* const pSpellInfo = sSpellStore.LookupEntry(spellId);

        if (spellId && GetSpellCastTime(pSpellInfo) >= 3000)
        {
            // A multiplier here is a boolean gate, not a ranking. Engine::DoNextAction
            // pops the queue by PUSH-time relevance first, then applies multipliers to a
            // local variable that is only ever tested against zero -- the product is
            // never written back to a basket and never compared against another action's
            // relevance. So any non-zero return runs the action unchanged, and 0.0f skips
            // it for this tick (pushing its alternatives, and letting the same tick fall
            // through to the next-highest basket).
            //
            // Therefore return 0.0f only when the bot genuinely has something better to
            // do against a dying target: inside melee reach, where the "melee in range"
            // fallback can take over -- the predicate below is exactly the one its
            // isUseful gates on (GenericActions.h). Beyond that reach the long nuke must
            // stay, because the old unconditional zero left specs whose only wired action
            // is the nuke -- Elemental, Restoration, a wandless warlock -- stood idle
            // until the mob walked to them.
            if (AI_VALUE2(float, "distance", "current target") <=
                sPlayerbotAIConfig.meleeDistance + sPlayerbotAIConfig.contactDistance +
                bot->GetObjectBoundingRadius())
            {
                return 0.0f;
            }

            return 1.0f;
        }
        else if (spellId && GetSpellCastTime(pSpellInfo) >= 1500)
        {
            // Deliberately inert, and always has been: per the gate note above a
            // non-zero return neither suppresses nor reprioritises anything, so 0.5f
            // behaves exactly like 1.0f. Not promoted to the conditional zero above,
            // because that would NEWLY suppress every 1.5-2.9s cast in melee on a dying
            // target -- behaviour this multiplier has never had. Kept as documentation
            // of the intended tiering, nothing more.
            return 0.5f;
        }
    }

    return 1.0f;
}

void CastTimeStrategy::InitMultipliers(std::list<Multiplier*> &multipliers)
{
    multipliers.push_back(new CastTimeMultiplier(ai));
}
