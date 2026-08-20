#ifndef _PLAYERBOTMGR_H
#define _PLAYERBOTMGR_H

#include <unordered_map>
#include "Platform/Define.h"
#include "PlayerbotAIBase.h"

class WorldPacket;
class Player;
class Unit;
class Object;
class Item;

typedef std::unordered_map<uint64, Player*> PlayerBotMap;

class PlayerbotHolder : public PlayerbotAIBase
{
    public:
        PlayerbotHolder();
        virtual ~PlayerbotHolder();

        // Returns false when the login was refused outright, so a caller driving the bot
        // from a persisted event can retire that event instead of retrying it every pass.
        bool AddPlayerBot(uint64 guid, uint32 masterAccountId);
        void LogoutPlayerBot(uint64 guid);
        Player* GetPlayerBot (uint64 guid) const;
        PlayerBotMap::const_iterator GetPlayerBotsBegin() const { return playerBots.begin(); }
        PlayerBotMap::const_iterator GetPlayerBotsEnd()   const { return playerBots.end();   }

        virtual void UpdateAIInternal(uint32 elapsed);
        void UpdateSessions(uint32 elapsed);

        void LogoutAllBots();
        void OnBotLogin(Player * const bot);

        list<string> HandlePlayerbotCommand(char* args, Player* master = NULL);
        bool ProcessBotCommand(string cmd, ObjectGuid guid, bool admin, uint32 masterAccountId);
        uint32 GetAccountId(string name);

    protected:
        virtual void OnBotLoginInternal(Player * const bot) = 0;

    protected:
        PlayerBotMap playerBots;
};

class PlayerbotMgr : public PlayerbotHolder
{
    public:
        PlayerbotMgr(Player* const master);
        virtual ~PlayerbotMgr();

        void HandleMasterIncomingPacket(const WorldPacket& packet);
        void HandleMasterOutgoingPacket(const WorldPacket& packet);
        void HandleCommand(uint32 type, const string& text);

        virtual void UpdateAIInternal(uint32 elapsed);

        Player* GetMaster() const { return master; };

        void SaveToDB();

    protected:
        virtual void OnBotLoginInternal(Player * const bot);

    private:
        Player* const master;
};

#endif
