#ifndef _RandomPlayerbotMgr_H
#define _RandomPlayerbotMgr_H

#include "Common.h"
#include "PlayerbotAIBase.h"
#include "PlayerbotMgr.h"

class WorldPacket;
class Player;
class Unit;
class Object;
class Item;

using namespace std;
/**
* \struct AreaCreatureStats
* \brief Entry representing creature levels within an area for playerbot spawning decisions
*/
struct AreaCreatureStats
{
    uint8   minLevel;
    uint8   maxLevel;
    uint16  creatureCount;

    AreaCreatureStats() : minLevel(0), maxLevel(0), creatureCount(0) {}
};
class MANGOS_DLL_SPEC RandomPlayerbotMgr : public PlayerbotHolder
{
    public:
        RandomPlayerbotMgr();

    /**
     * @brief Destructor for RandomPlayerbotMgr.
     */
    virtual ~RandomPlayerbotMgr();

    public:
        bool IsRandomBot(Player* bot);
        bool IsRandomBot(uint32 bot);
        void Randomize(Player* bot);
        void RandomizeFirst(Player* bot);
        void IncreaseLevel(Player* bot);
        void ScheduleTeleport(uint32 bot);
        void HandleCommand(uint32 type, const string& text, Player& fromPlayer);
        void OnPlayerLogout(Player* player);
        void OnPlayerLogin(Player* player);
        Player* GetRandomPlayer();
        void PrintStats();
        double GetBuyMultiplier(Player* bot);
        double GetSellMultiplier(Player* bot);
        uint32 GetLootAmount(Player* bot);
        void SetLootAmount(Player* bot, uint32 value);
        uint32 GetTradeDiscount(Player* bot);
        void Refresh(Player* bot);
        virtual void UpdateAIInternal(uint32 elapsed);

    protected:
        virtual void OnBotLoginInternal(Player * const bot) {}

    private:
        uint32 GetEventValue(uint32 bot, string event);
        uint32 SetEventValue(uint32 bot, string event, uint32 value, uint32 validIn);
        list<uint32> GetBots();
        vector<uint32> GetFreeBots(bool alliance);
        uint32 AddRandomBot(bool alliance);
        bool ProcessBot(uint32 bot);
        void ScheduleRandomize(uint32 bot, uint32 time);
        void RandomTeleport(Player* bot, uint32 mapId, float teleX, float teleY, float teleZ);
        void RandomTeleportForLevel(Player* bot);
        void RandomTeleport(Player* bot, vector<WorldLocation> &locs);
        uint32 GetZoneLevel(uint32 mapId, float teleX, float teleY, float teleZ);
        bool IsZoneSafeForBot(Player* bot, uint32 mapId, float x, float y, float z);
        void CalculateAreaCreatureStats();

    private:
        vector<Player*> players;
        int processTicks;
        std::map<uint32, AreaCreatureStats> m_areaCreatureStatsMap;
        std::map<std::pair<uint32, uint32>, uint32> m_cellToAreaCache;
};

#define sRandomPlayerbotMgr MaNGOS::Singleton<RandomPlayerbotMgr>::Instance()

#endif
