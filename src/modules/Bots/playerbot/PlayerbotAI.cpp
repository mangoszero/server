#include "../botpch.h"
#include "PlayerbotMgr.h"
#include "playerbot.h"

#include "AiFactory.h"

#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "strategy/values/LastMovementValue.h"
#include "strategy/actions/LogLevelAction.h"
#include "strategy/values/LastSpellCastValue.h"
#include "LootObjectStack.h"
#include "PlayerbotAIConfig.h"
#include "PlayerbotAI.h"
#include "PlayerbotFactory.h"
#include "PlayerbotSecurity.h"
#include "Util.h"

using namespace ai;
using namespace std;

vector<string>& split(const string &s, char delim, vector<string> &elems);
vector<string> split(const string &s, char delim);
char * strstri (string str1, string str2);
uint64 extractGuid(WorldPacket& packet);

uint32 PlayerbotChatHandler::extractQuestId(string str)
{
    char* source = (char*)str.c_str();
    char* cId = ExtractKeyFromLink(&source,"Hquest");
    return cId ? atol(cId) : 0;
}

void PacketHandlingHelper::AddHandler(uint16 opcode, string handler)
{
    handlers[opcode] = handler;
}

void PacketHandlingHelper::Handle(ExternalEventHelper &helper)
{
    while (!queue.empty())
    {
        helper.HandlePacket(handlers, queue.top());
        queue.pop();
    }
}

void PacketHandlingHelper::AddPacket(const WorldPacket& packet)
{
    if (handlers.find(packet.GetOpcode()) != handlers.end())
    {
        queue.push(WorldPacket(packet));
    }
}


PlayerbotAI::PlayerbotAI() : PlayerbotAIBase(), bot(NULL), aiObjectContext(NULL),
    currentEngine(NULL), chatHelper(this), chatFilter(this), accountId(0), security(NULL), master(NULL)
{
    for (int i = 0 ; i < BOT_STATE_MAX; i++)
    {
        engines[i] = NULL;
    }
}

PlayerbotAI::PlayerbotAI(Player* bot) :
    PlayerbotAIBase(), chatHelper(this), chatFilter(this), security(bot), master(NULL)
{
    this->bot = bot;

    accountId = sObjectMgr.GetPlayerAccountIdByGUID(bot->GetObjectGuid());

    aiObjectContext = AiFactory::createAiObjectContext(bot, this);

    engines[BOT_STATE_COMBAT] = AiFactory::createCombatEngine(bot, this, aiObjectContext);
    engines[BOT_STATE_NON_COMBAT] = AiFactory::createNonCombatEngine(bot, this, aiObjectContext);
    engines[BOT_STATE_DEAD] = AiFactory::createDeadEngine(bot, this, aiObjectContext);
    currentEngine = engines[BOT_STATE_NON_COMBAT];
    currentState = BOT_STATE_NON_COMBAT;

    //masterIncomingPacketHandlers.AddHandler(CMSG_GAMEOBJ_REPORT_USE, "use game object");
    masterIncomingPacketHandlers.AddHandler(CMSG_AREATRIGGER, "area trigger");
    masterIncomingPacketHandlers.AddHandler(CMSG_GAMEOBJ_USE, "use game object");
    masterIncomingPacketHandlers.AddHandler(CMSG_LOOT_ROLL, "loot roll");
    masterIncomingPacketHandlers.AddHandler(CMSG_GOSSIP_HELLO, "gossip hello");
    masterIncomingPacketHandlers.AddHandler(CMSG_QUESTGIVER_HELLO, "gossip hello");
    masterIncomingPacketHandlers.AddHandler(CMSG_QUESTGIVER_COMPLETE_QUEST, "complete quest");
    masterIncomingPacketHandlers.AddHandler(CMSG_QUESTGIVER_ACCEPT_QUEST, "accept quest");
    masterIncomingPacketHandlers.AddHandler(CMSG_ACTIVATETAXI, "activate taxi");
    masterIncomingPacketHandlers.AddHandler(CMSG_ACTIVATETAXIEXPRESS, "activate taxi");
    masterIncomingPacketHandlers.AddHandler(CMSG_MOVE_SPLINE_DONE, "taxi done");
    masterIncomingPacketHandlers.AddHandler(CMSG_GROUP_UNINVITE_GUID, "uninvite");
    masterIncomingPacketHandlers.AddHandler(CMSG_PUSHQUESTTOPARTY, "quest share");
    masterIncomingPacketHandlers.AddHandler(CMSG_GUILD_INVITE, "guild invite");

    botOutgoingPacketHandlers.AddHandler(SMSG_GROUP_INVITE, "group invite");
    botOutgoingPacketHandlers.AddHandler(BUY_ERR_NOT_ENOUGHT_MONEY, "not enough money");
    botOutgoingPacketHandlers.AddHandler(BUY_ERR_REPUTATION_REQUIRE, "not enough reputation");
    botOutgoingPacketHandlers.AddHandler(SMSG_GROUP_SET_LEADER, "group set leader");
    botOutgoingPacketHandlers.AddHandler(SMSG_FORCE_RUN_SPEED_CHANGE, "check mount state");
    botOutgoingPacketHandlers.AddHandler(SMSG_RESURRECT_REQUEST, "resurrect request");
    botOutgoingPacketHandlers.AddHandler(SMSG_INVENTORY_CHANGE_FAILURE, "cannot equip");
    botOutgoingPacketHandlers.AddHandler(SMSG_TRADE_STATUS, "trade status");
    botOutgoingPacketHandlers.AddHandler(SMSG_LOOT_RESPONSE, "loot response");
    botOutgoingPacketHandlers.AddHandler(SMSG_QUESTUPDATE_ADD_KILL, "quest objective completed");
    botOutgoingPacketHandlers.AddHandler(SMSG_ITEM_PUSH_RESULT, "item push result");
    botOutgoingPacketHandlers.AddHandler(SMSG_PARTY_COMMAND_RESULT, "party command");
    botOutgoingPacketHandlers.AddHandler(SMSG_CAST_FAILED, "cast failed");
    botOutgoingPacketHandlers.AddHandler(SMSG_DUEL_REQUESTED, "duel requested");
    //botOutgoingPacketHandlers.AddHandler(SMSG_LFG_ROLE_CHECK_UPDATE, "lfg role check");
    //botOutgoingPacketHandlers.AddHandler(SMSG_LFG_PROPOSAL_UPDATE, "lfg proposal");

    masterOutgoingPacketHandlers.AddHandler(SMSG_PARTY_COMMAND_RESULT, "party command");
    masterOutgoingPacketHandlers.AddHandler(MSG_RAID_READY_CHECK, "ready check");
    masterOutgoingPacketHandlers.AddHandler(MSG_RAID_READY_CHECK_FINISHED, "ready check finished");
}

PlayerbotAI::~PlayerbotAI()
{
    for (int i = 0 ; i < BOT_STATE_MAX; i++)
    {
        if (engines[i])
        {
            delete engines[i];
        }
    }

    if (aiObjectContext)
    {
        delete aiObjectContext;
    }
}

void PlayerbotAI::UpdateAI(uint32 elapsed)
{
    if (bot->IsBeingTeleported())
    {
        return;
    }

    if (nextAICheckDelay > sPlayerbotAIConfig.globalCoolDown &&
            bot->IsNonMeleeSpellCasted(true, true, false) &&
            *GetAiObjectContext()->GetValue<bool>("invalid target", "current target"))
    {
        Spell* spell = bot->GetCurrentSpell(CURRENT_GENERIC_SPELL);
        if (spell && !IsPositiveSpell(spell->m_spellInfo))
        {
            InterruptSpell();
            SetNextCheckDelay(sPlayerbotAIConfig.globalCoolDown);
        }
    }

    if (nextAICheckDelay > sPlayerbotAIConfig.maxWaitForMove && bot->IsInCombat() && !bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
    {
        nextAICheckDelay = sPlayerbotAIConfig.maxWaitForMove;
    }

    PlayerbotAIBase::UpdateAI(elapsed);
}

void PlayerbotAI::UpdateAIInternal(uint32 elapsed)
{
    ExternalEventHelper helper(aiObjectContext);
    while (!chatCommands.empty())
    {
        ChatCommandHolder holder = chatCommands.top();
        string command = holder.GetCommand();
        Player* owner = holder.GetOwner();
        if (!helper.ParseChatCommand(command, owner) && holder.GetType() == CHAT_MSG_WHISPER)
        {
            ostringstream out; out << "Unknown command " << command;
            TellMaster(out);
            helper.ParseChatCommand("help");
        }
        chatCommands.pop();
    }

    botOutgoingPacketHandlers.Handle(helper);
    masterIncomingPacketHandlers.Handle(helper);
    masterOutgoingPacketHandlers.Handle(helper);

    DoNextAction();
}

void PlayerbotAI::HandleTeleportAck()
{
    bot->GetMotionMaster()->Clear(true);
    if (bot->IsBeingTeleportedNear())
    {
        WorldPacket p = WorldPacket(MSG_MOVE_TELEPORT_ACK, 8 + 4 + 4);
        p << bot->GetObjectGuid();
        p << (uint32)0; // supposed to be flags? not used currently
        p << (uint32)time(0); // time - not currently used
        bot->GetSession()->HandleMoveTeleportAckOpcode(p);
    }
    else if (bot->IsBeingTeleportedFar())
    {
        bot->GetSession()->HandleMoveWorldportAckOpcode();
    }
}

void PlayerbotAI::Reset()
{
    if (bot->IsTaxiFlying())
    {
        return;
    }

    currentEngine = engines[BOT_STATE_NON_COMBAT];
    nextAICheckDelay = 0;

    aiObjectContext->GetValue<Unit*>("old target")->Set(NULL);
    aiObjectContext->GetValue<Unit*>("current target")->Set(NULL);
    aiObjectContext->GetValue<LootObject>("loot target")->Set(LootObject());
    aiObjectContext->GetValue<uint32>("lfg proposal")->Set(0);

    LastSpellCast & lastSpell = aiObjectContext->GetValue<LastSpellCast& >("last spell cast")->Get();
    lastSpell.Reset();

    LastMovement & lastMovement = aiObjectContext->GetValue<LastMovement& >("last movement")->Get();
    lastMovement.Set(NULL);

    bot->GetMotionMaster()->Clear();
    bot->m_taxi.ClearTaxiDestinations();
    InterruptSpell();

    for (int i = 0 ; i < BOT_STATE_MAX; i++)
    {
        engines[i]->Init();
    }
}

void PlayerbotAI::HandleCommand(uint32 type, const string& text, Player& fromPlayer)
{
    if (!GetSecurity()->CheckLevelFor(PLAYERBOT_SECURITY_INVITE, type != CHAT_MSG_WHISPER, &fromPlayer))
    {
        return;
    }

    if (type == CHAT_MSG_ADDON)
    {
        return;
    }

    string filtered = text;
    if (!sPlayerbotAIConfig.commandPrefix.empty())
    {
        if (filtered.find(sPlayerbotAIConfig.commandPrefix) != 0)
        {
            return;
        }

        filtered = filtered.substr(sPlayerbotAIConfig.commandPrefix.size());
    }

    filtered = chatFilter.Filter(trim((string&)filtered));
    if (filtered.empty())
    {
        return;
    }

    if (filtered.find("who") != 0 && !GetSecurity()->CheckLevelFor(PLAYERBOT_SECURITY_ALLOW_ALL, type != CHAT_MSG_WHISPER, &fromPlayer))
    {
        return;
    }

    if (type == CHAT_MSG_RAID_WARNING && filtered.find(bot->GetName()) != string::npos && filtered.find("award") == string::npos)
    {
        ChatCommandHolder cmd("warning", &fromPlayer, type);
        chatCommands.push(cmd);
        return;
    }

    if (filtered.size() > 2 && filtered.substr(0, 2) == "d " || filtered.size() > 3 && filtered.substr(0, 3) == "do ")
    {
        std::string action = filtered.substr(filtered.find(" ") + 1);
        DoSpecificAction(action);
    }
    else if (filtered == "reset")
    {
        Reset();
    }
    else
    {
        ChatCommandHolder cmd(filtered, &fromPlayer, type);
        chatCommands.push(cmd);
    }
}

void PlayerbotAI::HandleBotOutgoingPacket(const WorldPacket& packet)
{
    switch (packet.GetOpcode())
    {
    case SMSG_CAST_FAILED:
    {
        WorldPacket p(packet);
        p.rpos(0);
        uint8 status, result;
        p >> status >> result;
        if (result != SPELL_CAST_OK)
        {
            LastSpellCast& lastSpell = aiObjectContext->GetValue<LastSpellCast&>("last spell cast")->Get();
            SpellInterrupted(lastSpell.id);
            botOutgoingPacketHandlers.AddPacket(packet);
        }
        return;
    }
    case SMSG_SPELL_FAILURE:
    {
        WorldPacket p(packet);
        p.rpos(0);
        ObjectGuid casterGuid;
        p >> casterGuid.ReadAsPacked();
        if (casterGuid != bot->GetObjectGuid())
        {
            return;
        }

        uint32 spellId;
        p >> spellId;
        SpellInterrupted(spellId);
        return;
    }
    case SMSG_SPELL_DELAYED:
    {
        WorldPacket p(packet);
        p.rpos(0);
        ObjectGuid casterGuid;
        p >> casterGuid.ReadAsPacked();

        if (casterGuid != bot->GetObjectGuid())
        {
            return;
        }

        uint32 delaytime;
        p >> delaytime;
        if (delaytime <= 1000)
        {
            IncreaseNextCheckDelay(delaytime);
        }
        return;
    }
    default:
        botOutgoingPacketHandlers.AddPacket(packet);
    }
}

void PlayerbotAI::SpellInterrupted(uint32 spellid)
{
    LastSpellCast& lastSpell = aiObjectContext->GetValue<LastSpellCast&>("last spell cast")->Get();
    if (lastSpell.id != spellid)
    {
        return;
    }

    lastSpell.Reset();

    time_t now = time(0);
    if (now <= lastSpell.time)
    {
        return;
    }

    uint32 castTimeSpent = 1000 * (now - lastSpell.time);

    uint32 globalCooldown = CalculateGlobalCooldown(lastSpell.id);
    if (castTimeSpent < globalCooldown)
    {
        SetNextCheckDelay(globalCooldown - castTimeSpent);
    }
    else
    {
        SetNextCheckDelay(0);
    }

    lastSpell.id = 0;
}

uint32 PlayerbotAI::CalculateGlobalCooldown(uint32 spellid)
{
    if (!spellid)
    {
        return 0;
    }

    SpellEntry const *spellInfo = sSpellStore.LookupEntry(spellid );

    if (bot->GetGlobalCooldownMgr().HasGlobalCooldown(spellInfo))
    {
        return sPlayerbotAIConfig.globalCoolDown;
    }

    return sPlayerbotAIConfig.reactDelay;
}

void PlayerbotAI::HandleMasterIncomingPacket(const WorldPacket& packet)
{
    masterIncomingPacketHandlers.AddPacket(packet);
}

void PlayerbotAI::HandleMasterOutgoingPacket(const WorldPacket& packet)
{
    masterOutgoingPacketHandlers.AddPacket(packet);
}

void PlayerbotAI::ChangeEngine(BotState type)
{
    Engine* engine = engines[type];

    if (currentEngine != engine)
    {
        currentEngine = engine;
        currentState = type;
        ReInitCurrentEngine();

        switch (type)
        {
        case BOT_STATE_COMBAT:
            sLog.outDebug("=== %s COMBAT ===", bot->GetName());
            break;
        case BOT_STATE_NON_COMBAT:
            sLog.outDebug("=== %s NON-COMBAT ===", bot->GetName());
            break;
        case BOT_STATE_DEAD:
            sLog.outDebug("=== %s DEAD ===", bot->GetName());
            break;
        }
    }
}

void PlayerbotAI::DoNextAction()
{
    if (bot->IsBeingTeleported() /*|| bot->IsBeingTeleportedDelayEvent()*/|| (GetMaster() && GetMaster()->IsBeingTeleported()))
    {
        return;
    }

    currentEngine->DoNextAction(NULL);

    /*if (!bot->GetAurasByType(SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED).empty())
    {
        bot->m_movementInfo.SetMovementFlags((MovementFlags)(MOVEFLAG_FLYING|MOVEFLAG_CAN_FLY));

        WorldPacket packet(CMSG_MOVE_SET_FLY);
        packet << bot->GetObjectGuid().WriteAsPacked();
        packet << bot->m_movementInfo;
        bot->SetMover(bot);
        bot->GetSession()->HandleMovementOpcodes(packet);
    }*/

    Player* master = GetMaster();
    if (bot->IsMounted() && bot->IsFlying())
    {
        bot->m_movementInfo.SetMovementFlags((MovementFlags)(MOVEFLAG_FLYING|MOVEFLAG_CAN_FLY));

        //bot->SetSpeedRate(MOVE_FLIGHT, 1.0f, true);
        bot->SetSpeedRate(MOVE_RUN, 1.0f, true);

        if (master)
        {
            //bot->SetSpeedRate(MOVE_FLIGHT, master->GetSpeedRate(MOVE_FLIGHT), true);
            //bot->SetSpeedRate(MOVE_RUN, master->GetSpeedRate(MOVE_FLIGHT), true);
        }

    }

    if (currentEngine != engines[BOT_STATE_DEAD] && !bot->IsAlive())
    {
        ChangeEngine(BOT_STATE_DEAD);
    }

    if (currentEngine == engines[BOT_STATE_DEAD] && bot->IsAlive())
    {
        ChangeEngine(BOT_STATE_NON_COMBAT);
    }

    Group *group = bot->GetGroup();
    if (!master && group)
    {
        for (GroupReference *gref = group->GetFirstMember(); gref; gref = gref->next())
        {
            Player* member = gref->getSource();
            PlayerbotAI* ai = bot->GetPlayerbotAI();
            if (member && member->IsInWorld() && !member->GetPlayerbotAI() && (!master || master->GetPlayerbotAI()))
            {
                ai->SetMaster(member);
                ai->ResetStrategies();
                ai->TellMaster("Hello");
                break;
            }
        }
    }
}

void PlayerbotAI::ReInitCurrentEngine()
{
    InterruptSpell();
    currentEngine->Init();
}

void PlayerbotAI::ChangeStrategy(string names, BotState type)
{
    Engine* e = engines[type];
    if (!e)
    {
        return;
    }

    e->ChangeStrategy(names);
}

void PlayerbotAI::DoSpecificAction(string name)
{
    for (int i = 0 ; i < BOT_STATE_MAX; i++)
    {
        ostringstream out;
        ActionResult res = engines[i]->ExecuteAction(name);
        switch (res)
        {
        case ACTION_RESULT_UNKNOWN:
            continue;
        case ACTION_RESULT_OK:
            out << name << ": done";
            TellMaster(out);
            return;
        case ACTION_RESULT_IMPOSSIBLE:
            out << name << ": impossible";
            TellMaster(out);
            return;
        case ACTION_RESULT_USELESS:
            out << name << ": useless";
            TellMaster(out);
            return;
        case ACTION_RESULT_FAILED:
            out << name << ": failed";
            TellMaster(out);
            return;
        }
    }
    ostringstream out;
    out << name << ": unknown action";
    TellMaster(out);
}

bool PlayerbotAI::ContainsStrategy(StrategyType type)
{
    for (int i = 0 ; i < BOT_STATE_MAX; i++)
    {
        if (engines[i]->ContainsStrategy(type))
        {
            return true;
        }
    }
    return false;
}

bool PlayerbotAI::HasStrategy(string name, BotState type)
{
    return engines[type]->HasStrategy(name);
}

void PlayerbotAI::ResetStrategies()
{
    for (int i = 0 ; i < BOT_STATE_MAX; i++)
    {
        engines[i]->removeAllStrategies();
    }

    AiFactory::AddDefaultCombatStrategies(bot, this, engines[BOT_STATE_COMBAT]);
    AiFactory::AddDefaultNonCombatStrategies(bot, this, engines[BOT_STATE_NON_COMBAT]);
    AiFactory::AddDefaultDeadStrategies(bot, this, engines[BOT_STATE_DEAD]);
}

bool PlayerbotAI::IsRanged(Player* player)
{
    PlayerbotAI* botAi = player->GetPlayerbotAI();
    if (botAi)
    {
        return botAi->ContainsStrategy(STRATEGY_TYPE_RANGED);
    }

    switch (player->getClass())
    {
    //case CLASS_DEATH_KNIGHT:
    case CLASS_PALADIN:
    case CLASS_WARRIOR:
    case CLASS_ROGUE:
        return false;
    case CLASS_DRUID:
        return !HasAnyAuraOf(player, "cat form", "bear form", "dire bear form", NULL);
    }
    return true;
}

bool PlayerbotAI::IsTank(Player* player)
{
    PlayerbotAI* botAi = player->GetPlayerbotAI();
    if (botAi)
    {
        return botAi->ContainsStrategy(STRATEGY_TYPE_TANK);
    }

    switch (player->getClass())
    {
    //case CLASS_DEATH_KNIGHT:
    case CLASS_PALADIN:
    case CLASS_WARRIOR:
        return true;
    case CLASS_DRUID:
        return HasAnyAuraOf(player, "bear form", "dire bear form", NULL);
    }
    return false;
}

bool PlayerbotAI::IsHeal(Player* player)
{
    PlayerbotAI* botAi = player->GetPlayerbotAI();
    if (botAi)
    {
        return botAi->ContainsStrategy(STRATEGY_TYPE_HEAL);
    }

    switch (player->getClass())
    {
    case CLASS_PRIEST:
        return true;
    case CLASS_DRUID:
        return HasAnyAuraOf(player, "tree of life form", NULL);
    }
    return false;
}



namespace MaNGOS
{

    class UnitByGuidInRangeCheck
    {
    public:
        UnitByGuidInRangeCheck(WorldObject const* obj, ObjectGuid guid, float range) : i_obj(obj), i_range(range), i_guid(guid) {}
        WorldObject const& GetFocusObject() const { return *i_obj; }
        bool operator()(Unit* u)
        {
            return u->GetObjectGuid() == i_guid && i_obj->IsWithinDistInMap(u, i_range);
        }
    private:
        WorldObject const* i_obj;
        float i_range;
        ObjectGuid i_guid;
    };

    class GameObjectByGuidInRangeCheck
    {
    public:
        GameObjectByGuidInRangeCheck(WorldObject const* obj, ObjectGuid guid, float range) : i_obj(obj), i_range(range), i_guid(guid) {}
        WorldObject const& GetFocusObject() const { return *i_obj; }
        bool operator()(GameObject* u)
        {
            if (u && i_obj->IsWithinDistInMap(u, i_range) && u->isSpawned() && u->GetGOInfo() && u->GetObjectGuid() == i_guid)
            {
                return true;
            }

            return false;
        }
    private:
        WorldObject const* i_obj;
        float i_range;
        ObjectGuid i_guid;
    };

};


Unit* PlayerbotAI::GetUnit(ObjectGuid guid)
{
    if (!guid)
    {
        return NULL;
    }

    list<Unit*> targets;

    MaNGOS::UnitByGuidInRangeCheck u_check(bot, guid, sPlayerbotAIConfig.sightDistance);
    MaNGOS::UnitListSearcher<MaNGOS::UnitByGuidInRangeCheck> searcher(targets, u_check);
    Cell::VisitAllObjects(bot, searcher, sPlayerbotAIConfig.sightDistance);

    if (targets.empty())
    {
        return NULL;
    }

    return *targets.begin();
}


Creature* PlayerbotAI::GetCreature(ObjectGuid guid)
{
    if (!guid)
    {
        return NULL;
    }

    list<Unit *> targets;

    MaNGOS::UnitByGuidInRangeCheck u_check(bot, guid, sPlayerbotAIConfig.sightDistance);
    MaNGOS::UnitListSearcher<MaNGOS::UnitByGuidInRangeCheck> searcher(targets, u_check);
    Cell::VisitAllObjects(bot, searcher, sPlayerbotAIConfig.sightDistance);

    for(list<Unit *>::iterator i = targets.begin(); i != targets.end(); i++)
    {
        Creature* creature = dynamic_cast<Creature*>(*i);
        if (creature)
        {
            return creature;
        }
    }

    return NULL;
}

GameObject* PlayerbotAI::GetGameObject(ObjectGuid guid)
{
    if (!guid)
    {
        return NULL;
    }

    list<GameObject*> targets;

    MaNGOS::GameObjectByGuidInRangeCheck u_check(bot, guid, sPlayerbotAIConfig.sightDistance);
    MaNGOS::GameObjectListSearcher<MaNGOS::GameObjectByGuidInRangeCheck> searcher(targets, u_check);
    Cell::VisitAllObjects(bot, searcher, sPlayerbotAIConfig.sightDistance);

    for(list<GameObject*>::iterator i = targets.begin(); i != targets.end(); i++)
    {
        GameObject* go = *i;
        if (go && go->isSpawned())
        {
            return go;
        }
    }

    return NULL;
}

bool PlayerbotAI::TellMasterNoFacing(string text, PlayerbotSecurityLevel securityLevel)
{
    Player* master = GetMaster();
    if (!master)
    {
        return false;
    }

    if (!GetSecurity()->CheckLevelFor(securityLevel, true, master))
    {
        return false;
    }

    if (sPlayerbotAIConfig.whisperDistance && !bot->GetGroup() && sRandomPlayerbotMgr.IsRandomBot(bot) &&
            master->GetSession()->GetSecurity() < SEC_GAMEMASTER &&
            (bot->GetMapId() != master->GetMapId() || bot->GetDistance(master) > sPlayerbotAIConfig.whisperDistance))
        return false;

    bot->Whisper(text, LANG_UNIVERSAL, master->GetObjectGuid());
    return true;
}

bool PlayerbotAI::TellMaster(string text, PlayerbotSecurityLevel securityLevel)
{
    if (!TellMasterNoFacing(text, securityLevel))
    {
        return false;
    }

    if (!bot->isMoving() && !bot->IsInCombat() && bot->GetMapId() == master->GetMapId())
    {
        if (!bot->IsInFront(master, sPlayerbotAIConfig.sightDistance, M_PI / 2))
        {
            bot->SetFacingTo(bot->GetAngle(master));
        }

        bot->HandleEmoteCommand(EMOTE_ONESHOT_TALK);
    }

    return true;
}

bool IsRealAura(Player* bot, Aura* aura, Unit* unit)
{
    if (!aura)
    {
        return false;
    }

    if (!unit->IsHostileTo(bot))
    {
        return true;
    }

    uint32 stacks = aura->GetHolder()->GetStackAmount();
    if (stacks >= aura->GetHolder()->GetSpellProto()->StackAmount)
    {
        return true;
    }

    if (aura->GetHolder()->GetCaster() == bot || aura->GetHolder()->IsPositive() || aura->GetHolder()->IsAreaAura())
    {
        return true;
    }

    return false;
}

bool PlayerbotAI::HasAura(string name, Unit* unit)
{
    if (!unit)
    {
        return false;
    }

    uint32 spellId = aiObjectContext->GetValue<uint32>("spell id", name)->Get();
    if (spellId)
    {
        return HasAura(spellId, unit);
    }

    wstring wnamepart;
    if (!Utf8toWStr(name, wnamepart))
    {
        return 0;
    }

    wstrToLower(wnamepart);

    for (uint32 auraType = SPELL_AURA_BIND_SIGHT; auraType < TOTAL_AURAS; auraType++)
    {
        Unit::AuraList const& auras = unit->GetAurasByType((AuraType)auraType);
        for (Unit::AuraList::const_iterator i = auras.begin(); i != auras.end(); i++)
        {
            Aura* aura = *i;
            if (!aura)
            {
                continue;
            }

            const string auraName = aura->GetSpellProto()->SpellName[0];
            if (auraName.empty() || auraName.length() != wnamepart.length() || !Utf8FitTo(auraName, wnamepart))
            {
                continue;
            }

            if (IsRealAura(bot, aura, unit))
            {
                return true;
            }
        }
    }

    return false;
}

bool PlayerbotAI::HasAura(uint32 spellId, const Unit* unit)
{
    if (!spellId || !unit)
    {
        return false;
    }

    for (uint32 effect = EFFECT_INDEX_0; effect <= EFFECT_INDEX_2; effect++)
    {
        Aura* aura = ((Unit*)unit)->GetAura(spellId, (SpellEffectIndex)effect);

        if (IsRealAura(bot, aura, (Unit*)unit))
        {
            return true;
        }
    }

    return false;
}

bool PlayerbotAI::HasAnyAuraOf(Unit* player, ...)
{
    if (!player)
    {
        return false;
    }

    va_list vl;
    va_start(vl, player);

    const char* cur;
    do {
        cur = va_arg(vl, const char*);
        if (cur && HasAura(cur, player)) {
        {
            va_end(vl);
        }
            return true;
        }
    }
    while (cur);

    va_end(vl);
    return false;
}

bool PlayerbotAI::CanCastSpell(string name, Unit* target)
{
    return CanCastSpell(aiObjectContext->GetValue<uint32>("spell id", name)->Get(), target);
}

bool PlayerbotAI::CanCastSpell(uint32 spellid, Unit* target, bool checkHasSpell)
{
    if (!spellid)
    {
        return false;
    }

    if (!target)
    {
        target = bot;
    }

    if (checkHasSpell && !bot->HasSpell(spellid))
    {
        return false;
    }

    if (bot->HasSpellCooldown(spellid))
    {
        return false;
    }

    bool positiveSpell = IsPositiveSpell(spellid);
    if (positiveSpell && bot->IsHostileTo(target))
    {
        return false;
    }

    if (!positiveSpell && bot->IsFriendlyTo(target))
    {
        return false;
    }

    SpellEntry const *spellInfo = sSpellStore.LookupEntry(spellid );
    if (!spellInfo)
    {
        return false;
    }

    if (target->IsImmuneToSpell(spellInfo, false))
    {
        return false;
    }

    if (bot != target && bot->GetDistance(target) > sPlayerbotAIConfig.sightDistance)
    {
        return false;
    }

    ObjectGuid oldSel = bot->GetSelectionGuid();
    bot->SetSelectionGuid(target->GetObjectGuid());
    Spell *spell = new Spell(bot, spellInfo, false );

    spell->m_targets.setUnitTarget(target);
    spell->m_CastItem = aiObjectContext->GetValue<Item*>("item for spell", spellid)->Get();
    spell->m_targets.setItemTarget(spell->m_CastItem);
    SpellCastResult result = spell->CheckCast(false);
    delete spell;
    bot->SetSelectionGuid(oldSel);

    switch (result)
    {
    case SPELL_FAILED_NOT_INFRONT:
    case SPELL_FAILED_NOT_STANDING:
    case SPELL_FAILED_UNIT_NOT_INFRONT:
    case SPELL_FAILED_MOVING:
    case SPELL_FAILED_TRY_AGAIN:
    case SPELL_FAILED_BAD_IMPLICIT_TARGETS:
    case SPELL_FAILED_BAD_TARGETS:
    case SPELL_CAST_OK:
        return true;
    default:
        return false;
    }
}


bool PlayerbotAI::CastSpell(string name, Unit* target)
{
    bool result = CastSpell(aiObjectContext->GetValue<uint32>("spell id", name)->Get(), target);
    if (result)
    {
        aiObjectContext->GetValue<time_t>("last spell cast time", name)->Set(time(0));
    }

    return result;
}

bool PlayerbotAI::CastSpell(uint32 spellId, Unit* target)
{
    if (!spellId)
    {
        return false;
    }

    if (!target)
    {
        target = bot;
    }

    Pet* pet = bot->GetPet();
    if (pet && pet->HasSpell(spellId))
    {
        pet->ToggleAutocast(spellId, true);
        TellMaster("My pet will auto-cast this spell");
        return true;
    }

    aiObjectContext->GetValue<LastSpellCast&>("last spell cast")->Get().Set(spellId, target->GetObjectGuid(), time(0));
    aiObjectContext->GetValue<LastMovement&>("last movement")->Get().Set(NULL);

    const SpellEntry* const pSpellInfo = sSpellStore.LookupEntry(spellId);

    MotionMaster &mm = *bot->GetMotionMaster();
    if (bot->isMoving() && GetSpellCastTime(pSpellInfo, NULL))
    {
        return false;
    }

    if (bot->IsTaxiFlying())
    {
        return false;
    }

    bot->clearUnitState( UNIT_STAT_CHASE );
    bot->clearUnitState( UNIT_STAT_FOLLOW );

    ObjectGuid oldSel = bot->GetSelectionGuid();
    bot->SetSelectionGuid(target->GetObjectGuid());

    Spell *spell = new Spell(bot, pSpellInfo, false);

    SpellCastTargets targets;
    targets.setUnitTarget(target);
    WorldObject* faceTo = target;

    if (pSpellInfo->Targets & TARGET_FLAG_ITEM)
    {
        spell->m_CastItem = aiObjectContext->GetValue<Item*>("item for spell", spellId)->Get();
        targets.setItemTarget(spell->m_CastItem);
    }

    if (pSpellInfo->Effect[0] == SPELL_EFFECT_OPEN_LOCK ||
        pSpellInfo->Effect[0] == SPELL_EFFECT_SKINNING)
    {
        LootObject loot = *aiObjectContext->GetValue<LootObject>("loot target");
        if (!loot.IsLootPossible(bot))
        {
            return false;
        }

        GameObject* go = GetGameObject(loot.guid);
        if (go && go->isSpawned())
        {
            WorldPacket* const packetgouse = new WorldPacket(CMSG_GAMEOBJ_USE, 8);
            *packetgouse << loot.guid;
            bot->GetSession()->QueuePacket(packetgouse);
            targets.setGOTarget(go);
            faceTo = go;
        }
        else
        {
            Unit* creature = GetUnit(loot.guid);
            if (creature)
            {
                targets.setUnitTarget(creature);
                faceTo = creature;
            }
        }
    }


    if (!bot->IsInFront(faceTo, sPlayerbotAIConfig.sightDistance))
    {
        bot->SetFacingTo(bot->GetAngle(faceTo));
        SetNextCheckDelay(sPlayerbotAIConfig.globalCoolDown);
        return false;
    }

    WaitForSpellCast(spellId);

    spell->prepare(&targets);
    bot->SetSelectionGuid(oldSel);

    LastSpellCast& lastSpell = aiObjectContext->GetValue<LastSpellCast&>("last spell cast")->Get();
    return lastSpell.id == spellId;
}

void PlayerbotAI::WaitForSpellCast(uint32 spellId)
{
    const SpellEntry* const pSpellInfo = sSpellStore.LookupEntry(spellId);

    float castTime = GetSpellCastTime(pSpellInfo) + sPlayerbotAIConfig.reactDelay;
    if (IsChanneledSpell(pSpellInfo))
    {
        int32 duration = GetSpellDuration(pSpellInfo);
        if (duration > 0)
        {
            castTime += duration;
        }
    }

    castTime = ceil(castTime);

    uint32 globalCooldown = CalculateGlobalCooldown(spellId);
    if (castTime < globalCooldown)
    {
        castTime = globalCooldown;
    }

    SetNextCheckDelay(castTime);
}

void PlayerbotAI::InterruptSpell()
{
    if (bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
    {
        return;
    }

    LastSpellCast& lastSpell = aiObjectContext->GetValue<LastSpellCast&>("last spell cast")->Get();

    for (int type = CURRENT_MELEE_SPELL; type < CURRENT_CHANNELED_SPELL; type++)
    {
        Spell* spell = bot->GetCurrentSpell((CurrentSpellTypes)type);
        if (!spell)
        {
            continue;
        }

        bot->InterruptSpell((CurrentSpellTypes)type);

        WorldPacket data(SMSG_SPELL_FAILURE, 8 + 1 + 4 + 1);
        data << bot->GetPackGUID();
        data << uint8(1);
        data << uint32(spell->m_spellInfo->Id);
        data << uint8(0);
        bot->SendMessageToSet(&data, true);

        data.Initialize(SMSG_SPELL_FAILED_OTHER, 8 + 1 + 4 + 1);
        data << bot->GetObjectGuid();
        data << uint8(1);
        data << uint32(spell->m_spellInfo->Id);
        data << uint8(0);
        bot->SendMessageToSet(&data, true);

        SpellInterrupted(spell->m_spellInfo->Id);
    }

    SpellInterrupted(lastSpell.id);
}


void PlayerbotAI::RemoveAura(string name)
{
    uint32 spellid = aiObjectContext->GetValue<uint32>("spell id", name)->Get();
    if (spellid && HasAura(spellid, bot))
    {
        bot->RemoveAurasDueToSpell(spellid);
    }
}

bool PlayerbotAI::IsInterruptableSpellCasting(Unit* target, string spell)
{
    uint32 spellid = aiObjectContext->GetValue<uint32>("spell id", spell)->Get();
    if (!spellid || !target->IsNonMeleeSpellCasted(true))
    {
        return false;
    }

    SpellEntry const *spellInfo = sSpellStore.LookupEntry(spellid );
    if (!spellInfo)
    {
        return false;
    }

    if (target->IsImmuneToSpell(spellInfo, false))
    {
        return false;
    }

    for (int32 i = EFFECT_INDEX_0; i <= EFFECT_INDEX_2; i++)
    {
        if ((spellInfo->InterruptFlags & SPELL_INTERRUPT_FLAG_INTERRUPT) && spellInfo->PreventionType == SPELL_PREVENTION_TYPE_SILENCE)
        {
            return true;
        }

        if ((spellInfo->Effect[i] == SPELL_EFFECT_INTERRUPT_CAST) &&
                !target->IsImmuneToSpellEffect(spellInfo, (SpellEffectIndex)i, true))
            return true;
    }

    return false;
}

bool PlayerbotAI::HasAuraToDispel(Unit* target, uint32 dispelType)
{
    for (uint32 type = SPELL_AURA_NONE; type < TOTAL_AURAS; ++type)
    {
        Unit::AuraList const& auras = target->GetAurasByType((AuraType)type);
        for (Unit::AuraList::const_iterator itr = auras.begin(); itr != auras.end(); ++itr)
        {
            const Aura* aura = *itr;
            const SpellEntry* entry = aura->GetSpellProto();
            uint32 spellId = entry->Id;

            bool isPositiveSpell = IsPositiveSpell(spellId);
            if (isPositiveSpell && bot->IsFriendlyTo(target))
            {
                continue;
            }

            if (!isPositiveSpell && bot->IsHostileTo(target))
            {
                continue;
            }

            if (canDispel(entry, dispelType))
            {
                return true;
            }
        }
    }
    return false;
}


#ifndef WIN32
inline int strcmpi(const char* s1, const char* s2)
{
    for (; *s1 && *s2 && (toupper(*s1) == toupper(*s2)); ++s1, ++s2);
    {
        return *s1 - *s2;
    }
}
#endif

bool PlayerbotAI::canDispel(const SpellEntry* entry, uint32 dispelType)
{
    if (entry->Dispel != dispelType)
    {
        return false;
    }

    return !entry->SpellName[0] ||
        (strcmpi((const char*)entry->SpellName[0], "demon skin") &&
        strcmpi((const char*)entry->SpellName[0], "mage armor") &&
        strcmpi((const char*)entry->SpellName[0], "frost armor") &&
        strcmpi((const char*)entry->SpellName[0], "wavering will") &&
        strcmpi((const char*)entry->SpellName[0], "chilled") &&
        strcmpi((const char*)entry->SpellName[0], "ice armor"));
}

bool IsAlliance(uint8 race)
{
    return race == RACE_HUMAN || race == RACE_DWARF || race == RACE_NIGHTELF ||
            race == RACE_GNOME;
}

bool PlayerbotAI::IsOpposing(Player* player)
{
    return IsOpposing(player->getRace(), bot->getRace());
}

bool PlayerbotAI::IsOpposing(uint8 race1, uint8 race2)
{
    return (IsAlliance(race1) && !IsAlliance(race2)) || (!IsAlliance(race1) && IsAlliance(race2));
}

void PlayerbotAI::RemoveShapeshift()
{
    RemoveAura("bear form");
    RemoveAura("dire bear form");
    RemoveAura("moonkin form");
    RemoveAura("travel form");
    RemoveAura("cat form");
    RemoveAura("flight form");
    RemoveAura("swift flight form");
    RemoveAura("aquatic form");
    RemoveAura("ghost wolf");
    RemoveAura("tree of life");
}
