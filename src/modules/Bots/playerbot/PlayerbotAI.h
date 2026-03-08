#pragma once

#include "PlayerbotMgr.h"
#include "PlayerbotAIBase.h"
#include "strategy/AiObjectContext.h"
#include "strategy/Engine.h"
#include "strategy/ExternalEventHelper.h"
#include "ChatFilter.h"
#include "PlayerbotSecurity.h"

class Player;
class PlayerbotMgr;
class ChatHandler;

using namespace std;
using namespace ai;

bool IsAlliance(uint8 race);

/**
 * @brief Handles chat commands for player bots.
 */
class PlayerbotChatHandler: protected ChatHandler
{
public:
    explicit PlayerbotChatHandler(Player* pMasterPlayer) : ChatHandler(pMasterPlayer) {}
    bool revive(const Player& botPlayer) { return HandleReviveCommand((char*)botPlayer.GetName()); }
    bool teleport(const Player& botPlayer) { return HandleSummonCommand((char*)botPlayer.GetName()); }
    void sysmessage(string str) { SendSysMessage(str.c_str()); }
    bool dropQuest(string str) { return HandleQuestRemoveCommand((char*)str.c_str()); }
    uint32 extractQuestId(string str);
    uint32 extractSpellId(string str)
    {
        char* source = (char*)str.c_str();
        return ExtractSpellIdFromLink(&source);
    }
};

namespace ai
{
    /**
     * @brief Calculates the minimum value for a given parameter.
     */
    class MinValueCalculator {
    public:
        MinValueCalculator(float def = 0.0f) {
            param = NULL;
            minValue = def;
        }

    public:
        void probe(float value, void* p) {
            if (!param || minValue >= value) {
            {
                minValue = value;
            }
                param = p;
            }
        }

    public:
        void* param;
        float minValue;
    };
};

enum BotState
{
    BOT_STATE_COMBAT = 0,
    BOT_STATE_NON_COMBAT = 1,
    BOT_STATE_DEAD = 2
};

#define BOT_STATE_MAX 3

/**
 * @brief Handles packet processing for player bots.
 */
class PacketHandlingHelper
{
public:
    void AddHandler(uint16 opcode, string handler);
    void Handle(ExternalEventHelper &helper);
    void AddPacket(const WorldPacket& packet);

private:
    map<uint16, string> handlers;
    stack<WorldPacket> queue;
};

/**
 * @brief Holds chat commands for player bots.
 */
class ChatCommandHolder
{
public:
    ChatCommandHolder(string command, Player* owner = NULL, uint32 type = CHAT_MSG_WHISPER) : command(command), owner(owner), type(type) {}
    ChatCommandHolder(ChatCommandHolder const& other)
    {
        this->command = other.command;
        this->owner = other.owner;
        this->type = other.type;
    }

public:
    string GetCommand() { return command; }
    Player* GetOwner() { return owner; }
    uint32 GetType() const { return type; }

private:
    string command;
    Player* owner;
    uint32 type;
};

/**
 * @brief Manages the AI for player bots.
 */
class PlayerbotAI : public PlayerbotAIBase
{
public:
    PlayerbotAI();
    PlayerbotAI(Player* bot);
    virtual ~PlayerbotAI();

public:
    virtual void UpdateAI(uint32 elapsed);
    virtual void UpdateAIInternal(uint32 elapsed);
    void HandleCommand(uint32 type, const string& text, Player& fromPlayer);
    void HandleBotOutgoingPacket(const WorldPacket& packet);
    void HandleMasterIncomingPacket(const WorldPacket& packet);
    void HandleMasterOutgoingPacket(const WorldPacket& packet);
    void HandleTeleportAck();
    void ChangeEngine(BotState type);
    void DoNextAction();
    void DoSpecificAction(string name);
    void ChangeStrategy(string name, BotState type);
    bool ContainsStrategy(StrategyType type);
    bool HasStrategy(string name, BotState type);
    bool HasStrategy(string name) { return HasStrategy(name, currentState); }
    void ResetStrategies();
    void ReInitCurrentEngine();
    void Reset();
    bool IsTank(Player* player);
    bool IsHeal(Player* player);
    bool IsRanged(Player* player);
    Creature* GetCreature(ObjectGuid guid);
    Unit* GetUnit(ObjectGuid guid);
    GameObject* GetGameObject(ObjectGuid guid);
    bool TellMaster(ostringstream &stream, PlayerbotSecurityLevel securityLevel = PLAYERBOT_SECURITY_ALLOW_ALL) { return TellMaster(stream.str(), securityLevel); }
    bool TellMaster(string text, PlayerbotSecurityLevel securityLevel = PLAYERBOT_SECURITY_ALLOW_ALL);
    bool TellMasterNoFacing(string text, PlayerbotSecurityLevel securityLevel = PLAYERBOT_SECURITY_ALLOW_ALL);
    void SpellInterrupted(uint32 spellid);
    uint32 CalculateGlobalCooldown(uint32 spellid);
    void InterruptSpell();
    void RemoveAura(string name);
    void RemoveShapeshift();
    void WaitForSpellCast(uint32 spellId);

    virtual bool CanCastSpell(string name, Unit* target);
    virtual bool CastSpell(string name, Unit* target);
    virtual bool HasAura(string spellName, Unit* player);
    virtual bool HasAnyAuraOf(Unit* player, ...);

    virtual bool IsInterruptableSpellCasting(Unit* player, string spell);
    virtual bool HasAuraToDispel(Unit* player, uint32 dispelType);
    bool CanCastSpell(uint32 spellid, Unit* target, bool checkHasSpell = true);

    bool HasAura(uint32 spellId, const Unit* player);
    bool CastSpell(uint32 spellId, Unit* target);
    bool canDispel(const SpellEntry* entry, uint32 dispelType);

public:
    Player* GetBot() { return bot; }
    Player* GetMaster() { return master; }
    void SetMaster(Player* master) { this->master = master; }
    AiObjectContext* GetAiObjectContext() { return aiObjectContext; }
    ChatHelper* GetChatHelper() { return &chatHelper; }
    bool IsOpposing(Player* player);
    static bool IsOpposing(uint8 race1, uint8 race2);
    PlayerbotSecurity* GetSecurity() { return &security; }

protected:
    Player* bot;
    Player* master;
    uint32 accountId;
    AiObjectContext* aiObjectContext;
    Engine* currentEngine;
    Engine* engines[BOT_STATE_MAX];
    BotState currentState;
    ChatHelper chatHelper;
    stack<ChatCommandHolder> chatCommands;
    PacketHandlingHelper botOutgoingPacketHandlers;
    PacketHandlingHelper masterIncomingPacketHandlers;
    PacketHandlingHelper masterOutgoingPacketHandlers;
    CompositeChatFilter chatFilter;
    PlayerbotSecurity security;
};

