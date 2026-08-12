#include <string>
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

// Has this bot actually chosen a specialisation yet?
//
// GetPlayerSpecTabs seeds all three tab keys at zero, so a character with nothing spent
// produces a three-way tie that GetPlayerSpecTab resolves to whichever key sorts first.
// That is not a spec; it is an artefact of the seeding. Every class below level 10 has one,
// and the branch it lands in decides which abilities the bot will try to use for its whole
// early life -- which is how untalented druids were sent to Bear before Bear Form exists.
bool AiFactory::HasChosenSpec(Player* bot)
{
    // Ask the core how many points this bot should have rather than deriving it from level.
    // A hand-rolled (level - 9) assumes Rate.Talent is 1, which is the stock and the live
    // value but is configurable; at any other rate the subtraction misclassifies, and below
    // level 10 it underflows an unsigned.
    return bot->CalculateTalentsPoints() > bot->GetFreeTalentPoints();
}

int AiFactory::GetPlayerSpecTab(Player* bot)
{
    map<uint32, int32> tabs = GetPlayerSpecTabs(bot);

    int bestId = -1, max = 0;
    for (auto const& pair : tabs)
    {
        if (bestId == -1 || max < pair.second)
        {
            bestId = pair.first;
            max = pair.second;
        }
    }

    if (bestId == -1)
    {
        return -1;
    }

    return TalentTabToIndex(bot->getClass(), (uint32)bestId);
}

int AiFactory::TalentTabToIndex(uint8 cls, uint32 talentTabId)
{
    switch (cls)
    {
        case CLASS_MAGE:
            // The shipped 1.12 TalentTab.dbc gives Fire (41) OrderIndex 0, the same as
            // Arcane (81), so the data cannot separate them and the mapping is done by
            // hand: Arcane(81)=0, Fire(41)=1, Frost(61)=2.
            if (talentTabId == 41)
            {
                return 1;
            }
            if (talentTabId == 61)
            {
                return 2;
            }
            return 0;
        default:
        {
            TalentTabEntry const* tabEntry = sTalentTabStore.LookupEntry(talentTabId);
            if (tabEntry)
            {
                return (int)tabEntry->OrderIndex;
            }
            return -1;
        }
    }
}

map<uint32, int32> AiFactory::GetPlayerSpecTabs(Player* bot)
{
    map<uint32, int32> tabs;
    for (uint32 i = 0; i < uint32(3); i++)
    {
        tabs[i] = 0;
    }

    uint32 classMask = bot->getClassMask();
    uint32 spentPoints = bot->getLevel() >= 10 ? (bot->getLevel() - 9) - bot->GetFreeTalentPoints() : 0;
    uint32 found = 0;

    for (uint32 i = 0; i < sTalentStore.GetNumRows(); ++i)
    {
        if (found >= spentPoints)
        {
            break;
        }

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
                // Keyed by TalentTab ID, not OrderIndex, and deliberately so: the shipped
                // 1.12 TalentTab.dbc gives mage Fire (41) the same OrderIndex 0 as Arcane
                // (81), so keying by OrderIndex would merge two specs into one bucket.
                // GetPlayerSpecTab converts the winning ID to a 0/1/2 index and fixes the
                // mage row up by hand. The seeded keys 0/1/2 below are therefore never
                // incremented -- which is a display trap, not a behaviour one; see
                // ChatHelper::formatClass.
                tabs[talentTabInfo->ID]++;
                found++;
            }
        }
    }

    return tabs;
}

bool AiFactory::IsFeralCatSpec(Player* bot)
{
    if (GetPlayerSpecTab(bot) != 1)
    {
        return false;
    }

    int catPoints = 0;
    int bearPoints = 0;
    uint32 classMask = bot->getClassMask();

    for (uint32 i = 0; i < sTalentStore.GetNumRows(); ++i)
    {
        TalentEntry const *talentInfo = sTalentStore.LookupEntry(i);
        if (!talentInfo)
        {
            continue;
        }

        TalentTabEntry const *talentTabInfo = sTalentTabStore.LookupEntry(talentInfo->TalentTab);
        if ((!talentTabInfo) || ((classMask & talentTabInfo->ClassMask) == 0) || (talentTabInfo->OrderIndex != 1))
        {
            continue;
        }

        for (int rank = 0; rank < MAX_TALENT_RANK; ++rank)
        {
            uint32 spellid = talentInfo->RankID[rank];
            if (spellid && bot->HasSpell(spellid))
            {
                SpellEntry const *spellInfo = sSpellStore.LookupEntry(spellid);
                if (!spellInfo || !spellInfo->Name_lang[0])
                {
                    continue;
                }
                std::string name = spellInfo->Name_lang[0];
                if (name == "Feline Swiftness")           catPoints++;
                else if (name == "Thick Hide")            bearPoints++;
                else if (name == "Feral Charge")          bearPoints++;
                else if (name == "Improved Shred")        catPoints++;
                else if (name == "Blood Frenzy")          catPoints++;
                else if (name == "Primal Fury")           bearPoints++;
                break;
            }
        }
    }
    // Default to bear for low-level druids with no deep spec
    if (catPoints == 0 && bearPoints == 0)
    {
        return false;
    }

    return catPoints > bearPoints;
}

void AiFactory::AddDefaultCombatStrategies(Player* player, PlayerbotAI* const facade, Engine* engine)
{
    int tab = GetPlayerSpecTab(player);

    engine->addStrategies("attack weak", "racials", "chat", "default", "potions", "cast time", "conserve mana", "duel", "pvp", NULL);

    // "aoe" is NOT generic. Only four classes register it -- hunter, priest (as shadow_aoe),
    // warlock and warrior -- while the rest either have a spec-specific variant added in the
    // switch below ("fire aoe", "frost aoe", "cat aoe", "caster aoe", "melee aoe", "tank aoe")
    // or none at all. Adding it unconditionally therefore did nothing for five of the nine
    // classes except emit an unresolved-strategy error for every one of those bots on every
    // ResetStrategies -- around 150 lines per startup, which buries the real unresolved names
    // this diagnostic exists to surface. Add it where it exists instead.
    switch (player->getClass())
    {
        case CLASS_HUNTER:
        case CLASS_PRIEST:
        case CLASS_WARLOCK:
        case CLASS_WARRIOR:
            engine->addStrategy("aoe");
            break;
        default:
            break;
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
            else if (!HasChosenSpec(player))
            {
                // An untalented shaman is NOT an Enhancement shaman. It lands here only
                // because the seeded tab tie resolves to this branch, and Enhancement's
                // defining ability is Stormstrike -- a level 40 thirty-one-point talent.
                // Meanwhile every shaman starts with Lightning Bolt (403), so the caster
                // branch is the one it can actually play from level 1. Same strategies as
                // the Elemental branch above.
                engine->addStrategies("caster", "caster aoe", "bmana", "threat", "flee", NULL);
            }
            else
            {
                // "flee" to match the other two shaman branches, which both have it. An
                // Enhancement shaman had no escape at low health and simply fought until
                // it died.
                engine->addStrategies("dps", "melee aoe", "bdps", "threat", "flee", NULL);
            }
            break;
        case CLASS_PALADIN:
            if (tab == 1)
            {
                engine->addStrategies("tank", "tank aoe", "barmor", NULL);
            }
            else if (tab == 0)
            {
                // Holy. There was no branch for it and no paladin heal strategy to send it
                // to, so every Holy build fell through to dps -- about a fifth of paladin
                // bots by the shipped spec probabilities, running a damage rotation on a
                // healing build, and a standing shortfall of healers in any bot group.
                engine->addStrategies("heal", "bmana", NULL);
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
                // Gate each form on the bot actually HAVING it, not on the talent weighting
                // that asked for it. Cat Form (768) and Claw (1082) are level 20 and Bear
                // Form (5487) is level 10, but feral weighting can select Cat from level 10
                // and an untalented druid lands in this branch from level 1. Both produced a
                // druid running a form rotation in caster shape: no form, no form attacks,
                // and the caster spells it did own outranked by a strategy built around
                // shapeshifting. Falling back through Bear to caster keeps it playing the
                // character it actually is until the forms exist.
                if (IsFeralCatSpec(player) && player->HasSpell(768) && player->HasSpell(1082))
                {
                    engine->addStrategies("cat", "cat aoe", "threat", "flee", NULL);
                    if (player->getLevel() > 19)
                    {
                        engine->addStrategy("dps debuff");
                    }
                }
                else if (player->HasSpell(5487))
                {
                    engine->addStrategies("bear", "tank aoe", "threat", "flee", NULL);
                }
                else
                {
                    engine->addStrategies("caster", "caster aoe", "threat", "flee", NULL);
                }
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
            // Demonology used to map to the tank strategy. In 1.12 that tree makes the
            // PET durable, not the warlock, so a third of warlocks by the configured
            // probabilities were running a tank rotation they cannot perform. Every
            // warlock spec is a damage build here.
            engine->addStrategies("dps", "threat", NULL);

            if (player->getLevel() > 19)
            {
                engine->addStrategy("dps debuff");
            }

            engine->addStrategy("flee");
            break;
    }

    if (player->GetGroup())
    {
        if (engine->ContainsStrategy(STRATEGY_TYPE_TANK))
        {
            engine->ChangeStrategy(sPlayerbotAIConfig.botTankStrategies);
        }
        else if (engine->ContainsStrategy(STRATEGY_TYPE_HEAL))
        {
            engine->ChangeStrategy(sPlayerbotAIConfig.botHealStrategies);
        }
        else
        {
            engine->ChangeStrategy(sPlayerbotAIConfig.botDpsStrategies);
        }
    }
    else if (sRandomPlayerbotMgr.IsRandomBot(player))
    {
        // randomBotCombatStrategies defaults to "+dps,+attack weak", and dps is a sibling
        // of the tank and heal strategies chosen just above, so adding it evicted whatever
        // the talents had selected. That made the whole RandomClassSpecProbability roll
        // decorative for random bots: a Protection warrior or a Restoration shaman was
        // given its spec strategy and then had it taken away again a few lines later,
        // leaving every random bot a damage build whatever its talents said. Mages were
        // the accidental exception, having no dps sibling to be replaced by.
        //
        // Apply it only where it agrees with the build already selected. A tank or a
        // healer keeps what GetPlayerSpecTab picked for it, which is the same rule the
        // player-owned branch above follows.
        // Ranged is in that list for a reason the first version of this missed. The name
        // "dps" is resolved per class, and for a druid it creates CatDpsDruidStrategy and
        // for a shaman MeleeShamanStrategy -- both melee. So a Balance druid or an
        // Elemental shaman, having correctly been given its caster strategy above, then had
        // a feral or melee one bolted on beside it. Skipping any build that already chose a
        // ranged strategy leaves casters alone; a melee dps build still takes the config,
        // where "dps" resolves to what it already has and costs nothing.
        //
        // Healers are deliberately NOT excluded, which the first version got wrong in the
        // other direction. This branch is only reached when the bot has no group, and a
        // healer alone has nothing to heal: a level 1 Discipline priest was given "heal"
        // and "flee" and literally nothing else, so it stood beside its target and never
        // acted. Every non-shadow priest and every untalented one lands there, because
        // GetPlayerSpecTab answers 0 when no points are spent. Solo, a damage build is the
        // only useful build. Nothing is lost by it either: the grouped branch above hands a
        // HEAL build botHealStrategies, and AcceptInvitationAction calls ResetStrategies, so
        // joining a party rebuilds the healer it was meant to be.
        // Skipping the whole ChangeStrategy was too blunt. The setting is a list, and only
        // the entries that resolve per class to a damage build are the problem; an operator
        // who adds anything else to randomBotCombatStrategies -- and "+attack weak" is in
        // the shipped default -- lost it entirely on every tank and every caster. Drop only
        // the offending entry for those builds and apply the rest.
        bool keepSpec = engine->ContainsStrategy(STRATEGY_TYPE_TANK) ||
                        engine->ContainsStrategy(STRATEGY_TYPE_RANGED);

        if (!keepSpec)
        {
            engine->ChangeStrategy(sPlayerbotAIConfig.randomBotCombatStrategies);
        }
        else
        {
            vector<string> parts = split(sPlayerbotAIConfig.randomBotCombatStrategies, ',');
            for (vector<string>::iterator i = parts.begin(); i != parts.end(); ++i)
            {
                string entry = *i;
                entry.erase(0, entry.find_first_not_of(" \t"));
                size_t last = entry.find_last_not_of(" \t");
                if (last != string::npos)
                {
                    entry.erase(last + 1);
                }

                if (entry.empty() || entry == "+dps" || entry == "dps")
                {
                    continue;
                }

                engine->ChangeStrategy(entry);
            }
        }
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

    switch (player->getClass())
    {
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

    if (player->GetGroup())
    {
        nonCombatEngine->ChangeStrategy(sPlayerbotAIConfig.botGroupNonCombatStrategies);
    }
    else if (sRandomPlayerbotMgr.IsRandomBot(player))
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
