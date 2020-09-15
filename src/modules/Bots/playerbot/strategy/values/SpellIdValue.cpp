#include "botpch.h"
#include "../../playerbot.h"
#include "SpellIdValue.h"
#include "../../PlayerbotAIConfig.h"

using namespace ai;

SpellIdValue::SpellIdValue(PlayerbotAI* ai) :
        CalculatedValue<uint32>(ai, "spell id", 5)
{
}

uint32 SpellIdValue::Calculate()
{
    string namepart = qualifier;
    wstring wnamepart;

    if (!Utf8toWStr(namepart, wnamepart))
    {
        return 0;
    }

    wstrToLower(wnamepart);
    char firstSymbol = tolower(qualifier[0]);
    int spellLength = wnamepart.length();

    int loc = bot->GetSession()->GetSessionDbcLocale();

    uint32 foundSpellId = 0;
    bool foundMatchUsesNoReagents = false;

    for (PlayerSpellMap::iterator itr = bot->GetSpellMap().begin(); itr != bot->GetSpellMap().end(); ++itr)
    {
        uint32 spellId = itr->first;

        if (itr->second.state == PLAYERSPELL_REMOVED || itr->second.disabled || IsPassiveSpell(spellId))
        {
            continue;
        }

        const SpellEntry* pSpellInfo = sSpellStore.LookupEntry(spellId);
        if (!pSpellInfo)
        {
            continue;
        }

        if (pSpellInfo->Effect[0] == SPELL_EFFECT_LEARN_SPELL)
        {
            continue;
        }

        char* spellName = pSpellInfo->SpellName[loc];
        if (tolower(spellName[0]) != firstSymbol || strlen(spellName) != spellLength || !Utf8FitTo(spellName, wnamepart))
        {
            continue;
        }

        bool usesNoReagents = (pSpellInfo->Reagent[0] <= 0);

        // if we already found a spell
        bool useThisSpell = true;
        if (foundSpellId > 0) {
            if (usesNoReagents && !foundMatchUsesNoReagents)
            {

            }
            else if (spellId > foundSpellId)
            {

            }
            else
            {
                useThisSpell = false;
            }
        }
        if (useThisSpell) {
        {
            foundSpellId = spellId;
        }
            foundMatchUsesNoReagents = usesNoReagents;
        }
    }

    Pet* pet = bot->GetPet();
    if (!foundSpellId && pet)
    {
        for (PetSpellMap::const_iterator itr = pet->m_spells.begin(); itr != pet->m_spells.end(); ++itr)
        {
            if(itr->second.state == PETSPELL_REMOVED)
            {
                continue;
            }

            uint32 spellId = itr->first;
            const SpellEntry* pSpellInfo = sSpellStore.LookupEntry(spellId);
            if (!pSpellInfo)
            {
                continue;
            }

            if (pSpellInfo->Effect[0] == SPELL_EFFECT_LEARN_SPELL)
            {
                continue;
            }

            char* spellName = pSpellInfo->SpellName[loc];
            if (tolower(spellName[0]) != firstSymbol || strlen(spellName) != spellLength || !Utf8FitTo(spellName, wnamepart))
            {
                continue;
            }

            foundSpellId = spellId;
        }
    }

    return foundSpellId;
}
