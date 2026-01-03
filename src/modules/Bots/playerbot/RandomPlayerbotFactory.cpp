#include "Config/Config.h"
#include "../botpch.h"
#include "playerbot.h"
#include "PlayerbotAIConfig.h"
#include "PlayerbotFactory.h"
#include "AccountMgr.h"
#include "ObjectMgr.h"
#include "Database/DatabaseEnv.h"
#include "PlayerbotAI.h"
#include "Player.h"
#include "RandomPlayerbotFactory.h"
#include "SystemConfig.h"

/**
 * A static map that stores the available races for each class.
 *
 * The key is a uint8 representing the class ID (e.g., CLASS_WARRIOR, CLASS_PALADIN).
 * The value is a vector of uint8 representing the race IDs (e.g., RACE_HUMAN, RACE_ORC)
 * that are available for that class.
 *
 * This map is initialized in the constructor of RandomPlayerbotFactory and is used to
 * determine the possible races when creating a random bot for a specific class.
 */
map<uint8, vector<uint8> > RandomPlayerbotFactory::availableRaces;

/**
 * Constructor for RandomPlayerbotFactory.
 * Initializes the available races for each class.
 * @param accountId The account ID for the random bot.
 */
RandomPlayerbotFactory::RandomPlayerbotFactory(uint32 accountId) : accountId(accountId)
{
    availableRaces[CLASS_WARRIOR].push_back(RACE_HUMAN);
    availableRaces[CLASS_WARRIOR].push_back(RACE_NIGHTELF);
    availableRaces[CLASS_WARRIOR].push_back(RACE_GNOME);
    availableRaces[CLASS_WARRIOR].push_back(RACE_DWARF);
    availableRaces[CLASS_WARRIOR].push_back(RACE_ORC);
    availableRaces[CLASS_WARRIOR].push_back(RACE_UNDEAD);
    availableRaces[CLASS_WARRIOR].push_back(RACE_TAUREN);
    availableRaces[CLASS_WARRIOR].push_back(RACE_TROLL);

    availableRaces[CLASS_PALADIN].push_back(RACE_HUMAN);
    availableRaces[CLASS_PALADIN].push_back(RACE_DWARF);

    availableRaces[CLASS_ROGUE].push_back(RACE_HUMAN);
    availableRaces[CLASS_ROGUE].push_back(RACE_DWARF);
    availableRaces[CLASS_ROGUE].push_back(RACE_NIGHTELF);
    availableRaces[CLASS_ROGUE].push_back(RACE_GNOME);
    availableRaces[CLASS_ROGUE].push_back(RACE_ORC);
    availableRaces[CLASS_ROGUE].push_back(RACE_TROLL);

    availableRaces[CLASS_PRIEST].push_back(RACE_HUMAN);
    availableRaces[CLASS_PRIEST].push_back(RACE_DWARF);
    availableRaces[CLASS_PRIEST].push_back(RACE_NIGHTELF);
    availableRaces[CLASS_PRIEST].push_back(RACE_TROLL);
    availableRaces[CLASS_PRIEST].push_back(RACE_UNDEAD);

    availableRaces[CLASS_MAGE].push_back(RACE_HUMAN);
    availableRaces[CLASS_MAGE].push_back(RACE_GNOME);
    availableRaces[CLASS_MAGE].push_back(RACE_UNDEAD);
    availableRaces[CLASS_MAGE].push_back(RACE_TROLL);

    availableRaces[CLASS_WARLOCK].push_back(RACE_HUMAN);
    availableRaces[CLASS_WARLOCK].push_back(RACE_GNOME);
    availableRaces[CLASS_WARLOCK].push_back(RACE_UNDEAD);
    availableRaces[CLASS_WARLOCK].push_back(RACE_ORC);

    availableRaces[CLASS_SHAMAN].push_back(RACE_ORC);
    availableRaces[CLASS_SHAMAN].push_back(RACE_TAUREN);
    availableRaces[CLASS_SHAMAN].push_back(RACE_TROLL);

    availableRaces[CLASS_HUNTER].push_back(RACE_DWARF);
    availableRaces[CLASS_HUNTER].push_back(RACE_NIGHTELF);
    availableRaces[CLASS_HUNTER].push_back(RACE_ORC);
    availableRaces[CLASS_HUNTER].push_back(RACE_TAUREN);
    availableRaces[CLASS_HUNTER].push_back(RACE_TROLL);

    availableRaces[CLASS_DRUID].push_back(RACE_NIGHTELF);
    availableRaces[CLASS_DRUID].push_back(RACE_TAUREN);
}

/**
 * Creates a random bot for the specified class.
 * @param cls The class of the bot to create.
 * @return True if the bot was created successfully, false otherwise.
 */
bool RandomPlayerbotFactory::CreateRandomBot(uint8 cls)
{
    sLog.outDetail("Creating new random bot for class %d", cls);

    uint8 gender = rand() % 2 ? GENDER_MALE : GENDER_FEMALE;

    uint8 race = availableRaces[cls][urand(0, availableRaces[cls].size() - 1)];
    string name = CreateRandomBotName();
    if (name.empty())
    {
        return false;
    }

    uint8 skin = urand(0, 7);
    uint8 face = urand(0, 7);
    uint8 hairStyle = urand(0, 7);
    uint8 hairColor = urand(0, 7);
    uint8 facialHair = urand(0, 7);
    uint8 outfitId = 0;

    WorldSession* session = new WorldSession(accountId, NULL, SEC_PLAYER, 0, LOCALE_enUS);
    if (!session)
    {
        sLog.outError("Couldn't create session for random bot account %d", accountId);
        delete session;
        return false;
    }

    Player *player = new Player(session);
    if (!player->Create(sObjectMgr.GeneratePlayerLowGuid(), name, race, cls, gender, skin, face, hairStyle, hairColor, facialHair, outfitId))
    {
        player->DeleteFromDB(player->GetObjectGuid(), accountId, true, true);
        delete session;
        delete player;
        sLog.outError("Unable to create random bot for account %d - name: \"%s\"; race: %u; class: %u; gender: %u; skin: %u; face: %u; hairStyle: %u; hairColor: %u; facialHair: %u; outfitId: %u",
            accountId, name.c_str(), race, cls, gender, skin, face, hairStyle, hairColor, facialHair, outfitId);
        return false;
    }

    player->setCinematic(2);
    player->SetAtLoginFlag(AT_LOGIN_NONE);
    player->SaveToDB();

    sLog.outDetail("Random bot created for account %d - name: \"%s\"; race: %u; class: %u; gender: %u; skin: %u; face: %u; hairStyle: %u; hairColor: %u; facialHair: %u; outfitId: %u",
        accountId, name.c_str(), race, cls, gender, skin, face, hairStyle, hairColor, facialHair, outfitId);

    return true;
}

/**
 * Generates a random name for a bot.
 *
 * This function queries the database to find a random name from the `ai_playerbot_names` table
 * that is not already used by a character in the `characters` table. It ensures that the generated
 * name is unique and available for use.
 *
 * @return A randomly generated bot name, or an empty string if no names are available.
 */
string RandomPlayerbotFactory::CreateRandomBotName()
{
    // Query the database to get the maximum name_id from the ai_playerbot_names table
    QueryResult *result = CharacterDatabase.Query("SELECT MAX(`name_id`) FROM `ai_playerbot_names`");
    if (!result)
    {
        // Return an empty string if the query fails
        return "";
    }

    // Fetch the result and get the maximum name_id
    Field *fields = result->Fetch();
    uint32 maxId = fields[0].GetUInt32();
    delete result;

    // Generate a random id between 0 and maxId
    uint32 id = urand(0, maxId);

    // Query the database to get a random name that is not already used by a character
    result = CharacterDatabase.PQuery("SELECT `n`.`name` FROM `ai_playerbot_names` n "
            "LEFT OUTER JOIN `characters` e ON `e`.`name` = `n`.`name` "
            "WHERE `e`.`guid` IS NULL AND `n`.`name_id` >= '%u' ORDER BY `n`.`name_id` LIMIT 1", id);
    if (!result)
    {
        // Log an error and return an empty string if no names are left
        sLog.outError("No more names left for random bots");
        return "";
    }

    // Fetch the result and get the name
    Field *nfields = result->Fetch();
    string name = nfields[0].GetCppString();
    delete result;

    // Return the generated name
    return name;
}
