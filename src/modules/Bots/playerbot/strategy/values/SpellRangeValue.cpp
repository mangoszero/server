#include "botpch.h"
#include "../../playerbot.h"
#include "SpellRangeValue.h"
#include "../../PlayerbotAIConfig.h"

using namespace ai;

SpellRangeValue::SpellRangeValue(PlayerbotAI* ai) :
        CalculatedValue<float>(ai, "spell range", sPlayerbotAIConfig.spellDistance)
{
}

float SpellRangeValue::Calculate()
{
    uint32 spellId = AI_VALUE2(uint32, "spell id", qualifier);
    if (spellId)
    {
        const SpellEntry* pSpellInfo = sSpellStore.LookupEntry(spellId);
        if (pSpellInfo)
        {
            SpellRangeEntry const* spellRange = sSpellRangeStore.LookupEntry(pSpellInfo->RangeIndex);
            if (spellRange)
            {
                float actualMaxRange = GetSpellMaxRange(spellRange);
                // Only clamp if spell has a defined range
                if (actualMaxRange > 1 && actualMaxRange < sPlayerbotAIConfig.spellDistance)
                {
                    return actualMaxRange - 1;
                }
            }
        }
    }
    return sPlayerbotAIConfig.spellDistance;
}
