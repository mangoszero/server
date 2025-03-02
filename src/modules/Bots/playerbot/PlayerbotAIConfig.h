#pragma once

#include "Config/Config.h"

class Player;
class PlayerbotMgr;
class ChatHandler;

/**
 * @brief Configuration class for Playerbot AI.
 * This class holds all the configuration parameters for the Playerbot AI and provides methods to initialize and access these parameters.
 */
class PlayerbotAIConfig
{
public:
    /**
     * @brief Constructor for PlayerbotAIConfig.
     * Initializes all configuration parameters with default values.
     */
    PlayerbotAIConfig();

public:
    /**
     * @brief Initializes the Playerbot AI configuration by reading from the configuration file.
     * @return True if initialization is successful, false otherwise.
     */
    bool Initialize();

    /**
     * @brief Checks if a given account ID is in the random bot account list.
     * @param id The account ID to check.
     * @return True if the account ID is in the list, false otherwise.
     */
    bool IsInRandomAccountList(uint32 id);

    /**
     * @brief Checks if a given item ID is in the random bot quest item list.
     * @param id The item ID to check.
     * @return True if the item ID is in the list, false otherwise.
     */
    bool IsInRandomQuestItemList(uint32 id);

    bool enabled;
    bool allowGuildBots;
    uint32 globalCoolDown, reactDelay, maxWaitForMove;
    float sightDistance, spellDistance, reactDistance, grindDistance, lootDistance,
        fleeDistance, tooCloseDistance, meleeDistance, followDistance, whisperDistance, contactDistance;
    uint32 criticalHealth, lowHealth, mediumHealth, almostFullHealth;
    uint32 lowMana, mediumMana;

    bool randomBotAutologin;
    std::string randomBotMapsAsString;
    std::vector<uint32> randomBotMaps;
    std::list<uint32> randomBotQuestItems;
    std::list<uint32> randomBotAccounts;
    std::list<uint32> randomBotSpellIds;
    uint32 randomBotTeleportDistance;
    float randomGearLoweringChance;
    float randomBotMaxLevelChance;
    uint32 minRandomBots, maxRandomBots;
    uint32 randomBotUpdateInterval, randomBotCountChangeMinInterval, randomBotCountChangeMaxInterval;
    uint32 minRandomBotInWorldTime, maxRandomBotInWorldTime;
    uint32 minRandomBotRandomizeTime, maxRandomBotRandomizeTime;
    uint32 minRandomBotReviveTime, maxRandomBotReviveTime;
    uint32 minRandomBotPvpTime, maxRandomBotPvpTime;
    uint32 minRandomBotsPerInterval, maxRandomBotsPerInterval;
    uint32 minRandomBotsPriceChangeInterval, maxRandomBotsPriceChangeInterval;
    bool randomBotJoinLfg; ///< Indicates if random bots should join Looking For Group.
    bool randomBotLoginAtStartup; ///< Indicates if random bots should login at startup.
    uint32 randomBotTeleLevel; ///< The teleport level for random bots.
    bool logInGroupOnly, logValuesPerTick;
    bool fleeingEnabled;
    std::string randomBotCombatStrategies, randomBotNonCombatStrategies;
    uint32 randomBotMinLevel, randomBotMaxLevel;
    float randomChangeMultiplier;
    uint32 specProbability[MAX_CLASSES][3];
    std::string commandPrefix;

    uint32 iterationsPerTick; ///< Number of iterations per tick.

    int commandServerPort; ///< Port for the command server.

    /**
     * @brief Gets the value of a configuration parameter by name.
     * @param name The name of the configuration parameter.
     * @return The value of the configuration parameter as a string.
     */
    std::string GetValue(std::string name) const;

    /**
     * @brief Sets the value of a configuration parameter by name.
     * @param name The name of the configuration parameter.
     * @param value The value to set the configuration parameter to.
     */
    void SetValue(std::string &name, std::string value);

private:
    void CreateRandomBots();
    Config config;
};

#define sPlayerbotAIConfig MaNGOS::Singleton<PlayerbotAIConfig>::Instance()
