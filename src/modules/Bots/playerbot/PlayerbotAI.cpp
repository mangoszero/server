#include "Utilities/MathDefines.h"
#include <cmath>
#include <string>
#include <cstdlib>
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
#include <cstdarg>

using namespace ai;
using namespace std;

// Function declarations
vector<string>& split(const string &s, char delim, vector<string> &elems);
vector<string> split(const string &s, char delim);
char * strstri (string str1, string str2);
uint64 extractGuid(WorldPacket& packet);

/**
 * Extracts the quest ID from a string.
 * @param str The input string.
 * @return The extracted quest ID.
 */
uint32 PlayerbotChatHandler::extractQuestId(string str)
{
    char* source = (char*)str.c_str();
    char* cId = ExtractKeyFromLink(&source,"Hquest");
    return cId ? std::strtoul(cId, NULL, 10) : 0;
}

/**
 * Adds a packet handler for a specific opcode.
 * @param opcode The opcode.
 * @param handler The handler name.
 */
void PacketHandlingHelper::AddHandler(uint16 opcode, string handler)
{
    handlers[opcode] = handler;
}

/**
 * Handles packets using the provided ExternalEventHelper.
 * @param helper The ExternalEventHelper instance.
 */
void PacketHandlingHelper::Handle(ExternalEventHelper &helper)
{
    while (!queue.empty())
    {
        helper.HandlePacket(handlers, queue.top());
        queue.pop();
    }
}

/**
 * Adds a packet to the queue for handling.
 * @param packet The packet to add.
 */
void PacketHandlingHelper::AddPacket(const WorldPacket& packet)
{
    if (handlers.find(packet.GetOpcode()) != handlers.end())
    {
        queue.push(WorldPacket(packet));
    }
}

/**
 * Default constructor for PlayerbotAI.
 */
PlayerbotAI::PlayerbotAI() : PlayerbotAIBase(), bot(NULL), aiObjectContext(NULL),
    currentEngine(NULL), chatHelper(this), chatFilter(this), accountId(0), security(NULL), master(NULL), currentState(BOT_STATE_NON_COMBAT),
    m_eatingUntil(0), m_drinkingUntil(0),
    m_isJumping(false), m_jumpStartTime(0),
    m_jumpStartX(0.f), m_jumpStartY(0.f), m_jumpStartZ(0.f),
    m_jumpSinAngle(0.f), m_jumpCosAngle(1.f), m_jumpXYSpeed(0.f),
    m_pendingJump(false), m_jumpHere(false), m_jumpRequestTime(0),
    m_jumpTargetX(0.f), m_jumpTargetY(0.f), m_jumpTargetZ(0.f), m_jumpTargetO(0.f)
{
    for (int i = 0 ; i < BOT_STATE_MAX; i++)
    {
        engines[i] = NULL;
    }
}

/**
 * Constructor for PlayerbotAI with a bot parameter.
 * @param bot The player bot.
 */
PlayerbotAI::PlayerbotAI(Player* bot)
    : PlayerbotAIBase(), chatHelper(this), chatFilter(this), security(bot), master(NULL),
    m_eatingUntil(0), m_drinkingUntil(0), m_wasDead(false),
    m_isJumping(false), m_jumpStartTime(0),
    m_jumpStartX(0.f), m_jumpStartY(0.f), m_jumpStartZ(0.f),
    m_jumpSinAngle(0.f), m_jumpCosAngle(1.f), m_jumpXYSpeed(0.f),
    m_pendingJump(false), m_jumpHere(false), m_jumpRequestTime(0),
    m_jumpTargetX(0.f), m_jumpTargetY(0.f), m_jumpTargetZ(0.f), m_jumpTargetO(0.f)
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
    botOutgoingPacketHandlers.AddHandler(SMSG_LOOT_START_ROLL, "loot roll");
    masterIncomingPacketHandlers.AddHandler(CMSG_GOSSIP_HELLO, "gossip hello");
    masterIncomingPacketHandlers.AddHandler(CMSG_QUESTGIVER_HELLO, "gossip hello");
    masterIncomingPacketHandlers.AddHandler(CMSG_QUESTGIVER_COMPLETE_QUEST, "complete quest");
    masterIncomingPacketHandlers.AddHandler(CMSG_QUESTGIVER_ACCEPT_QUEST, "accept quest");
    masterIncomingPacketHandlers.AddHandler(CMSG_ACTIVATETAXI, "activate taxi");
    masterIncomingPacketHandlers.AddHandler(CMSG_ACTIVATETAXIEXPRESS, "activate taxi");
    masterIncomingPacketHandlers.AddHandler(CMSG_MOVE_SPLINE_DONE, "taxi done");
    masterIncomingPacketHandlers.AddHandler(CMSG_GROUP_DISBAND, "disband");
    masterIncomingPacketHandlers.AddHandler(CMSG_GROUP_UNINVITE_GUID, "uninvite");
    masterIncomingPacketHandlers.AddHandler(CMSG_GROUP_UNINVITE, "uninvite");
    masterIncomingPacketHandlers.AddHandler(CMSG_PUSHQUESTTOPARTY, "quest share");
    masterIncomingPacketHandlers.AddHandler(CMSG_REPOP_REQUEST, "master released spirit");
    masterIncomingPacketHandlers.AddHandler(CMSG_SPIRIT_HEALER_ACTIVATE, "master spirit healer");

    botOutgoingPacketHandlers.AddHandler(SMSG_GUILD_INVITE, "guild invite");
    botOutgoingPacketHandlers.AddHandler(SMSG_GROUP_INVITE, "group invite");
    botOutgoingPacketHandlers.AddHandler(SMSG_PETITION_SHOW_SIGNATURES, "petition sign");
    botOutgoingPacketHandlers.AddHandler(BUY_ERR_NOT_ENOUGHT_MONEY, "not enough money");
    botOutgoingPacketHandlers.AddHandler(BUY_ERR_REPUTATION_REQUIRE, "not enough reputation");
    botOutgoingPacketHandlers.AddHandler(SMSG_GROUP_SET_LEADER, "group set leader");
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

    masterOutgoingPacketHandlers.AddHandler(SMSG_FORCE_RUN_SPEED_CHANGE, "check mount state");
    masterOutgoingPacketHandlers.AddHandler(SMSG_PARTY_COMMAND_RESULT, "party command");
    masterOutgoingPacketHandlers.AddHandler(MSG_RAID_READY_CHECK, "ready check");
    masterOutgoingPacketHandlers.AddHandler(MSG_RAID_READY_CHECK_FINISHED, "ready check finished");
}

/**
 * Destructor for PlayerbotAI.
 */
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

static const float BOT_JUMP_VELOCITY = 7.9557f;
static const float BOT_JUMP_GRAVITY  = 19.2911f;

void PlayerbotAI::RequestJump(bool here)
{
    if (m_pendingJump || m_isJumping)
    {
        return;
    }

    Player* master = GetMaster();
    if (!master)
    {
        return;
    }

    m_jumpTargetX = master->GetPositionX();
    m_jumpTargetY = master->GetPositionY();
    m_jumpTargetZ = master->GetPositionZ();
    m_jumpTargetO = master->GetOrientation();
    m_pendingJump = true;
    m_jumpHere = here;
    m_jumpRequestTime = getMSTime();
}

void PlayerbotAI::StartJump(bool forward, float orientation)
{
    if (m_isJumping || bot->IsDead())
    {
        return;
    }

    // Clear() removes generators; it does NOT interrupt the spline already running, so
    // without the explicit stop the old leg keeps advancing underneath the simulated
    // parabola and UpdateSplineMovement overwrites the jump's own position writes.
    bot->GetMotionMaster()->Clear();
    bot->InterruptMoving(true);
    bot->GetMotionMaster()->MoveIdle();

    m_jumpStartTime = getMSTime();
    m_jumpStartX    = bot->GetPositionX();
    m_jumpStartY    = bot->GetPositionY();
    m_jumpStartZ    = bot->GetPositionZ();

    float o = (orientation >= 0.f) ? orientation : bot->GetOrientation();
    m_jumpCosAngle  = cosf(o);
    m_jumpSinAngle  = sinf(o);
    m_jumpXYSpeed   = forward ? bot->GetSpeed(MOVE_RUN) : 0.f;
    m_isJumping     = true;

    bot->SetFallInformation(0, m_jumpStartZ);

    bot->m_movementInfo.SetMovementFlags(MOVEFLAG_FALLING);
    if (forward)
    {
        bot->m_movementInfo.AddMovementFlag(MOVEFLAG_FORWARD);
    }
    bot->m_movementInfo.SetFallTime(0);
    bot->m_movementInfo.SetJumpInfo(-BOT_JUMP_VELOCITY, m_jumpCosAngle, m_jumpSinAngle, m_jumpXYSpeed);
    bot->m_movementInfo.ChangePosition(m_jumpStartX, m_jumpStartY, m_jumpStartZ, o);
    bot->m_movementInfo.UpdateTime(m_jumpStartTime);

    WorldPacket data(MSG_MOVE_JUMP, 64);
    data << bot->GetPackGUID();
    bot->m_movementInfo.Write(data);
    bot->SendMessageToSet(&data, false);
}

void PlayerbotAI::UpdateJump()
{
    if (m_pendingJump && !m_isJumping)
    {
        if (getMSTime() - m_jumpRequestTime > 10000)
        {
            m_pendingJump = false;
        }
        else
        {
            float dx = m_jumpTargetX - bot->GetPositionX();
            float dy = m_jumpTargetY - bot->GetPositionY();
            float dist2d = sqrtf(dx * dx + dy * dy);
            if (dist2d <= 0.5f)
            {
                m_pendingJump = false;
                if (m_jumpHere)
                {
                    StartJump(false);
                }
                else
                {
                    StartJump(true, m_jumpTargetO);
                }
            }
            else
            {
                // Ask once, not on every AI update. This used to re-issue the approach every
                // pass, stacking point generators and interrupting the leg it had just laid --
                // the same restart churn MovementAction::MoveTo now guards against, and for
                // the same reason: the client is handed a stream of cancelled splines.
                float destX, destY, destZ;
                const bool alreadyHeadingThere =
                    bot->GetMotionMaster()->GetCurrentMovementGeneratorType() == POINT_MOTION_TYPE &&
                    bot->GetMotionMaster()->GetDestination(destX, destY, destZ) &&
                    sqrtf((destX - m_jumpTargetX) * (destX - m_jumpTargetX) +
                          (destY - m_jumpTargetY) * (destY - m_jumpTargetY)) < 0.5f;

                if (!alreadyHeadingThere)
                {
                    bot->GetMotionMaster()->MovePoint(0, m_jumpTargetX, m_jumpTargetY, m_jumpTargetZ);
                }
            }
        }
        return;
    }

    if (!m_isJumping)
    {
        return;
    }

    uint32 now        = getMSTime();
    uint32 fallTimeMs = now - m_jumpStartTime;
    float  t          = fallTimeMs / 1000.f;

    float z = m_jumpStartZ + BOT_JUMP_VELOCITY * t - 0.5f * BOT_JUMP_GRAVITY * t * t;
    float x = m_jumpStartX + m_jumpCosAngle * m_jumpXYSpeed * t;
    float y = m_jumpStartY + m_jumpSinAngle * m_jumpXYSpeed * t;

    float maxDuration = 2.f * BOT_JUMP_VELOCITY / BOT_JUMP_GRAVITY * 1000.f + 100.f;
    bool  landed      = fallTimeMs > 200 && ((z <= m_jumpStartZ + 0.05f) || (float(fallTimeMs) >= maxDuration));

    bot->m_movementInfo.UpdateTime(now);
    bot->m_movementInfo.SetFallTime(fallTimeMs);
    bot->m_movementInfo.ChangePosition(x, y, z, bot->GetOrientation());

    if (landed)
    {
        m_isJumping = false;

        float landZ = m_jumpStartZ;
        if (Map* map = bot->GetMap())
        {
            float terrainZ = map->GetHeight(x, y, z > m_jumpStartZ ? z : m_jumpStartZ);
            if (terrainZ > INVALID_HEIGHT)
            {
                landZ = terrainZ;
            }
        }

        bot->m_movementInfo.RemoveMovementFlag(MovementFlags(MOVEFLAG_FALLING | MOVEFLAG_FORWARD));
        bot->m_movementInfo.ChangePosition(x, y, landZ, bot->GetOrientation());

        WorldPacket data(MSG_MOVE_FALL_LAND, 64);
        data << bot->GetPackGUID();
        bot->m_movementInfo.Write(data);
        bot->SendMessageToSet(&data, false);

        bot->SetFallInformation(fallTimeMs, landZ);
        bot->GetMotionMaster()->Clear();
        bot->GetMotionMaster()->MoveIdle();
        bot->GetMap()->PlayerRelocation(bot, x, y, landZ, bot->GetOrientation());
    }
}

void PlayerbotAI::UpdateAI(uint32 elapsed)
{
    if (bot->IsBeingTeleported())
    {
        return;
    }

    if (sPlayerbotAIConfig.randomBotActiveZoneOnly &&
        !bot->GetGroup() && sRandomPlayerbotMgr.IsRandomBot(bot) &&
        botOutgoingPacketHandlers.IsEmpty() &&
        !sRandomPlayerbotMgr.HasRealPlayerInZone(bot->GetZoneId()))
    {
        SetNextCheckDelay(5000);
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

    if (nextAICheckDelay > sPlayerbotAIConfig.maxWaitForMove &&
        !bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
    {
        if (bot->IsInCombat())
        {
            nextAICheckDelay = sPlayerbotAIConfig.maxWaitForMove;
        }
        else
        {
            Player* master = GetMaster();
            if (master && master->IsInCombat())
            {
                InterruptSpell();
                nextAICheckDelay = sPlayerbotAIConfig.maxWaitForMove;
            }
        }
    }

    if (m_drinkingUntil || m_eatingUntil)
    {
        if (bot->IsInCombat() || !bot->IsSitState())
        {
            m_drinkingUntil = 0;
            m_eatingUntil = 0;
        }
    }

    if (m_isJumping || m_pendingJump)
    {
        UpdateJump();
    }

    PlayerbotAIBase::UpdateAI(elapsed);
}

void PlayerbotAI::UpdateAIInternal(uint32 elapsed)
{
    // Repair the strategy set on the dead->alive transition FIRST, before anything else this
    // tick can run. Releasing applies "+stay", which evicts whichever movement sibling the bot
    // held, and nothing else puts it back -- so a bot that misses this repair is alive and
    // permanently parked.
    //
    // This was previously a test of `currentEngine == engines[BOT_STATE_DEAD]` performed AFTER
    // the engine had already executed, and that is bypassable: a queued master command drained
    // below is handled by the dead engine's chat strategy, an "attack" switches the current
    // engine to combat, and the later predicate then reads false. The repair never happened and
    // the bot kept the stale "stay" once combat ended. Latching the bot's OWN death state is
    // independent of which engine is current and of anything that mutates it later in the tick.
    if (!bot->IsAlive())
    {
        m_wasDead = true;
    }
    else if (m_wasDead)
    {
        m_wasDead = false;
        ResetStrategies();
    }

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

/**
 * Handles teleport acknowledgment for the bot.
 */
void PlayerbotAI::HandleTeleportAck()
{
    // Before anything else: a jump must not survive a teleport. See CancelJump.
    CancelJump();

    bot->GetMotionMaster()->Clear(true);
    bot->StopMoving(true);
    if (bot->IsBeingTeleportedNear())
    {
        WorldPacket p = WorldPacket(MSG_MOVE_TELEPORT_ACK, 8 + 4 + 4);
        p << bot->GetObjectGuid();
        p << (uint32)0; // supposed to be flags? not used currently
        p << (uint32)time(0); // time - not currently used
        bot->GetSession()->HandleMoveTeleportAckOpcode(p);

        // Tell the watchers. A near teleport moves the bot server-side and notifies NOBODY:
        // BuildTeleportAckMsg goes only to the mover's own session; DisableSpline runs before
        // StopMoving so MoveSplineInit::Stop early-returns and emits no stop packet; and
        // UpdateVisibilityOf is transition-only, so a hop that stays inside the hundred-yard
        // bubble is not a transition and sends nothing at all.
        //
        // An observer therefore keeps rendering the bot where it was. The 1.12 client does
        // not read the declared start of the next SMSG_MONSTER_MOVE -- it PREPENDS its own
        // rendered position to the path and then travels the stitched leg at
        // min(runSpeed * 4, length/duration). So the next ordinary step becomes a glide at
        // 28 yd/s across the whole teleport distance: measured here as 56.89 yards covered in
        // 2032 ms, where running it would take 8.13 seconds. No server packet carries that
        // motion, which is why every packet-level speed audit of it comes back clean.
        //
        // The heartbeat carries Where() -- the destination -- and the client writes it
        // straight into the slots it later reads as "current position", so the render snaps
        // and the next leg's stitch distance is zero. This is exactly what the core already
        // does for the creature branch of Unit::NearTeleportTo, and it is deliberately here
        // rather than in MovementHandler: a real client echoes its own movement after a
        // teleport, so only a client-less bot needs this.
        // Only if the ack was actually taken. HandleMoveTeleportAckOpcode validates against
        // the SESSION's mover, not the guid in the packet, and returns without doing
        // anything when that mover is not this bot -- which possession legitimately causes.
        // The near semaphore then stays set and UpdateSessions calls this again next tick,
        // so an unguarded heartbeat would announce a destination the bot has not been moved
        // to, and the resync would rebuild every observer's copy of it, every tick, for as
        // long as the possession lasts. The semaphore clearing is the ack's own receipt.
        if (!bot->IsBeingTeleportedNear())
        {
            bot->SendHeartBeat();
            ResyncObserversAfterTeleport();
        }
    }
    else if (bot->IsBeingTeleportedFar())
    {
        bot->GetSession()->HandleMoveWorldportAckOpcode();
    }

    LastMovement& movement = aiObjectContext->GetValue<LastMovement&>("last movement")->Get();
    if (movement.lastFollowState)
    {
        ChangeStrategy("+follow master,-stay", BOT_STATE_NON_COMBAT);
        movement.lastFollowState = false;
    }
}

/**
 * @brief Abandon any jump in progress or pending.
 *
 * A teleport during a simulated jump used only to PAUSE it -- UpdateAI returns early while
 * IsBeingTeleported() is true, and neither jump flag is cleared. After the ack the old
 * parabola simply resumes from its original start, and when it "lands" it relocates the bot
 * to jumpStart plus run speed times elapsed -- silently overwriting the destination the
 * teleport just put it at, with no observer told anything. Cancelling on both ack paths is
 * what stops a teleport being undone by physics that belonged to somewhere else.
 */
void PlayerbotAI::CancelJump()
{
    if (!m_isJumping && !m_pendingJump)
    {
        return;
    }

    m_isJumping   = false;
    m_pendingJump = false;

    bot->m_movementInfo.RemoveMovementFlag(MovementFlags(MOVEFLAG_FALLING | MOVEFLAG_FORWARD));
    bot->SetFallInformation(0, bot->GetPositionZ());
}

/**
 * @brief Bring the bot to a genuine halt. See the header for why Clear() alone does not.
 */
void PlayerbotAI::StopMovement()
{
    bot->GetMotionMaster()->Clear();
    bot->InterruptMoving(true);
    bot->GetMotionMaster()->MoveIdle();
}

/**
 * @brief Force every observer on this bot's map to re-sync its position after a teleport.
 *
 * The heartbeat above corrects observers who can see the DESTINATION. It cannot help one who
 * could see the bot where it was and cannot see where it went: SendMessageToSet broadcasts to
 * the visibility set of the new position, so an observer left behind never hears anything and
 * keeps rendering the bot at the old spot until something else happens to refresh it.
 *
 * Destroying it for anyone who currently holds it, then asking for a normal visibility
 * decision, covers all three cases in one pass: an observer who can only see the old position
 * gets a destroy and no re-create; one who can see both gets it back at the destination; one
 * who can only see the new position creates it there as usual.
 *
 * Ported from MaNGOS Three, which already carries this
 * (PlayerbotAI::ResyncObserversAfterTeleport, commit dd4037f26).
 */
void PlayerbotAI::ResyncObserversAfterTeleport()
{
    Map* map = bot->FindMap();
    if (!map)
    {
        return;
    }

    Map::PlayerList const& players = map->GetPlayers();
    for (Map::PlayerList::const_iterator itr = players.begin(); itr != players.end(); ++itr)
    {
        Player* observer = itr->getSource();
        if (!observer || observer == bot)
        {
            continue;
        }

        if (observer->HaveAtClient(bot))
        {
            bot->DestroyForPlayer(observer);
            observer->m_clientGUIDs.erase(bot->GetObjectGuid());
        }

        // Through the CAMERA, not the body. HaveAtClient above is a statement about what the
        // observer's current viewpoint holds, so the re-create has to be judged from that same
        // viewpoint -- an observer using farsight or bind-sight would otherwise be asked
        // whether its BODY can see the bot, and be told the wrong answer.
        observer->GetCamera().UpdateVisibilityOf(bot);
    }

    // Refresh the bot's own view too, which the Three original does and this port first
    // omitted: the bot has just arrived somewhere new and its own visibility set is stale.
    bot->UpdateVisibilityAndView();
}

/**
 * Resets the bot's state and strategies.
 */
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

/**
 * Handles a command from the master.
 * @param type The type of the command.
 * @param text The command text.
 * @param fromPlayer The player who sent the command.
 */
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

    // "who" used to skip the security check entirely, and it answers with the bot's class,
    // level, spec and gear. On a random bot that meant anyone at all could interrogate it,
    // and because a channel line is fanned out to every random bot in the world, one "~who"
    // in trade chat returned a whisper from each of them at once -- disclosure and a
    // whisper flood from a single message. It now needs TALK, the level the module already
    // uses to mean "may hold a conversation with this bot"; everything else still needs
    // full control.
    PlayerbotSecurityLevel required = (filtered.find("who") == 0)
        ? PLAYERBOT_SECURITY_TALK
        : PLAYERBOT_SECURITY_ALLOW_ALL;

    if (!GetSecurity()->CheckLevelFor(required, type != CHAT_MSG_WHISPER, &fromPlayer))
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

/**
 * Handles outgoing packets from the bot.
 * @param packet The packet to handle.
 */
void PlayerbotAI::HandleBotOutgoingPacket(const WorldPacket& packet)
{
    switch (packet.GetOpcode())
    {
        case SMSG_CAST_FAILED:
        {
            WorldPacket p(packet);
            p.rpos(0);
            uint32 spellId;
            p >> spellId;
            uint8 result = SPELL_CAST_OK;
            if (p.size() >= 6) // failure packet has status + result bytes
            {
                uint8 status;
                p >> status >> result;
            }
            if (result != SPELL_CAST_OK)
            {
                SpellInterrupted(spellId);
            }
            botOutgoingPacketHandlers.AddPacket(packet);
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
            break;
    }
}

/**
 * Handles spell interruption for the bot.
 * @param spellid The ID of the interrupted spell.
 */
void PlayerbotAI::SpellInterrupted(uint32 spellid)
{
    if (!spellid)
    {
        return;
    }

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

/**
 * Calculates the global cooldown for a spell.
 * @param spellid The ID of the spell.
 * @return The global cooldown in milliseconds.
 */
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

/**
 * Handles incoming packets from the master.
 * @param packet The packet to handle.
 */
void PlayerbotAI::HandleMasterIncomingPacket(const WorldPacket& packet)
{
    masterIncomingPacketHandlers.AddPacket(packet);
}

/**
 * Handles outgoing packets to the master.
 * @param packet The packet to handle.
 */
void PlayerbotAI::HandleMasterOutgoingPacket(const WorldPacket& packet)
{
    masterOutgoingPacketHandlers.AddPacket(packet);
}

/**
 * Changes the current engine to the specified type.
 * @param type The type of the engine.
 */
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

/**
 * Executes the next action for the bot.
 */
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
        // Engine selection only. The strategy repair that coming back to life requires is done
        // at the top of UpdateAIInternal against a latched death state, because this predicate
        // reads the current engine pointer AFTER the engine has run and can be bypassed by
        // anything that switches engines earlier in the tick.
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

/**
 * Reinitializes the current engine.
 */
void PlayerbotAI::ReInitCurrentEngine()
{
    InterruptSpell();
    currentEngine->Init();
}

/**
 * Changes the strategy for the specified engine type.
 * @param names The names of the strategies.
 * @param type The type of the engine.
 */
void PlayerbotAI::ChangeStrategy(string names, BotState type)
{
    Engine* e = engines[type];
    if (!e)
    {
        return;
    }

    e->ChangeStrategy(names);
}

/**
 * Executes a specific action for the bot.
 * @param name The name of the action.
 */
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

/**
 * Checks if the bot contains a specific strategy.
 * @param type The type of the strategy.
 * @return True if the strategy is contained, false otherwise.
 */
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

/**
 * Checks if the bot has a specific strategy.
 * @param name The name of the strategy.
 * @param type The type of the engine.
 * @return True if the strategy is present, false otherwise.
 */
bool PlayerbotAI::HasStrategy(const string& name, BotState type)
{
    return engines[type]->HasStrategy(name);
}

/**
 * Resets the strategies for the bot.
 */
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

/**
 * Checks if the player is a ranged class.
 * @param player The player to check.
 * @return True if the player is a ranged class, false otherwise.
 */
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

/**
 * Checks if the player is a tank class.
 * @param player The player to check.
 * @return True if the player is a tank class, false otherwise.
 */
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

/**
 * Returns the tank player in a given player's group.
 * @param except The player to check.
 * @return the tank in the group, or null
 */
Player* PlayerbotAI::GetGroupTank(Player* except)
{
    Group* group = except->GetGroup();
    if (!group)
    {
        return nullptr;
    }

    Group::MemberSlotList const& slots = group->GetMemberSlots();
    if (slots.size() < 5)
    {
        return nullptr;
    }

    for (auto itr = slots.begin(); itr != slots.end(); ++itr)
    {
        Player* member = sObjectMgr.GetPlayer(itr->guid);
        if (member && member != except && IsTank(member))
        {
            return member;
        }
    }
    return nullptr;
}

/**
 * Checks if the player is a healer class.
 * @param player The player to check.
 * @return True if the player is a healer class, false otherwise.
 */
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

bool PlayerbotAI::HasBreakableCrowdControl(Unit* unit)
{
    if (!unit || !unit->IsAlive())
    {
        return false;
    }

    if (unit->HasAuraType(SPELL_AURA_MOD_CHARM) ||
        unit->HasAuraType(SPELL_AURA_TRANSFORM) ||
        unit->HasAuraType(SPELL_AURA_MOD_PACIFY))
    {
        return true;
    }

    if (unit->IsFeared() || unit->IsInRoots() || unit->IsPolymorphed())
    {
        return true;
    }

    static const uint32 breakableCcMechanicMask =
        (1 << (MECHANIC_SAPPED - 1)) |
        (1 << (MECHANIC_FREEZE - 1)) |
        (1 << (MECHANIC_BANISH - 1)) |
        (1 << (MECHANIC_SHACKLE - 1)) |
        (1 << (MECHANIC_HORROR - 1)) |
        (1 << (MECHANIC_SLEEP - 1)) |
        (1 << (MECHANIC_TURN - 1)) |
        (1 << (MECHANIC_DAZE - 1)) |
        (1 << (MECHANIC_POLYMORPH - 1));

    Unit::SpellAuraHolderMap const& auras = unit->GetSpellAuraHolderMap();
    for (Unit::SpellAuraHolderMap::const_iterator itr = auras.begin();
         itr != auras.end(); ++itr)
    {
        if (itr->second->HasMechanicMask(breakableCcMechanicMask))
        {
            return true;
        }
    }

    return false;
}

/*
 * Uses the bots in-range unfriendlys as a pool to determine if
 * the given center point (or the bots position) has any unfriendlys
 * within the given range of that point. Such npcs would have to be
 * non-combatants, and non-cced.
 */
bool PlayerbotAI::HasNonCombatantInRange(float range,
    float centerX, float centerY, float centerZ)
{
    // Find nearby unfriendly units using grid search
    list<Unit*> targets;
    MaNGOS::AnyUnfriendlyUnitInObjectRangeCheck u_check(bot, range);
    MaNGOS::UnitListSearcher<MaNGOS::AnyUnfriendlyUnitInObjectRangeCheck> searcher(targets, u_check);
    Cell::VisitAllObjects(bot, searcher, range);

    for (list<Unit*>::iterator i = targets.begin(); i != targets.end(); ++i)
    {
        Unit* unit = *i;
        if (!unit || !unit->IsAlive())
        {
            continue;
        }
        // Check distance from center point (bot position by default, or explicit coords)
        float dist;
        if (centerX != 0 || centerY != 0 || centerZ != 0)
        {
            float dx = unit->GetPositionX() - centerX;
            float dy = unit->GetPositionY() - centerY;
            float dz = unit->GetPositionZ() - centerZ;
            dist = sqrt(dx * dx + dy * dy + dz * dz);
        }
        else
        {
            dist = bot->GetDistance(unit);
        }
        if (dist > range || unit->IsStunned())
        {
            continue;
        }
        if (HasBreakableCrowdControl(unit) || !unit->getVictim()) // do not aggro, or break cc
        {
            return true;
        }
    }
    return false;
}

namespace MaNGOS
{

    /**
     * Checks if a unit is within range based on its GUID.
     */
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

    /**
     * Checks if a game object is within range based on its GUID.
     */
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

/**
 * Retrieves a unit based on its GUID.
 * @param guid The GUID of the unit.
 * @return The unit, or NULL if not found.
 */
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

/**
 * Retrieves a creature based on its GUID.
 * @param guid The GUID of the creature.
 * @return The creature, or NULL if not found.
 */
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

    for (list<Unit *>::iterator i = targets.begin(); i != targets.end(); i++)
    {
        Creature* creature = dynamic_cast<Creature*>(*i);
        if (creature)
        {
            return creature;
        }
    }

    return NULL;
}

/**
 * Retrieves a game object based on its GUID.
 * @param guid The GUID of the game object.
 * @return The game object, or NULL if not found.
 */
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

    for (list<GameObject*>::iterator i = targets.begin(); i != targets.end(); i++)
    {
        GameObject* go = *i;
        if (go && go->isSpawned())
        {
            return go;
        }
    }

    return NULL;
}

/**
 * Sends a message to the master without facing the master.
 * @param text The message text.
 * @param securityLevel The required security level.
 * @return True if the message was sent, false otherwise.
 */
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
    {
        return false;
    }

    bot->Whisper(text, LANG_UNIVERSAL, master->GetObjectGuid());
    return true;
}

/**
 * Sends a message to the master.
 * @param text The message text.
 * @param securityLevel The required security level.
 * @return True if the message was sent, false otherwise.
 */
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

/**
 * Checks if an aura is real.
 * @param bot The bot.
 * @param aura The aura to check.
 * @param unit The unit with the aura.
 * @return True if the aura is real, false otherwise.
 */
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
    if (stacks >= aura->GetHolder()->GetSpellProto()->CumulativeAura)
    {
        return true;
    }

    if (aura->GetHolder()->GetCaster() == bot || aura->GetHolder()->IsPositive() || aura->GetHolder()->IsAreaAura())
    {
        return true;
    }

    return false;
}

/**
 * Checks if a unit has a specific aura.
 * @param name The name of the aura.
 * @param unit The unit to check.
 * @return True if the unit has the aura, false otherwise.
 */
bool PlayerbotAI::HasAura(string name, Unit* unit)
{
    if (!unit)
    {
        return false;
    }

    uint32 spellId = aiObjectContext->GetValue<uint32>("spell id", name)->Get();
    if (spellId && HasAura(spellId, unit))
    {
        return true;
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

            const string auraName = aura->GetSpellProto()->Name_lang[0];
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

/**
 * Checks if a unit has a specific aura.
 * @param spellId The ID of the aura.
 * @param unit The unit to check.
 * @return True if the unit has the aura, false otherwise.
 */
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

/**
 * Checks if a unit has any of the specified auras.
 * @param player The unit to check.
 * @param ... The list of aura names.
 * @return True if the unit has any of the auras, false otherwise.
 */
bool PlayerbotAI::HasAnyAuraOf(Unit* player, ...)
{
    if (!player)
    {
        return false;
    }

    va_list vl;
    va_start(vl, player);

    const char* cur;
    do
    {
        cur = va_arg(vl, const char*);
        if (cur && HasAura(cur, player))
        {
            va_end(vl);
            return true;
        }
    }
    while (cur);

    va_end(vl);
    return false;
}

/**
 * Checks if the bot can cast a spell on a target.
 * @param name The name of the spell.
 * @param target The target unit.
 * @return True if the spell can be cast, false otherwise.
 */
bool PlayerbotAI::CanCastSpell(string name, Unit* target)
{
    return CanCastSpell(aiObjectContext->GetValue<uint32>("spell id", name)->Get(), target);
}

/**
 * Checks if the bot can cast a spell on a target.
 * @param spellid The ID of the spell.
 * @param target The target unit.
 * @param checkHasSpell Whether to check if the bot has the spell.
 * @return True if the spell can be cast, false otherwise.
 */
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
    Spell *spell = new Spell(bot, spellInfo, false);

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

/**
 * Casts a spell on a target.
 * @param name The name of the spell.
 * @param target The target unit.
 * @return True if the spell was cast, false otherwise.
 */
bool PlayerbotAI::CastSpell(string name, Unit* target)
{
    bool result = CastSpell(aiObjectContext->GetValue<uint32>("spell id", name)->Get(), target);
    if (result)
    {
        aiObjectContext->GetValue<time_t>("last spell cast time", name)->Set(time(0));
    }

    return result;
}

/**
 * Casts a spell on a target.
 * @param spellId The ID of the spell.
 * @param target The target unit.
 * @return True if the spell was cast, false otherwise.
 */
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
        // bot wants to cast, but mm is still active; don't reject, just STOP!
        bot->StopMoving(false);
        mm.MoveIdle();
    }

    if (bot->IsTaxiFlying())
    {
        return false;
    }

    bot->clearUnitState(UNIT_STAT_CHASE);
    bot->clearUnitState(UNIT_STAT_FOLLOW);

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
            delete spell;
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
        delete spell;
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

/**
 * Interrupts the current spell being cast by the bot.
 */
void PlayerbotAI::InterruptSpell()
{
    Spell* autoRepeat = bot->GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL);
    if (autoRepeat)
    {
        bot->InterruptSpell(CURRENT_AUTOREPEAT_SPELL);
    }

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
        data << uint32(spell->m_spellInfo->ID);
        data << uint8(0);
        bot->SendMessageToSet(&data, true);

        data.Initialize(SMSG_SPELL_FAILED_OTHER, 8 + 1 + 4 + 1);
        data << bot->GetObjectGuid();
        data << uint8(1);
        data << uint32(spell->m_spellInfo->ID);
        data << uint8(0);
        bot->SendMessageToSet(&data, true);

        SpellInterrupted(spell->m_spellInfo->ID);
    }

    SpellInterrupted(lastSpell.id);
}

/**
 * Removes an aura from the bot.
 * @param name The name of the aura.
 */
void PlayerbotAI::RemoveAura(string name)
{
    uint32 spellid = aiObjectContext->GetValue<uint32>("spell id", name)->Get();
    if (spellid && HasAura(spellid, bot))
    {
        bot->RemoveAurasDueToSpell(spellid);
    }
}

/**
 * Checks if a spell being cast by a target can be interrupted.
 * @param target The target unit.
 * @param spell The name of the spell.
 * @return True if the spell can be interrupted, false otherwise.
 */
bool PlayerbotAI::IsInterruptableSpellCasting(Unit* target, string spell)
{
    uint32 spellid = aiObjectContext->GetValue<uint32>("spell id", spell)->Get();
    if (!spellid || !target->IsNonMeleeSpellCasted(true))
    {
        return false;
    }

    SpellEntry const *spellInfo = sSpellStore.LookupEntry(spellid);
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
        {
            return true;
        }
    }

    return false;
}

/**
 * Checks if a unit has an aura that can be dispelled.
 * @param target The target unit.
 * @param dispelType The type of dispel.
 * @return True if the unit has an aura that can be dispelled, false otherwise.
 */
bool PlayerbotAI::HasAuraToDispel(Unit* target, uint32 dispelType)
{
    // Iterate through all aura types
    for (uint32 type = SPELL_AURA_NONE; type < TOTAL_AURAS; ++type)
    {
        // Get the list of auras of the current type
        Unit::AuraList const& auras = target->GetAurasByType((AuraType)type);
        for (Unit::AuraList::const_iterator itr = auras.begin(); itr != auras.end(); ++itr)
        {
            const Aura* aura = *itr;
            const SpellEntry* entry = aura->GetSpellProto();
            uint32 spellId = entry->ID;

            // Check if the spell is positive or negative
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

/**
 * Case-insensitive string comparison.
 * @param s1 The first string.
 * @param s2 The second string.
 * @return The difference between the first non-matching characters.
 */
inline int strcmpi(const char* s1, const char* s2)
{
    for (; *s1 && *s2 && (toupper(*s1) == toupper(*s2)); ++s1, ++s2);
    {
        return *s1 - *s2;
    }
}
#endif

/**
 * Checks if a spell can be dispelled.
 * @param entry The spell entry.
 * @param dispelType The type of dispel.
 * @return True if the spell can be dispelled, false otherwise.
 */
bool PlayerbotAI::canDispel(const SpellEntry* entry, uint32 dispelType)
{
    if (entry->DispelType != dispelType)
    {
        return false;
    }

    // Check if the spell name matches any of the known non-dispellable spells
    return !entry->Name_lang[0] ||
        (strcmpi((const char*)entry->Name_lang[0], "demon skin") &&
        strcmpi((const char*)entry->Name_lang[0], "mage armor") &&
        strcmpi((const char*)entry->Name_lang[0], "frost armor") &&
        strcmpi((const char*)entry->Name_lang[0], "wavering will") &&
        strcmpi((const char*)entry->Name_lang[0], "chilled") &&
        strcmpi((const char*)entry->Name_lang[0], "ice armor"));
}

/**
 * Checks if a race is part of the Alliance faction.
 * @param race The race to check.
 * @return True if the race is part of the Alliance, false otherwise.
 */
bool IsAlliance(uint8 race)
{
    return race == RACE_HUMAN || race == RACE_DWARF || race == RACE_NIGHTELF ||
#if !defined(CLASSIC)
        race == RACE_DRAENEI || race == RACE_BLOODELF ||
#endif
        race == RACE_GNOME;
}

/**
 * Checks if a player is from an opposing faction.
 * @param player The player to check.
 * @return True if the player is from an opposing faction, false otherwise.
 */
bool PlayerbotAI::IsOpposing(Player* player)
{
    return IsOpposing(player->getRace(), bot->getRace());
}

/**
 * Checks if two races are from opposing factions.
 * @param race1 The first race.
 * @param race2 The second race.
 * @return True if the races are from opposing factions, false otherwise.
 */
bool PlayerbotAI::IsOpposing(uint8 race1, uint8 race2)
{
    return (IsAlliance(race1) && !IsAlliance(race2)) || (!IsAlliance(race1) && IsAlliance(race2));
}

/**
 * Removes all shapeshift forms from the bot.
 */
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
