#ifndef _RandomPlayerbotFactory_H
#define _RandomPlayerbotFactory_H

#include "Common.h"
#include "PlayerbotAIBase.h"

class WorldPacket;
class Player;
class Unit;
class Object;
class Item;

using namespace std;

/**
 * @brief Factory class for creating random player bots.
 * This class provides methods to create and manage random player bots, including their names and guilds.
 */
class RandomPlayerbotFactory
{
public:
    /**
     * @brief Constructor for RandomPlayerbotFactory.
     * @param accountId The account ID for the random player bot.
     */
    RandomPlayerbotFactory(uint32 accountId);

    /**
     * @brief Destructor for RandomPlayerbotFactory.
     */
    virtual ~RandomPlayerbotFactory() {}

public:
    /**
     * @brief Creates a random player bot of a given class.
     * @param cls The class of the player bot.
     * @return True if the player bot is created successfully, false otherwise.
     */
    bool CreateRandomBot(uint8 cls);

    private:
    /**
     * @brief Creates a random name for a player bot.
     * @return The random name for the player bot.
     */
        string CreateRandomBotName();


private:
    uint32 accountId; ///< The account ID for the random player bot.
    static map<uint8, vector<uint8> > availableRaces; ///< Map of available races for each class.
};

#endif
