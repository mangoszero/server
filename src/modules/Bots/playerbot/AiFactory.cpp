#include "../botpch.h"
#include "playerbot.h"
#include "AiFactory.h"
#include "strategy/Engine.h"

#include "strategy/priest/PriestAiObjectContext.h"
#include "strategy/mage/MageAiObjectContext.h"
#include "strategy/warlock/WarlockAiObjectContext.h"
#include "strategy/warrior/WarriorAiObjectContext.h"
#include "strategy/shaman/ShamanAiObjectContext.h"
#include "strategy/paladin/PaladinAiObjectContext.h"
#include "strategy/druid/DruidAiObjectContext.h"
#include "strategy/hunter/HunterAiObjectContext.h"
#include "strategy/rogue/RogueAiObjectContext.h"
#include "Player.h"
#include "PlayerbotAIConfig.h"
#include "RandomPlayerbotMgr.h"


AiObjectContext* AiFactory::createAiObjectContext(Player* player, PlayerbotAI* ai)
{
    switch (player->getClass())
    {
    case CLASS_PRIEST:
        return new PriestAiObjectContext(ai);
        break;
    case CLASS_MAGE:
        return new MageAiObjectContext(ai);
        break;
    case CLASS_WARLOCK:
        return new WarlockAiObjectContext(ai);
        break;
    case CLASS_WARRIOR:
        return new WarriorAiObjectContext(ai);
        break;
    case CLASS_SHAMAN:
        return new ShamanAiObjectContext(ai);
        break;
    case CLASS_PALADIN:
        return new PaladinAiObjectContext(ai);
        break;
    case CLASS_DRUID:
        return new DruidAiObjectContext(ai);
        break;
    case CLASS_HUNTER:
        return new HunterAiObjectContext(ai);
        break;
    case CLASS_ROGUE:
        return new RogueAiObjectContext(ai);
        break;
    }
    return new AiObjectContext(ai);
}

int AiFactory::GetPlayerSpecTab(Player* bot)
{
    map<uint32, int32> tabs = GetPlayerSpecTabs(bot);

    int tab = -1, max = 0;
    for (uint32 i = 0; i < uint32(3); i++)
    {
        if (tab == -1 || max < tabs[i])
        {
            tab = i;
            max = tabs[i];
        }
    }

    return tab;
}

map<uint32, int32> AiFactory::GetPlayerSpecTabs(Player* bot)
{
    map<uint32, int32> tabs;
    for (uint32 i = 0; i < uint32(3); i++)
    {
        tabs[i] = 0;
    }

    uint32 classMask = bot->getClassMask();
    for (uint32 i = 0; i < sTalentStore.GetNumRows() && bot->GetFreeTalentPoints(); ++i)
    {
        TalentEntry const *talentInfo = sTalentStore.LookupEntry(i);
        if (!talentInfo)
        {
            continue;
        }

        TalentTabEntry const *talentTabInfo = sTalentTabStore.LookupEntry(talentInfo->TalentTab);
        if (!talentTabInfo)
        {
            continue;
        }

        if ((classMask & talentTabInfo->ClassMask) == 0)
        {
            continue;
        }

        for (int rank = MAX_TALENT_RANK - 1; rank >= 0; --rank)
        {
            if (!talentInfo->RankID[rank])
            {
                continue;
            }

            uint32 spellid = talentInfo->RankID[rank];
            if (spellid && bot->HasSpell(spellid))
            {
                tabs[talentTabInfo->tabpage]++;
            }

        }
    }

    return tabs;
}

void AiFactory::AddDefaultCombatStrategies(Player* player, PlayerbotAI* const facade, Engine* engine)
{
    int tab = GetPlayerSpecTab(player);

    engine->addStrategies("attack weak", "racials", "chat", "default", "aoe", "potions", "cast time", "conserve mana", "duel", "pvp", NULL);

    if (sPlayerbotAIConfig.cautiousDefault)
    {
        engine->addStrategy("cautious");
    }

    switch (player->getClass())
    {
        case CLASS_PRIEST:
            if (tab == 2)
            {
                engine->addStrategies("dps", "threat", NULL);
                if (player->getLevel() > 19)
                {
                    engine->addStrategy("dps debuff");
                }
            }
            else
            {
                engine->addStrategy("heal");
            }

            engine->addStrategy("flee");
            break;
        case CLASS_MAGE:
            if (tab == 0)
            {
                engine->addStrategies("arcane", "threat", NULL);
            }
            else if (tab == 1)
            {
                engine->addStrategies("fire", "fire aoe", "threat", NULL);
            }
            else
            {
                engine->addStrategies("frost", "frost aoe", "threat", NULL);
            }

            engine->addStrategy("flee");
            break;
        case CLASS_WARRIOR:
            if (tab == 2)
            {
                engine->addStrategies("tank", "tank aoe", NULL);
            }
            else
            {
                engine->addStrategies("dps", "threat", NULL);
            }
            break;
        case CLASS_SHAMAN:
            if (tab == 0)
            {
                engine->addStrategies("caster", "caster aoe", "bmana", "threat", "flee", NULL);
            }
            else if (tab == 2)
            {
                engine->addStrategies("heal", "bmana", "flee", NULL);
            }
            else
            {
                engine->addStrategies("dps", "melee aoe", "bdps", "threat", NULL);
            }
            break;
        case CLASS_PALADIN:
            if (tab == 1)
            {
                engine->addStrategies("tank", "tank aoe", "barmor", NULL);
            }
            else
            {
                engine->addStrategies("dps", "bdps", "threat", NULL);
            }
            break;
        case CLASS_DRUID:
            if (tab == 0)
            {
                engine->addStrategies("caster", "caster aoe", "threat", "flee", NULL);
                if (player->getLevel() > 19)
                {
                    engine->addStrategy("caster debuff");
                }
            }
            else if (tab == 2)
            {
                engine->addStrategies("heal", "flee", NULL);
            }
            else
            {
                engine->addStrategies("bear", "tank aoe", "threat", "flee", NULL);
            }
            break;
        case CLASS_HUNTER:
            engine->addStrategies("dps", "bdps", "threat", NULL);
            if (player->getLevel() > 19)
            {
                engine->addStrategy("dps debuff");
            }
            break;
        case CLASS_ROGUE:
            engine->addStrategies("dps", "threat", NULL);
            break;
        case CLASS_WARLOCK:
            if (tab == 1)
            {
                engine->addStrategies("tank", "threat", NULL);
            }
            else
            {
                engine->addStrategies("dps", "threat", NULL);
            }

            if (player->getLevel() > 19)
            {
                engine->addStrategy("dps debuff");
            }

            engine->addStrategy("flee");
            break;
    }

    if (sRandomPlayerbotMgr.IsRandomBot(player) && !player->GetGroup())
    {
        engine->ChangeStrategy(sPlayerbotAIConfig.randomBotCombatStrategies);
    }
}

Engine* AiFactory::createCombatEngine(Player* player, PlayerbotAI* const facade, AiObjectContext* AiObjectContext) {
    Engine* engine = new Engine(facade, AiObjectContext);
    AddDefaultCombatStrategies(player, facade, engine);
    return engine;
}

void AiFactory::AddDefaultNonCombatStrategies(Player* player, PlayerbotAI* const facade, Engine* nonCombatEngine)
{
    int tab = GetPlayerSpecTab(player);

    if (sPlayerbotAIConfig.cautiousDefault)
    {
        nonCombatEngine->addStrategy("cautious");
    }

    switch (player->getClass()){
        case CLASS_PALADIN:
        case CLASS_HUNTER:
        case CLASS_SHAMAN:
            nonCombatEngine->addStrategy("bmana");
            break;
        case CLASS_MAGE:
            if (tab == 1)
            {
                nonCombatEngine->addStrategy("bdps");
            }
            else
            {
                nonCombatEngine->addStrategy("bmana");
            }
            break;
    }
    nonCombatEngine->addStrategies("nc", "attack weak", "food", "stay", "chat",
            "default", "quest", "loot", "gather", "duel", "emote", NULL);

    if (sRandomPlayerbotMgr.IsRandomBot(player) && !player->GetGroup())
    {
        nonCombatEngine->ChangeStrategy(sPlayerbotAIConfig.randomBotNonCombatStrategies);
    }
}

Engine* AiFactory::createNonCombatEngine(Player* player, PlayerbotAI* const facade, AiObjectContext* AiObjectContext) {
    Engine* nonCombatEngine = new Engine(facade, AiObjectContext);

    AddDefaultNonCombatStrategies(player, facade, nonCombatEngine);
    return nonCombatEngine;
}

void AiFactory::AddDefaultDeadStrategies(Player* player, PlayerbotAI* const facade, Engine* deadEngine)
{
    deadEngine->addStrategies("dead", "stay", "chat", "default", "follow master", NULL);
    if (sRandomPlayerbotMgr.IsRandomBot(player) && !player->GetGroup())
    {
        deadEngine->removeStrategy("follow master");
    }
}

Engine* AiFactory::createDeadEngine(Player* player, PlayerbotAI* const facade, AiObjectContext* AiObjectContext) {
    Engine* deadEngine = new Engine(facade, AiObjectContext);
    AddDefaultDeadStrategies(player, facade, deadEngine);
    return deadEngine;
}
