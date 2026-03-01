#pragma once

#include <unordered_map>

class Player;

using namespace ai;

class AiFactory
{
public:
    static AiObjectContext* createAiObjectContext(Player* player, PlayerbotAI* ai);
    static Engine* createCombatEngine(Player* player, PlayerbotAI* const facade, AiObjectContext* AiObjectContext);
    static Engine* createNonCombatEngine(Player* player, PlayerbotAI* const facade, AiObjectContext* AiObjectContext);
    static Engine* createDeadEngine(Player* player, PlayerbotAI* const facade, AiObjectContext* AiObjectContext);
    static void AddDefaultNonCombatStrategies(Player* player, PlayerbotAI* const facade, Engine* nonCombatEngine);
    static void AddDefaultDeadStrategies(Player* player, PlayerbotAI* const facade, Engine* deadEngine);
    static void AddDefaultCombatStrategies(Player* player, PlayerbotAI* const facade, Engine* engine);

public:
    static int GetPlayerSpecTab(Player* player);
    static map<uint32, int32> GetPlayerSpecTabs(Player* player);
    static void ClearSpecCache();

private:
    static std::unordered_map<uint32, std::pair<map<uint32, int32>, uint32>> specCache;
    static const uint32 SPEC_CACHE_DURATION = 300000; // 5 minutes in milliseconds
};
