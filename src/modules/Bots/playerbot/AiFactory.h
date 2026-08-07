#pragma once

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

        /**
         * @brief Folds a TalentTab DBC id onto its 0/1/2 position in the class's trees.
         * @return The tree index, or -1 if the id is not a tab of that class.
         */
        static int TalentTabToIndex(uint8 cls, uint32 talentTabId);
        static bool IsFeralCatSpec(Player* player);
};
