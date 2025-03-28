#include "../botpch.h"
#include "PlayerbotAIConfig.h"
#include "playerbot.h"
#include "RandomPlayerbotFactory.h"
#include "AccountMgr.h"
#include "SystemConfig.h"

using namespace std;

INSTANTIATE_SINGLETON_1(PlayerbotAIConfig);

/**
 * @brief Constructor for PlayerbotAIConfig.
 * Initializes all configuration parameters with default values.
 */
PlayerbotAIConfig::PlayerbotAIConfig()
    : enabled(false),
      allowGuildBots(false),
      globalCoolDown(0),
      reactDelay(0),
      maxWaitForMove(0),
      sightDistance(0.0f),
      spellDistance(0.0f),
      reactDistance(0.0f),
      grindDistance(0.0f),
      lootDistance(0.0f),
      fleeDistance(0.0f),
      tooCloseDistance(0.0f),
      meleeDistance(0.0f),
      followDistance(0.0f),
      whisperDistance(0.0f),
      contactDistance(0.0f),
      criticalHealth(0),
      lowHealth(0),
      mediumHealth(0),
      almostFullHealth(0),
      lowMana(0),
      mediumMana(0),
      randomBotAutologin(false),
      randomBotTeleportDistance(0),
      randomGearLoweringChance(0.0f),
      randomBotMaxLevelChance(0.0f),
      minRandomBots(0),
      maxRandomBots(0),
      randomBotUpdateInterval(0),
      randomBotCountChangeMinInterval(0),
      randomBotCountChangeMaxInterval(0),
      minRandomBotInWorldTime(0),
      maxRandomBotInWorldTime(0),
      minRandomBotRandomizeTime(0),
      maxRandomBotRandomizeTime(0),
      minRandomBotReviveTime(0),
      maxRandomBotReviveTime(0),
      minRandomBotPvpTime(0),
      maxRandomBotPvpTime(0),
      minRandomBotsPerInterval(0),
      maxRandomBotsPerInterval(0),
      minRandomBotsPriceChangeInterval(0),
      maxRandomBotsPriceChangeInterval(0),
      randomBotJoinLfg(false),
      randomBotLoginAtStartup(false),
      randomBotTeleLevel(0),
      logInGroupOnly(false),
      logValuesPerTick(false),
      fleeingEnabled(false),
      randomBotMinLevel(0),
      randomBotMaxLevel(0),
      randomChangeMultiplier(0.0f),
      commandServerPort(0),
      iterationsPerTick(0)
{
}

/**
 * @brief Template function to load a list of values from a comma-separated string.
 * @param value The comma-separated string.
 * @param list The list to load the values into.
 */
template <class T>
void LoadList(string value, T &list)
{
    vector<string> ids = split(value, ',');
    for (vector<string>::iterator i = ids.begin(); i != ids.end(); i++)
    {
        uint32 id = atoi((*i).c_str());
        if (!id)
        {
            continue;
        }

        list.push_back(id);
    }
}

/**
 * @brief Initializes the Playerbot AI configuration by reading from the configuration file.
 * @return True if initialization is successful, false otherwise.
 */
bool PlayerbotAIConfig::Initialize()
{
    sLog.outString("Initializing AI Playerbot by ike3, based on the original Playerbot by blueboy");

    if (!config.SetSource(SYSCONFDIR"aiplayerbot.conf"))
    {
        sLog.outString("AI Playerbot is Disabled. Unable to open configuration file aiplayerbot.conf");
        return false;
    }

    enabled = config.GetBoolDefault("AiPlayerbot.Enabled", true);
    if (!enabled)
    {
        sLog.outString("AI Playerbot is Disabled in aiplayerbot.conf");
        return false;
    }

    // Load various configuration parameters from the configuration file
    globalCoolDown = (uint32) config.GetIntDefault("AiPlayerbot.GlobalCooldown", 500);
    maxWaitForMove = config.GetIntDefault("AiPlayerbot.MaxWaitForMove", 3000);
    reactDelay = (uint32) config.GetIntDefault("AiPlayerbot.ReactDelay", 100);

    sightDistance = config.GetFloatDefault("AiPlayerbot.SightDistance", 50.0f);
    spellDistance = config.GetFloatDefault("AiPlayerbot.SpellDistance", 30.0f);
    reactDistance = config.GetFloatDefault("AiPlayerbot.ReactDistance", 150.0f);
    grindDistance = config.GetFloatDefault("AiPlayerbot.GrindDistance", 100.0f);
    lootDistance = config.GetFloatDefault("AiPlayerbot.LootDistance", 20.0f);
    fleeDistance = config.GetFloatDefault("AiPlayerbot.FleeDistance", 20.0f);
    tooCloseDistance = config.GetFloatDefault("AiPlayerbot.TooCloseDistance", 7.0f);
    meleeDistance = config.GetFloatDefault("AiPlayerbot.MeleeDistance", 1.5f);
    followDistance = config.GetFloatDefault("AiPlayerbot.FollowDistance", 1.5f);
    whisperDistance = config.GetFloatDefault("AiPlayerbot.WhisperDistance", 6000.0f);
    contactDistance = config.GetFloatDefault("AiPlayerbot.ContactDistance", 0.5f);

    criticalHealth = config.GetIntDefault("AiPlayerbot.CriticalHealth", 20);
    lowHealth = config.GetIntDefault("AiPlayerbot.LowHealth", 50);
    mediumHealth = config.GetIntDefault("AiPlayerbot.MediumHealth", 70);
    almostFullHealth = config.GetIntDefault("AiPlayerbot.AlmostFullHealth", 85);
    lowMana = config.GetIntDefault("AiPlayerbot.LowMana", 15);
    mediumMana = config.GetIntDefault("AiPlayerbot.MediumMana", 40);

    randomGearLoweringChance = config.GetFloatDefault("AiPlayerbot.RandomGearLoweringChance", 0.15f);
    randomBotMaxLevelChance = config.GetFloatDefault("AiPlayerbot.RandomBotMaxLevelChance", 0.4f);

    iterationsPerTick = config.GetIntDefault("AiPlayerbot.IterationsPerTick", 4);

    allowGuildBots = config.GetBoolDefault("AiPlayerbot.AllowGuildBots", true);

    // Load lists of values from the configuration file
    randomBotMapsAsString = config.GetStringDefault("AiPlayerbot.RandomBotMaps", "0,1,530,571");
    LoadList<vector<uint32> >(randomBotMapsAsString, randomBotMaps);
    LoadList<list<uint32> >(config.GetStringDefault("AiPlayerbot.RandomBotQuestItems", "6948,5175,5176,5177,5178"), randomBotQuestItems);
    LoadList<list<uint32> >(config.GetStringDefault("AiPlayerbot.RandomBotSpellIds", "54197"), randomBotSpellIds);

    randomBotAutologin = config.GetBoolDefault("AiPlayerbot.RandomBotAutologin", true);
    minRandomBots = config.GetIntDefault("AiPlayerbot.MinRandomBots", 50);
    maxRandomBots = config.GetIntDefault("AiPlayerbot.MaxRandomBots", 200);
    randomBotUpdateInterval = config.GetIntDefault("AiPlayerbot.RandomBotUpdateInterval", 60);
    randomBotCountChangeMinInterval = config.GetIntDefault("AiPlayerbot.RandomBotCountChangeMinInterval", 24 * 3600);
    randomBotCountChangeMaxInterval = config.GetIntDefault("AiPlayerbot.RandomBotCountChangeMaxInterval", 3 * 24 * 3600);
    minRandomBotInWorldTime = config.GetIntDefault("AiPlayerbot.MinRandomBotInWorldTime", 2 * 3600);
    maxRandomBotInWorldTime = config.GetIntDefault("AiPlayerbot.MaxRandomBotInWorldTime", 14 * 24 * 3600);
    minRandomBotRandomizeTime = config.GetIntDefault("AiPlayerbot.MinRandomBotRandomizeTime", 2 * 3600);
    maxRandomBotRandomizeTime = config.GetIntDefault("AiPlayerbot.MaxRandomRandomizeTime", 14 * 24 * 3600);
    minRandomBotReviveTime = config.GetIntDefault("AiPlayerbot.MinRandomBotReviveTime", 60);
    maxRandomBotReviveTime = config.GetIntDefault("AiPlayerbot.MaxRandomReviveTime", 300);
    randomBotTeleportDistance = config.GetIntDefault("AiPlayerbot.RandomBotTeleportDistance", 1000);
    minRandomBotsPerInterval = config.GetIntDefault("AiPlayerbot.MinRandomBotsPerInterval", 50);
    maxRandomBotsPerInterval = config.GetIntDefault("AiPlayerbot.MaxRandomBotsPerInterval", 100);
    minRandomBotsPriceChangeInterval = config.GetIntDefault("AiPlayerbot.MinRandomBotsPriceChangeInterval", 2 * 3600);
    maxRandomBotsPriceChangeInterval = config.GetIntDefault("AiPlayerbot.MaxRandomBotsPriceChangeInterval", 48 * 3600);
    randomBotJoinLfg = config.GetBoolDefault("AiPlayerbot.RandomBotJoinLfg", true);
    logInGroupOnly = config.GetBoolDefault("AiPlayerbot.LogInGroupOnly", true);
    logValuesPerTick = config.GetBoolDefault("AiPlayerbot.LogValuesPerTick", false);
    fleeingEnabled = config.GetBoolDefault("AiPlayerbot.FleeingEnabled", true);
    randomBotMinLevel = config.GetIntDefault("AiPlayerbot.RandomBotMinLevel", 1);
    randomBotMaxLevel = config.GetIntDefault("AiPlayerbot.RandomBotMaxLevel", 255);
    randomBotLoginAtStartup = config.GetBoolDefault("AiPlayerbot.RandomBotLoginAtStartup", true);
    randomBotTeleLevel = config.GetIntDefault("AiPlayerbot.RandomBotTeleLevel", 3);

    randomChangeMultiplier = config.GetFloatDefault("AiPlayerbot.RandomChangeMultiplier", 1.0);

    randomBotCombatStrategies = config.GetStringDefault("AiPlayerbot.RandomBotCombatStrategies", "+dps,+attack weak");
    randomBotNonCombatStrategies = config.GetStringDefault("AiPlayerbot.RandomBotNonCombatStrategies", "+grind,+move random,+loot");

    commandPrefix = config.GetStringDefault("AiPlayerbot.CommandPrefix", "");

    commandServerPort = config.GetIntDefault("AiPlayerbot.CommandServerPort", 0);

    // Load class spec probabilities from the configuration file
    for (uint32 cls = 0; cls < MAX_CLASSES; ++cls)
    {
        for (uint32 spec = 0; spec < 3; ++spec)
        {
            ostringstream os; os << "AiPlayerbot.RandomClassSpecProbability." << cls << "." << spec;
            specProbability[cls][spec] = config.GetIntDefault(os.str().c_str(), 33);
        }
    }

    CreateRandomBots();
    sLog.outString("AI Playerbot configuration loaded");

    return true;
}

/**
 * @brief Checks if a given account ID is in the random bot account list.
 * @param id The account ID to check.
 * @return True if the account ID is in the list, false otherwise.
 */
bool PlayerbotAIConfig::IsInRandomAccountList(uint32 id)
{
    return find(randomBotAccounts.begin(), randomBotAccounts.end(), id) != randomBotAccounts.end();
}

/**
 * @brief Checks if a given item ID is in the random bot quest item list.
 * @param id The item ID to check.
 * @return True if the item ID is in the list, false otherwise.
 */
bool PlayerbotAIConfig::IsInRandomQuestItemList(uint32 id)
{
    return find(randomBotQuestItems.begin(), randomBotQuestItems.end(), id) != randomBotQuestItems.end();
}

/**
 * @brief Gets the value of a configuration parameter by name.
 * @param name The name of the configuration parameter.
 * @return The value of the configuration parameter as a string.
 */
string PlayerbotAIConfig::GetValue(string name) const
{
    ostringstream out;

    if (name == "GlobalCooldown")
    {
        out << globalCoolDown;
    }
    else if (name == "ReactDelay")
    {
        out << reactDelay;
    }

    else if (name == "SightDistance")
    {
        out << sightDistance;
    }
    else if (name == "SpellDistance")
    {
        out << spellDistance;
    }
    else if (name == "ReactDistance")
    {
        out << reactDistance;
    }
    else if (name == "GrindDistance")
    {
        out << grindDistance;
    }
    else if (name == "LootDistance")
    {
        out << lootDistance;
    }
    else if (name == "FleeDistance")
    {
        out << fleeDistance;
    }

    else if (name == "CriticalHealth")
    {
        out << criticalHealth;
    }
    else if (name == "LowHealth")
    {
        out << lowHealth;
    }
    else if (name == "MediumHealth")
    {
        out << mediumHealth;
    }
    else if (name == "AlmostFullHealth")
    {
        out << almostFullHealth;
    }
    else if (name == "LowMana")
    {
        out << lowMana;
    }

    else if (name == "IterationsPerTick")
    {
        out << iterationsPerTick;
    }

    return out.str();
}

/**
 * @brief Sets the value of a configuration parameter by name.
 * @param name The name of the configuration parameter.
 * @param value The value to set the configuration parameter to.
 */
void PlayerbotAIConfig::SetValue(string &name, string value)
{
    istringstream out(value, istringstream::in);

    if (name == "GlobalCooldown")
    {
        out >> globalCoolDown;
    }
    else if (name == "ReactDelay")
    {
        out >> reactDelay;
    }

    else if (name == "SightDistance")
    {
        out >> sightDistance;
    }
    else if (name == "SpellDistance")
    {
        out >> spellDistance;
    }
    else if (name == "ReactDistance")
    {
        out >> reactDistance;
    }
    else if (name == "GrindDistance")
    {
        out >> grindDistance;
    }
    else if (name == "LootDistance")
    {
        out >> lootDistance;
    }
    else if (name == "FleeDistance")
    {
        out >> fleeDistance;
    }

    else if (name == "CriticalHealth")
    {
        out >> criticalHealth;
    }
    else if (name == "LowHealth")
    {
        out >> lowHealth;
    }
    else if (name == "MediumHealth")
    {
        out >> mediumHealth;
    }
    else if (name == "AlmostFullHealth")
    {
        out >> almostFullHealth;
    }
    else if (name == "LowMana")
    {
        out >> lowMana;
    }

    else if (name == "IterationsPerTick")
    {
        out >> iterationsPerTick;
    }
}


void PlayerbotAIConfig::CreateRandomBots()
{
    string randomBotAccountPrefix = config.GetStringDefault("AiPlayerbot.RandomBotAccountPrefix", "rndbot");
    int32 randomBotAccountCount = config.GetIntDefault("AiPlayerbot.RandomBotAccountCount", 50);

    if (config.GetBoolDefault("AiPlayerbot.DeleteRandomBotAccounts", false))
    {
        sLog.outBasic("Deleting random bot accounts...");
        QueryResult *results = LoginDatabase.PQuery("SELECT `id` FROM `account` WHERE `username` LIKE '%s%%'", randomBotAccountPrefix.c_str());
        if (results)
        {
            do
            {
                Field* fields = results->Fetch();
                sAccountMgr.DeleteAccount(fields[0].GetUInt32());
            } while (results->NextRow());

            delete results;
        }

        CharacterDatabase.Execute("DELETE FROM `ai_playerbot_random_bots`");
        sLog.outBasic("Random bot accounts deleted");
    }

    for (int accountNumber = 0; accountNumber < randomBotAccountCount; ++accountNumber)
    {
        ostringstream out; out << randomBotAccountPrefix << accountNumber;
        string accountName = out.str();
        QueryResult *results = LoginDatabase.PQuery("SELECT `id` FROM `account` WHERE `username` = '%s'", accountName.c_str());
        if (results)
        {
            delete results;
            continue;
        }

        string password = "";
        for (int i = 0; i < 10; i++)
        {
            password += (char)urand('!', 'z');
        }
        sAccountMgr.CreateAccount(accountName, password);

        sLog.outDetail("Account %s created for random bots", accountName.c_str());
    }

    LoginDatabase.PExecute("UPDATE `account` SET `expansion` = '%u', `playerbot` = %u WHERE `username` LIKE '%s%%'", 0,true, randomBotAccountPrefix.c_str());

    int totalRandomBotChars = 0;
    for (int accountNumber = 0; accountNumber < randomBotAccountCount; ++accountNumber)
    {
        ostringstream out; out << randomBotAccountPrefix << accountNumber;
        string accountName = out.str();

        QueryResult *results = LoginDatabase.PQuery("SELECT `id` FROM `account` WHERE `username` = '%s'", accountName.c_str());
        if (!results)
        {
            continue;
        }

        Field* fields = results->Fetch();
        uint32 accountId = fields[0].GetUInt32();
        delete results;

        randomBotAccounts.push_back(accountId);

        int count = sAccountMgr.GetCharactersCount(accountId);
        if (count >= 10)
        {
            totalRandomBotChars += count;
            continue;
        }

        RandomPlayerbotFactory factory(accountId);
        for (uint8 cls = CLASS_WARRIOR; cls < MAX_CLASSES; ++cls)
        {
            if (cls != 10 && cls != 6)
            {
                factory.CreateRandomBot(cls);
            }
        }

        totalRandomBotChars += sAccountMgr.GetCharactersCount(accountId);
    }

    sLog.outBasic("%d random bot accounts with %d characters available", randomBotAccounts.size(), totalRandomBotChars);
}
