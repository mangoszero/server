/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2025 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include "Player.h"
#include "Language.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "Opcodes.h"
#include "SpellMgr.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "UpdateMask.h"
#include "CinematicFlyover.h"
#include "QuestDef.h"
#include "GossipDef.h"
#include "UpdateData.h"
#include "Channel.h"
#include "ChannelMgr.h"
#include "MapManager.h"
#include "MapPersistentStateMgr.h"
#include "InstanceData.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "ObjectMgr.h"
#include "ObjectAccessor.h"
#include "Formulas.h"
#include "Group.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Pet.h"
#include "Util.h"
#include "Transports.h"
#include "Weather.h"
#include "BattleGround/BattleGround.h"
#include "BattleGround/BattleGroundMgr.h"
#include "BattleGround/BattleGroundAV.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "Chat.h"
#include "revision_data.h"
#include "Database/DatabaseImpl.h"
#include "Spell.h"
#include "ScriptMgr.h"
#include "SocialMgr.h"
#include "Mail.h"
#include "DBCStores.h"
#include "SQLStorages.h"
#include "DisableMgr.h"
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */

#ifdef ENABLE_PLAYERBOTS
#include "playerbot.h"
#endif

#include <cmath>

#define ZONE_UPDATE_INTERVAL (1*IN_MILLISECONDS)

#define PLAYER_SKILL_INDEX(x)       (PLAYER_SKILL_INFO_1_1 + ((x)*3))
#define PLAYER_SKILL_VALUE_INDEX(x) (PLAYER_SKILL_INDEX(x)+1)
#define PLAYER_SKILL_BONUS_INDEX(x) (PLAYER_SKILL_INDEX(x)+2)

#define MAKE_SKILL_VALUE(v, m) MAKE_PAIR32(v,m)


enum CharacterFlags
{
    CHARACTER_FLAG_NONE                 = 0x00000000,
    CHARACTER_FLAG_UNK1                 = 0x00000001,
    CHARACTER_FLAG_UNK2                 = 0x00000002,
    CHARACTER_LOCKED_FOR_TRANSFER       = 0x00000004,
    CHARACTER_FLAG_UNK4                 = 0x00000008,
    CHARACTER_FLAG_UNK5                 = 0x00000010,
    CHARACTER_FLAG_UNK6                 = 0x00000020,
    CHARACTER_FLAG_UNK7                 = 0x00000040,
    CHARACTER_FLAG_UNK8                 = 0x00000080,
    CHARACTER_FLAG_UNK9                 = 0x00000100,
    CHARACTER_FLAG_UNK10                = 0x00000200,
    CHARACTER_FLAG_HIDE_HELM            = 0x00000400,
    CHARACTER_FLAG_HIDE_CLOAK           = 0x00000800,
    CHARACTER_FLAG_UNK13                = 0x00001000,
    CHARACTER_FLAG_GHOST                = 0x00002000,
    CHARACTER_FLAG_RENAME               = 0x00004000,
    CHARACTER_FLAG_UNK16                = 0x00008000,
    CHARACTER_FLAG_UNK17                = 0x00010000,
    CHARACTER_FLAG_UNK18                = 0x00020000,
    CHARACTER_FLAG_UNK19                = 0x00040000,
    CHARACTER_FLAG_UNK20                = 0x00080000,
    CHARACTER_FLAG_UNK21                = 0x00100000,
    CHARACTER_FLAG_UNK22                = 0x00200000,
    CHARACTER_FLAG_UNK23                = 0x00400000,
    CHARACTER_FLAG_UNK24                = 0x00800000,
    CHARACTER_FLAG_LOCKED_BY_BILLING    = 0x01000000,
    CHARACTER_FLAG_DECLINED             = 0x02000000,
    CHARACTER_FLAG_UNK27                = 0x04000000,
    CHARACTER_FLAG_UNK28                = 0x08000000,
    CHARACTER_FLAG_UNK29                = 0x10000000,
    CHARACTER_FLAG_UNK30                = 0x20000000,
    CHARACTER_FLAG_UNK31                = 0x40000000,
    CHARACTER_FLAG_UNK32                = 0x80000000
};

#define MAX_DEATH_COUNT 3


//== PlayerTaxi ================================================

PlayerTaxi::PlayerTaxi()
{
    // Taxi nodes
    memset(m_taximask, 0, sizeof(m_taximask));
}

/**
 * @brief Initializes known taxi nodes for a newly created player.
 *
 * @param race The player race id.
 * @param level Unused player level.
 */
void PlayerTaxi::InitTaxiNodes(uint32 race, uint32 /*level*/)
{
    memset(m_taximask, 0, sizeof(m_taximask));
    // capital and taxi hub masks
    ChrRacesEntry const* rEntry = sChrRacesStore.LookupEntry(race);
    m_taximask[0] = rEntry->startingTaxiMask;
}






/**
 * @brief Serializes the player's discovered taxi mask into a stream.
 *
 * @param ss The destination output stream.
 * @param taxi The taxi data to serialize.
 * @return The output stream.
 */
std::ostringstream& operator<< (std::ostringstream& ss, PlayerTaxi const& taxi)
{
    for (int i = 0; i < TaxiMaskSize; ++i)
    {
        ss << taxi.m_taximask[i] << " ";
    }
    return ss;
}

/**
 * @brief Builds a spell modifier from a spell effect definition.
 *
 * @param _op The spell modifier operation.
 * @param _type The spell modifier type.
 * @param _value The modifier value.
 * @param spellEntry The source spell entry.
 * @param eff The spell effect index.
 * @param _charges The initial charge count.
 */
SpellModifier::SpellModifier(SpellModOp _op, SpellModType _type, int32 _value, SpellEntry const* spellEntry, SpellEffectIndex eff, int16 _charges /*= 0*/) : op(_op), type(_type), charges(_charges), value(_value), spellId(spellEntry->Id), lastAffected(NULL)
{
    mask = sSpellMgr.GetSpellAffectMask(spellEntry->Id, eff);
}

/**
 * @brief Builds a spell modifier from an aura source.
 *
 * @param _op The spell modifier operation.
 * @param _type The spell modifier type.
 * @param _value The modifier value.
 * @param aura The aura providing the modifier.
 * @param _charges The initial charge count.
 */
SpellModifier::SpellModifier(SpellModOp _op, SpellModType _type, int32 _value, Aura const* aura, int16 _charges /*= 0*/) : op(_op), type(_type), charges(_charges), value(_value), spellId(aura->GetId()), lastAffected(NULL)
{
    mask = sSpellMgr.GetSpellAffectMask(aura->GetId(), aura->GetEffIndex());
}

/**
 * @brief Checks whether this modifier affects a given spell.
 *
 * @param spell The spell to test.
 * @return true if the modifier applies to the spell; otherwise, false.
 */
bool SpellModifier::isAffectedOnSpell(SpellEntry const* spell) const
{
    SpellEntry const* affect_spell = sSpellStore.LookupEntry(spellId);
    // False if affect_spell == NULL or spellFamily not equal
    if (!affect_spell || affect_spell->SpellFamilyName != spell->SpellFamilyName)
    {
        return false;
    }
    return spell->IsFitToFamilyMask(mask);
}

//== TradeData =================================================

TradeData* TradeData::GetTraderData() const
{
    return m_trader->GetTradeData();
}

/**
 * @brief Gets the item placed in a trade slot.
 *
 * @param slot The trade slot.
 * @return The item in the slot, or null if empty.
 */
Item* TradeData::GetItem(TradeSlots slot) const
{
    return m_items[slot] ? m_player->GetItemByGuid(m_items[slot]) : NULL;
}

/**
 * @brief Checks whether a specific item is part of the current trade.
 *
 * @param item_guid The item GUID to test.
 * @return true if the item is present in a trade slot; otherwise, false.
 */
bool TradeData::HasItem(ObjectGuid item_guid) const
{
    for (int i = 0; i < TRADE_SLOT_COUNT; ++i)
    {
        if (m_items[i] == item_guid)
        {
            return true;
        }
    }
    return false;
}

/**
 * @brief Gets the item used as the current trade spell reagent.
 *
 * @return The spell-cast item, or null if none is set.
 */
Item* TradeData::GetSpellCastItem() const
{
    return m_spellCastItem ?  m_player->GetItemByGuid(m_spellCastItem) : NULL;
}

/**
 * @brief Sets the item assigned to a trade slot and refreshes trade state.
 *
 * @param slot The trade slot to update.
 * @param item The item to place in the slot, or null to clear it.
 */
void TradeData::SetItem(TradeSlots slot, Item* item)
{
    ObjectGuid itemGuid = item ? item->GetObjectGuid() : ObjectGuid();

    if (m_items[slot] == itemGuid)
    {
        return;
    }

    m_items[slot] = itemGuid;

    SetAccepted(false);
    GetTraderData()->SetAccepted(false);

    Update();

    // need remove possible trader spell applied to changed item
    if (slot == TRADE_SLOT_NONTRADED)
    {
        GetTraderData()->SetSpell(0);
    }

    // need remove possible player spell applied (possible move reagent)
    SetSpell(0);
}

/**
 * @brief Sets the spell cast through the trade window.
 *
 * @param spell_id The spell identifier.
 * @param castItem The optional reagent item used for the spell.
 */
void TradeData::SetSpell(uint32 spell_id, Item* castItem /*= NULL*/)
{
    ObjectGuid itemGuid = castItem ? castItem->GetObjectGuid() : ObjectGuid();

    if (m_spell == spell_id && m_spellCastItem == itemGuid)
    {
        return;
    }

    m_spell = spell_id;
    m_spellCastItem = itemGuid;

    SetAccepted(false);
    GetTraderData()->SetAccepted(false);

    Update(true);                                           // send spell info to item owner
    Update(false);                                          // send spell info to caster self
}

/**
 * @brief Sets the trade money offer and refreshes trade state.
 *
 * @param money The offered money amount.
 */
void TradeData::SetMoney(uint32 money)
{
    if (m_money == money)
    {
        return;
    }

    if (money > m_player->GetMoney())
    {
        TradeStatusInfo info;
        info.Status = TRADE_STATUS_CLOSE_WINDOW;
        info.Result = EQUIP_ERR_NOT_ENOUGH_MONEY;
        m_player->GetSession()->SendTradeStatus(info);
        return;
    }

    m_money = money;

    SetAccepted(false);
    GetTraderData()->SetAccepted(false);

    Update();
}

/**
 * @brief Sends the current trade state to one side of the trade.
 *
 * @param for_trader true to update the trader view; false to update the player view.
 */
void TradeData::Update(bool for_trader /*= true*/)
{
    if (for_trader)
    {
        m_trader->GetSession()->SendUpdateTrade(true); // player state for trader
    }
    else
    {
        m_player->GetSession()->SendUpdateTrade(false); // player state for player
    }
}

/**
 * @brief Sets the accepted state for the trade and optionally notifies the other trader.
 *
 * @param state The new accepted state.
 * @param crosssend true to send the status to the trader instead of the owner.
 */
void TradeData::SetAccepted(bool state, bool crosssend /*= false*/)
{
    m_accepted = state;

    if (!state)
    {
        TradeStatusInfo info;
        info.Status = TRADE_STATUS_BACK_TO_TRADE;
        if (crosssend)
        {
            m_trader->GetSession()->SendTradeStatus(info);
        }
        else
        {
            m_player->GetSession()->SendTradeStatus(info);
        }
    }
}

//== Player ====================================================

UpdateMask Player::updateVisualBits;

/**
 * @brief Initializes a player instance and its runtime state.
 *
 * @param session The owning world session.
 */
Player::Player(WorldSession* session): Unit(), m_mover(this), m_camera(this), m_reputationMgr(this)
{
#ifdef ENABLE_PLAYERBOTS
    m_playerbotAI = 0;
    m_playerbotMgr = 0;
#endif

    m_transport = 0;

    m_speakTime = 0;
    m_speakCount = 0;

    m_visibilityObserverSweepTimer = World::GetVisibilityObserverSweepInterval();

    m_objectType |= TYPEMASK_PLAYER;
    m_objectTypeId = TYPEID_PLAYER;

    m_valuesCount = PLAYER_END;

    SetActiveObjectState(true);                             // player is always active object

    m_session = session;

    m_ExtraFlags = 0;
    if (GetSession()->GetSecurity() >= SEC_GAMEMASTER)
    {
        SetAcceptTicket(true);
    }

    // players always accept
    if (GetSession()->GetSecurity() == SEC_PLAYER)
    {
        SetAcceptWhispers(true);
    }

    m_comboPoints = 0;

    m_usedTalentCount = 0;

    m_modManaRegen = 0;
    m_modManaRegenInterrupt = 0;

    m_rageDecayRate = 1.25f;
    m_rageDecayMultiplier = 19.50f;

    for (int s = 0; s < MAX_SPELL_SCHOOL; s++)
    {
        m_SpellCritPercentage[s] = 0.0f;
    }
    m_regenTimer = 0;
    m_weaponChangeTimer = 0;

    m_zoneUpdateId = 0;
    m_zoneUpdateTimer = 0;
    m_positionStatusUpdateTimer = 0;

    m_areaUpdateId = 0;

    m_nextSave = sWorld.getConfig(CONFIG_UINT32_INTERVAL_SAVE);

    // randomize first save time in range [CONFIG_UINT32_INTERVAL_SAVE] around [CONFIG_UINT32_INTERVAL_SAVE]
    // this must help in case next save after mass player load after server startup
    m_nextSave = urand(m_nextSave / 2, m_nextSave * 3 / 2);

    clearResurrectRequestData();

    m_SpellModRemoveCount = 0;

    memset(m_items, 0, sizeof(Item*)*PLAYER_SLOTS_COUNT);

    m_social = NULL;

    // group is initialized in the reference constructor
    SetGroupInvite(NULL);
    m_groupUpdateMask = 0;
    m_auraUpdateMask = 0;

    ClearHonorInfo();

    duel = NULL;

    m_GuildIdInvited = 0;

    m_atLoginFlags = AT_LOGIN_NONE;

    mSemaphoreTeleport_Near = false;
    mSemaphoreTeleport_Far = false;

    m_DelayedOperations = 0;
    m_bCanDelayTeleport = false;
    m_bHasDelayedTeleport = false;
    m_bHasBeenAliveAtDelayedTeleport = true;                // overwrite always at setup teleport data, so not used infact
    m_teleport_options = 0;

    m_trade = NULL;

    m_cinematic = 0;

    PlayerTalkClass = new PlayerMenu(GetSession());
    m_currentBuybackSlot = BUYBACK_SLOT_START;

    m_lastLiquid = NULL;

    for (int i = 0; i < MAX_TIMERS; ++i)
    {
        m_MirrorTimer[i] = DISABLED_MIRROR_TIMER;
    }

    m_MirrorTimerFlags = UNDERWATER_NONE;
    m_MirrorTimerFlagsLast = UNDERWATER_NONE;

    m_isInWater = false;
    m_drunkTimer = 0;
    m_drunk = 0;
    m_restTime = 0;
    m_deathTimer = 0;
    // Initialize death expire time to 0
    m_deathExpireTime = 0;

    // Initialize swing error message to 0
    m_swingErrorMsg = 0;

    // Initialize detection invisibility timer to 1 millisecond
    m_DetectInvTimer = 1 * IN_MILLISECONDS;

    // Initialize battleground queue IDs and invited instances
    for (int j = 0; j < PLAYER_MAX_BATTLEGROUND_QUEUES; ++j)
    {
        m_bgBattleGroundQueueID[j].bgQueueTypeId  = BATTLEGROUND_QUEUE_NONE;
        m_bgBattleGroundQueueID[j].invitedToInstance = 0;
    }

    // Set login time to current time
    m_logintime = time(NULL);
    // Set last tick time to login time
    m_Last_tick = m_logintime;
    // Initialize weapon proficiency to 0
    m_WeaponProficiency = 0;
    // Initialize armor proficiency to 0
    m_ArmorProficiency = 0;
    // Initialize parry ability to false
    m_canParry = false;
    // Initialize block ability to false
    m_canBlock = false;
    // Initialize dual wield ability to false
    m_canDualWield = false;
    m_ammoDPSMin = 0.0f;
    m_ammoDPSMax = 0.0f;

    // Initialize temporary unsummoned pet number to 0
    m_temporaryUnsummonedPetNumber = 0;

    //////////////////// Rest System/////////////////////
    // Initialize time of entering inn to 0
    time_inn_enter = 0;
    // Initialize inn trigger ID to 0
    inn_trigger_id = 0;
    // Initialize rest bonus to 0
    m_rest_bonus = 0;
    // Initialize rest type to no rest
    rest_type = REST_TYPE_NO;
    //////////////////// Rest System/////////////////////

    // Initialize mails updated flag to false
    m_mailsUpdated = false;
    // Initialize unread mails count to 0
    unReadMails = 0;
    // Initialize next mail delivery time to 0
    m_nextMailDelivereTime = 0;

    // Initialize reset talents cost to 0
    m_resetTalentsCost = 0;
    // Initialize reset talents time to 0
    m_resetTalentsTime = 0;
    // Initialize item update queue blocked flag to false
    m_itemUpdateQueueBlocked = false;

    // Initialize forced speed changes for all move types to 0
    for (int i = 0; i < MAX_MOVE_TYPE; ++i)
    {
        m_forced_speed_changes[i] = 0;
    }

    // Initialize stable slots to 0
    m_stableSlots = 0;

    /////////////////// Instance System /////////////////////
    // Initialize homebind timer to 0
    m_HomebindTimer = 0;
    // Initialize instance validity to true
    m_InstanceValid = true;

    // Initialize aura base modifiers
    for (int i = 0; i < BASEMOD_END; ++i)
    {
        m_auraBaseMod[i][FLAT_MOD] = 0.0f;
        m_auraBaseMod[i][PCT_MOD] = 1.0f;
    }

    // Player summoning
    // Initialize summon expire time to 0
    m_summon_expire = 0;
    // Initialize summon map ID to 0
    m_summon_mapid = 0;
    // Initialize summon coordinates to (0.0f, 0.0f, 0.0f)
    m_summon_x = 0.0f;
    m_summon_y = 0.0f;
    m_summon_z = 0.0f;

    // Initialize contested PvP timer to 0
    m_contestedPvPTimer = 0;

    m_lastFallTime = 0;
    // Initialize last fall Z coordinate to 0
    m_lastFallZ = 0;
#ifdef ENABLE_PLAYERBOTS
    // Initialize player bot AI to NULL
    m_playerbotAI = NULL;
    // Initialize player bot manager to NULL
    m_playerbotMgr = NULL;
#endif

}

/**
 * @brief Destroys the player and releases owned resources.
 */
Player::~Player()
{
    // Perform cleanup before deleting the player object
    CleanupsBeforeDelete();

    // Ensure the social object is unloaded (should already be done in PlayerLogout)
    // m_social = NULL;

    // Delete all items in the player's inventory
    for (int i = 0; i < PLAYER_SLOTS_COUNT; ++i)
    {
        delete m_items[i];
    }

    // Clean up communication channels
    CleanupChannels();

    // Delete all mailed items and deallocate mail objects
    for (PlayerMails::const_iterator itr = m_mail.begin(); itr != m_mail.end(); ++itr)
    {
        delete *itr;
    }

    // Delete all items in the player's item map
    for (ItemMap::const_iterator iter = mMitems.begin(); iter != mMitems.end(); ++iter)
    {
        delete iter->second; // Ensure no duplicated items to avoid crashes
    }

    // Delete the player's talk class
    delete PlayerTalkClass;

    // Remove the player from any transport they are on
    if (m_transport)
    {
        m_transport->RemovePassenger(this);
    }

    // Delete all item set effects
    for (size_t x = 0; x < ItemSetEff.size(); ++x)
    {
        delete ItemSetEff[x];
    }

#ifdef ENABLE_PLAYERBOTS
    // Delete player bot AI and manager if they exist
    if (m_playerbotAI)
    {
        delete m_playerbotAI;
        m_playerbotAI = 0;
    }
    if (m_playerbotMgr)
    {
        delete m_playerbotMgr;
        m_playerbotMgr = 0;
    }
#endif

    // clean up player-instance binds, may unload some instance saves
    for (BoundInstancesMap::iterator itr = m_boundInstances.begin(); itr != m_boundInstances.end(); ++itr)
    {
        itr->second.state->RemovePlayer(this);
    }
}

/**
 * @brief Performs pre-destruction cleanup for trade, duel, zone, and unit state.
 */
void Player::CleanupsBeforeDelete()
{
    // Stop cinematic flyover if active (must happen before camera dtor)
    if (m_cinematicFlyover && m_cinematicFlyover->IsActive())
    {
        m_cinematicFlyover->Stop();
    }
    m_cinematicFlyover.reset();

    // Perform cleanup only if the object is fully created
    if (m_uint32Values)
    {
        // Cancel any ongoing trade
        TradeCancel(false);
        // Complete any ongoing duel
        DuelComplete(DUEL_FLED);
    }

    // Notify zone scripts that the player is leaving the zone
    sOutdoorPvPMgr.HandlePlayerLeaveZone(this, m_zoneUpdateId);

    // Perform unit-specific cleanup
    Unit::CleanupsBeforeDelete();
}

/**
 * @brief Creates a new player character with starting data and equipment.
 *
 * @param guidlow The low GUID for the player.
 * @param name The player name.
 * @param race The race id.
 * @param class_ The class id.
 * @param gender The gender id.
 * @param skin The skin customization id.
 * @param face The face customization id.
 * @param hairStyle The hairstyle customization id.
 * @param hairColor The hair color customization id.
 * @param facialHair The facial hair customization id.
 * @param outfitId Unused outfit identifier.
 * @return true if the player was initialized successfully; otherwise, false.
 */
bool Player::Create(uint32 guidlow, const std::string& name, uint8 race, uint8 class_, uint8 gender, uint8 skin, uint8 face, uint8 hairStyle, uint8 hairColor, uint8 facialHair, uint8 /*outfitId */)
{
    // FIXME: outfitId not used in player creation

    // Create the player object with the given GUID
    Object::_Create(guidlow, 0, HIGHGUID_PLAYER);

    // Set the player's name
    m_name = name;

    // Get player info based on race and class
    PlayerInfo const* info = sObjectMgr.GetPlayerInfo(race, class_);
    if (!info)
    {
        sLog.outError("Player has incorrect race/class pair. Can't be loaded.");
        return false;
    }

    // Get class entry from DBC
    ChrClassesEntry const* cEntry = sChrClassesStore.LookupEntry(class_);
    if (!cEntry)
    {
        sLog.outError("Class %u not found in DBC (Wrong DBC files?)", class_);
        return false;
    }

    // Validate gender
    if (gender != uint8(GENDER_MALE) && gender != uint8(GENDER_FEMALE))
    {
        sLog.outError("Invalid gender %u at player creation", uint32(gender));
        return false;
    }

    // Initialize player items to NULL
    for (int i = 0; i < PLAYER_SLOTS_COUNT; ++i)
    {
        m_items[i] = NULL;
    }

    // Set player's initial location
    SetLocationMapId(info->mapId);
    Relocate(info->positionX, info->positionY, info->positionZ, info->orientation);

    // Set the player's map
    SetMap(sMapMgr.CreateMap(info->mapId, this));

    // Set player's power type based on class
    uint8 powertype = cEntry->powerType;

    // Set player's faction based on race
    setFactionForRace(race);

    // Set player's race, class, gender, and power type
    SetByteValue(UNIT_FIELD_BYTES_0, 0, race);
    SetByteValue(UNIT_FIELD_BYTES_0, 1, class_);
    SetByteValue(UNIT_FIELD_BYTES_0, 2, gender);
    SetByteValue(UNIT_FIELD_BYTES_0, 3, powertype);

    // Initialize player's display IDs (model, scale, and model data)
    InitDisplayIds();

    // is it need, only in pre-2.x used and field byte removed later?
    if (powertype == POWER_RAGE || powertype == POWER_MANA)
    {
        SetByteValue(UNIT_FIELD_BYTES_1, 1, 0xEE);
    }

    SetByteValue(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_UNK3 | UNIT_BYTE2_FLAG_UNK5);
    SetUInt32Value(UNIT_FIELD_FLAGS, UNIT_FLAG_PVP_ATTACKABLE);
    SetFloatValue(UNIT_MOD_CAST_SPEED, 1.0f); // Fix cast time shown in spell tooltip on client

    // Set default watched faction index
    SetInt32Value(PLAYER_FIELD_WATCHED_FACTION_INDEX, -1); // -1 is default value

    // Set player's appearance (skin, face, hair style, hair color, facial hair)
    SetByteValue(PLAYER_BYTES, 0, skin);
    SetByteValue(PLAYER_BYTES, 1, face);
    SetByteValue(PLAYER_BYTES, 2, hairStyle);
    SetByteValue(PLAYER_BYTES, 3, hairColor);
    SetByteValue(PLAYER_BYTES_2, 0, facialHair);
    SetByteValue(PLAYER_BYTES_2, 3, REST_STATE_NORMAL);

    // Set player's gender and battlefield arena faction
    SetUInt16Value(PLAYER_BYTES_3, 0, gender); // Only GENDER_MALE/GENDER_FEMALE (1 bit) allowed, drunk state = 0
    SetByteValue(PLAYER_BYTES_3, 3, 0); // BattlefieldArenaFaction (0 or 1)

    // Initialize player's guild information
    SetUInt32Value(PLAYER_GUILDID, 0);
    SetUInt32Value(PLAYER_GUILDRANK, 0);
    SetUInt32Value(PLAYER_GUILD_TIMESTAMP, 0);

    // set starting level
    if (GetSession()->GetSecurity() >= SEC_MODERATOR)
    {
        SetUInt32Value(UNIT_FIELD_LEVEL, sWorld.getConfig(CONFIG_UINT32_START_GM_LEVEL));
    }
    else
    {
        SetUInt32Value(UNIT_FIELD_LEVEL, sWorld.getConfig(CONFIG_UINT32_START_PLAYER_LEVEL));
    }

    // Set player's starting money, honor points, and arena points
    SetUInt32Value(PLAYER_FIELD_COINAGE, sWorld.getConfig(CONFIG_UINT32_START_PLAYER_MONEY));

    // Initialize played time
    m_Last_tick = time(NULL);
    m_Played_time[PLAYED_TIME_TOTAL] = 0;
    m_Played_time[PLAYED_TIME_LEVEL] = 0;

    // Initialize base stats and related field values
    InitStatsForLevel();
    InitTaxiNodes();
    InitTalentForLevel();
    InitPrimaryProfessions(); // To max set before any spell added

    // Apply original stats mods before spell loading or item equipment
    UpdateMaxHealth(); // Update max Health (for add bonus from stamina)
    SetHealth(GetMaxHealth());

    if (GetPowerType() == POWER_MANA)
    {
        UpdateMaxPower(POWER_MANA); // Update max Mana (for add bonus from intellect)
        SetPower(POWER_MANA, GetMaxPower(POWER_MANA));
    }

    // Learn default spells
    learnDefaultSpells();

    // Initialize action bar with default actions
    for (PlayerCreateInfoActions::const_iterator action_itr = info->action.begin(); action_itr != info->action.end(); ++action_itr)
    {
        addActionButton(action_itr->button, action_itr->action, action_itr->type);
    }

    // Initialize player's starting items
    uint32 raceClassGender = GetUInt32Value(UNIT_FIELD_BYTES_0) & 0x00FFFFFF;

    CharStartOutfitEntry const* oEntry = NULL;
    for (uint32 i = 1; i < sCharStartOutfitStore.GetNumRows(); ++i)
    {
        if (CharStartOutfitEntry const* entry = sCharStartOutfitStore.LookupEntry(i))
        {
            if (entry->RaceClassGender == raceClassGender)
            {
                oEntry = entry;
                break;
            }
        }
    }

    if (oEntry)
    {
        for (int j = 0; j < MAX_OUTFIT_ITEMS; ++j)
        {
            if (oEntry->ItemId[j] <= 0)
            {
                continue;
            }

            uint32 item_id = oEntry->ItemId[j];

            // Just skip, reported in ObjectMgr::LoadItemPrototypes
            ItemPrototype const* iProto = ObjectMgr::GetItemPrototype(item_id);
            if (!iProto)
            {
                continue;
            }

            // BuyCount by default
            int32 count = iProto->BuyCount;

            // Special amount for food/drink
            if (iProto->Class == ITEM_CLASS_CONSUMABLE && iProto->SubClass == ITEM_SUBCLASS_FOOD)
            {
                switch (iProto->Spells[0].SpellCategory)
                {
                    case 11: // Food
                        if (iProto->Stackable > 4)
                        {
                            count = 4;
                        }
                        break;
                    case 59: // Drink
                        if (iProto->Stackable > 2)
                        {
                            count = 2;
                        }
                        break;
                }
            }

            StoreNewItemInBestSlots(item_id, count);
        }
    }

    for (PlayerCreateInfoItems::const_iterator item_id_itr = info->item.begin(); item_id_itr != info->item.end(); ++item_id_itr)
    {
        StoreNewItemInBestSlots(item_id_itr->item_id, item_id_itr->item_amount);
    }

    // Equip bags and main-hand weapon
    // Second pass for not equipped items (offhand weapon/shield if it attempted to equip before main-hand weapon)
    // or ammo not equipped in special bag
    for (int i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            uint16 eDest;
            // Equip offhand weapon/shield if it attempted to equip before main-hand weapon
            InventoryResult msg = CanEquipItem(NULL_SLOT, eDest, pItem, false);
            if (msg == EQUIP_ERR_OK)
            {
                RemoveItem(INVENTORY_SLOT_BAG_0, i, true);
                EquipItem(eDest, pItem, true);
            }
            // Move other items to more appropriate slots (ammo not equipped in special bag)
            else
            {
                ItemPosCountVec sDest;
                msg = CanStoreItem(NULL_BAG, NULL_SLOT, sDest, pItem, false);
                if (msg == EQUIP_ERR_OK)
                {
                    RemoveItem(INVENTORY_SLOT_BAG_0, i, true);
                    pItem = StoreItem(sDest, pItem, true);
                }

                // If this is ammo then use it
                msg = CanUseAmmo(pItem->GetEntry());
                if (msg == EQUIP_ERR_OK)
                {
                    SetAmmo(pItem->GetEntry());
                }
            }
        }
    }
    // All item positions resolved

    return true;
}

/**
 * @brief Equips or stores a newly created item in the best available slots.
 *
 * @param titem_id The item entry id.
 * @param titem_amount The amount to create.
 * @return true if all remaining items were equipped or stored; otherwise, false.
 */
bool Player::StoreNewItemInBestSlots(uint32 titem_id, uint32 titem_amount)
{
    DEBUG_LOG("STORAGE: Creating initial item, itemId = %u, count = %u", titem_id, titem_amount);

    // Attempt to equip the item one by one
    while (titem_amount > 0)
    {
        uint16 eDest;
        uint8 msg = CanEquipNewItem(NULL_SLOT, eDest, titem_id, false);
        if (msg != EQUIP_ERR_OK)
        {
            break;
        }

        // Equip the new item
        EquipNewItem(eDest, titem_id, true);
        AutoUnequipOffhandIfNeed();
        --titem_amount;
    }

    if (titem_amount == 0)
    {
        return true; // All items equipped
    }

    // Attempt to store the remaining items
    ItemPosCountVec sDest;
    // Store in the main bag to simplify the second pass (special bags may not be equipped yet)
    uint8 msg = CanStoreNewItem(INVENTORY_SLOT_BAG_0, NULL_SLOT, sDest, titem_id, titem_amount);
    if (msg == EQUIP_ERR_OK)
    {
        StoreNewItem(sDest, titem_id, true, Item::GenerateItemRandomPropertyId(titem_id));
        return true; // Items stored
    }

    // Item cannot be added
    sLog.outError("STORAGE: Can't equip or store initial item %u for race %u class %u , error msg = %u", titem_id, getRace(), getClass(), msg);
    return false;
}

// Helper function, mainly for script side, but can be used for simple tasks in MaNGOS as well
Item* Player::StoreNewItemInInventorySlot(uint32 itemEntry, uint32 amount)
{
    ItemPosCountVec vDest;

    uint8 msg = CanStoreNewItem(INVENTORY_SLOT_BAG_0, NULL_SLOT, vDest, itemEntry, amount);

    if (msg == EQUIP_ERR_OK)
    {
        if (Item* pItem = StoreNewItem(vDest, itemEntry, true, Item::GenerateItemRandomPropertyId(itemEntry)))
        {
            return pItem;
        }
    }

    return NULL;
}










/**
 * @brief Updates player state, timers, combat, saving, and delayed actions.
 *
 * @param update_diff The elapsed update time in milliseconds.
 * @param p_time The elapsed update time used by some player timers.
 */
void Player::Update(uint32 update_diff, uint32 p_time)
{
    // If the player is not in the world, return early
    if (!IsInWorld())
    {
        return;
    }

    // Handle undelivered mail
    if (m_nextMailDelivereTime && m_nextMailDelivereTime <= time(NULL))
    {
        SendNewMail();
        ++unReadMails;

        // It will be recalculate at mailbox open (for unReadMails important non-0 until mailbox open, it also will be recalculated)
        m_nextMailDelivereTime = 0;
    }

    // Used to implement delayed far teleports
    SetCanDelayTeleport(true);
    Unit::Update(update_diff, p_time);
    SetCanDelayTeleport(false);

    // Periodic observer-side visibility maintenance.
    // The owner's visible set is otherwise refreshed only when the player moves
    // (Camera::UpdateVisibilityForOwner via OnRelocated), while an object moving
    // out of range only re-notifies observers still near that object. A
    // near-stationary player therefore never gets an out-of-range update for an
    // active object that walks away, leaving it frozen on the client. Sweep the
    // owner's visible set on an interval so out-of-range objects are dropped even
    // when the player stands still. Skipped mid-teleport (visibility is rebuilt
    // by the teleport path) and gated by config.
    if (World::GetVisibilityObserverSweepEnabled() && !IsBeingTeleported())
    {
        if (m_visibilityObserverSweepTimer <= update_diff)
        {
            m_visibilityObserverSweepTimer = World::GetVisibilityObserverSweepInterval();
            GetCamera().UpdateVisibilityForOwner();
        }
        else
        {
            m_visibilityObserverSweepTimer -= update_diff;
        }
    }

    // Update cinematic flyover if active
    if (m_cinematicFlyover && m_cinematicFlyover->IsActive())
    {
        m_cinematicFlyover->Update(update_diff);
    }

    // Update player-only attacks
    if (uint32 ranged_att = getAttackTimer(RANGED_ATTACK))
    {
        setAttackTimer(RANGED_ATTACK, (update_diff >= ranged_att ? 0 : ranged_att - update_diff));
    }

    time_t now = time(NULL);

    // Update PvP flag
    UpdatePvPFlag(now);

    // Update contested PvP state
    UpdateContestedPvP(update_diff);

    // Update duel flag
    UpdateDuelFlag(now);

    // Check duel distance
    CheckDuelDistance(now);

    // Update items that have just a limited lifetime
    if (now > m_Last_tick)
    {
        UpdateItemDuration(uint32(now - m_Last_tick));
    }

    // Update timed quests
    if (!m_timedquests.empty())
    {
        QuestSet::iterator iter = m_timedquests.begin();
        while (iter != m_timedquests.end())
        {
            QuestStatusData& q_status = mQuestStatus[*iter];
            if (q_status.m_timer <= update_diff)
            {
                uint32 quest_id  = *iter;
                ++iter; // Current iter will be removed in FailQuest
                FailQuest(quest_id);
            }
            else
            {
                q_status.m_timer -= update_diff;
                if (q_status.uState != QUEST_NEW)
                {
                    q_status.uState = QUEST_CHANGED;
                }
                ++iter;
            }
        }
    }

    // Update melee attacking state
    if (hasUnitState(UNIT_STAT_MELEE_ATTACKING))
    {
        UpdateMeleeAttackingState();

        Unit* pVictim = getVictim();
        if (pVictim && !IsNonMeleeSpellCasted(false))
        {
            Player* vOwner = pVictim->GetCharmerOrOwnerPlayerOrPlayerItself();
            if (vOwner && vOwner->IsPvP() && !IsInDuelWith(vOwner))
            {
                UpdatePvP(true);
                RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_ENTER_PVP_COMBAT);
            }
        }
    }

    // Speed collect rest bonus (section/in hour)
    if (HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_RESTING))
    {
        if (GetTimeInnEnter() > 0) // Freeze update
        {
            time_t time_inn = now - GetTimeInnEnter();
            if (time_inn >= 10) // Freeze update
            {
                SetRestBonus(GetRestBonus() + ComputeRest(time_inn));
                UpdateInnerTime(now);
            }
        }
    }

    // Update regeneration timer
    if (m_regenTimer)
    {
        if (update_diff >= m_regenTimer)
        {
            m_regenTimer = 0;
        }
        else
        {
            m_regenTimer -= update_diff;
        }
    }

    // Update position status timer
    if (m_positionStatusUpdateTimer)
    {
        if (update_diff >= m_positionStatusUpdateTimer)
        {
            m_positionStatusUpdateTimer = 0;
        }
        else
        {
            m_positionStatusUpdateTimer -= update_diff;
        }
    }

    // Update weapon change timer
    if (m_weaponChangeTimer > 0)
    {
        if (update_diff >= m_weaponChangeTimer)
        {
            m_weaponChangeTimer = 0;
        }
        else
        {
            m_weaponChangeTimer -= update_diff;
        }
    }

    // Update zone timer
    if (m_zoneUpdateTimer > 0)
    {
        if (update_diff >= m_zoneUpdateTimer)
        {
            uint32 newzone, newarea;
            GetZoneAndAreaId(newzone, newarea);

            if (m_zoneUpdateId != newzone)
            {
                UpdateZone(newzone, newarea); // Also update area
            }
            else
            {
                // Use area updates as well
                // Needed for free-for-all arenas, for example
                if (m_areaUpdateId != newarea)
                {
                    UpdateArea(newarea);
                }

                m_zoneUpdateTimer = ZONE_UPDATE_INTERVAL;
            }
        }
        else
        {
            m_zoneUpdateTimer -= update_diff;
        }
    }

    if (IsAlive())
    {
        RegenerateAll();
    }

    // Handle player death
    if (m_deathState == JUST_DIED)
    {
        KillPlayer();
    }

    // Handle periodic saving
    if (m_nextSave > 0)
    {
        if (update_diff >= m_nextSave)
        {
            // m_nextSave reset in SaveToDB call
            // Used by Eluna
#ifdef ENABLE_ELUNA
            if (Eluna* e = GetEluna())
            {
                e->OnSave(this);
            }
#endif /* ENABLE_ELUNA */
            SaveToDB();
            DETAIL_LOG("Player '%s' (GUID: %u) saved", GetName(), GetGUIDLow());
        }
        else
        {
            m_nextSave -= update_diff;
        }
    }

    // Handle water/drowning
    HandleDrowning(update_diff);

    // Handle detect stealth players
    if (m_DetectInvTimer > 0)
    {
        if (update_diff >= m_DetectInvTimer)
        {
            HandleStealthedUnitsDetection();
            m_DetectInvTimer = 3000;
        }
        else
        {
            m_DetectInvTimer -= update_diff;
        }
    }

    // Update played time
    if (now > m_Last_tick)
    {
        uint32 elapsed = uint32(now - m_Last_tick);
        m_Played_time[PLAYED_TIME_TOTAL] += elapsed; // Total played time
        m_Played_time[PLAYED_TIME_LEVEL] += elapsed; // Level played time
        m_Last_tick = now;
    }

    // Handle sobering if the player is drunk
    if (m_drunk)
    {
        m_drunkTimer += update_diff;

        if (m_drunkTimer > 10 * IN_MILLISECONDS)
        {
            HandleSobering();
        }
    }

    // Handle ghost auto-free from body in instances
    if (m_deathTimer > 0 && !GetMap()->Instanceable())
    {
        if (p_time >= m_deathTimer)
        {
            m_deathTimer = 0;
            BuildPlayerRepop();
            RepopAtGraveyard();
        }
        else
        {
            m_deathTimer -= p_time;
        }
    }

    // Update enchant time
    UpdateEnchantTime(update_diff);

    // Update homebind time
    UpdateHomebindTime(update_diff);

    // Group update
    SendUpdateToOutOfRangeGroupMembers();
    if (IsHasDelayedTeleport())
    {
        TeleportTo(m_teleport_dest, m_teleport_options);
    }

#ifdef ENABLE_PLAYERBOTS
    // Update player bot AI
    if (m_playerbotAI)
    {
        m_playerbotAI->UpdateAI(p_time);
    }
    if (m_playerbotMgr)
    {
        m_playerbotMgr->UpdateAI(p_time);
    }
#endif

}

/**
 * @brief Changes the player's death state and handles player-specific death logic.
 *
 * @param s The new death state.
 */
void Player::SetDeathState(DeathState s)
{
    uint32 ressSpellId = 0;

    bool cur = IsAlive();

    if (s == JUST_DIED && cur)
    {
        // drunken state is cleared on death
        SetDrunkValue(0);
        // lost combo points at any target (targeted combo points clear in Unit::SetDeathState)
        ClearComboPoints();

        clearResurrectRequestData();

        // remove form before other mods to prevent incorrect stats calculation
        RemoveSpellsCausingAura(SPELL_AURA_MOD_SHAPESHIFT);

        // FIXME: is pet dismissed at dying or releasing spirit? if second, add SetDeathState(DEAD) to HandleRepopRequestOpcode and define pet unsummon here with (s == DEAD)
        RemovePet(PET_SAVE_REAGENTS);

        // remove uncontrolled pets
        RemoveMiniPet();

        // save value before aura remove in Unit::SetDeathState
        ressSpellId = GetUInt32Value(PLAYER_SELF_RES_SPELL);

        // passive spell
        if (!ressSpellId)
        {
            ressSpellId = GetResurrectionSpellId();
        }

        if (InstanceData* mapInstance = GetInstanceData())
        {
            mapInstance->OnPlayerDeath(this);
        }
    }

    Unit::SetDeathState(s);

    // restore resurrection spell id for player after aura remove
    if (s == JUST_DIED && cur && ressSpellId)
    {
        SetUInt32Value(PLAYER_SELF_RES_SPELL, ressSpellId);
    }

    if (IsAlive() && !cur)
    {
        // clear aura case after resurrection by another way (spells will be applied before next death)
        SetUInt32Value(PLAYER_SELF_RES_SPELL, 0);

        // restore default warrior stance
        if (getClass() == CLASS_WARRIOR)
        {
            CastSpell(this, SPELL_ID_PASSIVE_BATTLE_STANCE, true);
        }
    }
}

/**
 * @brief Builds character-enumeration data for the character selection screen.
 *
 * @param result The database row for the character.
 * @param p_data The packet being populated.
 * @return true if the enum data was built successfully; otherwise, false.
 */
bool Player::BuildEnumData(QueryResult* result, WorldPacket* p_data)
{
    //             0               1                2                3                 4                  5                       6                        7
    //    "SELECT characters.guid, characters.name, characters.race, characters.class, characters.gender, characters.playerBytes, characters.playerBytes2, characters.level, "
    //     8                9               10                     11                     12                     13                    14
    //    "characters.zone, characters.map, characters.position_x, characters.position_y, characters.position_z, guild_member.guildid, characters.playerFlags, "
    //    15                    16                   17                     18                   19
    //    "characters.at_login, character_pet.entry, character_pet.modelid, character_pet.level, characters.equipmentCache "

    Field* fields = result->Fetch();

    uint32 guid = fields[0].GetUInt32();
    uint8 pRace = fields[2].GetUInt8();
    uint8 pClass = fields[3].GetUInt8();

    PlayerInfo const* info = sObjectMgr.GetPlayerInfo(pRace, pClass);
    if (!info)
    {
        sLog.outError("Player %u has incorrect race/class pair. Don't build enum.", guid);
        return false;
    }

    *p_data << ObjectGuid(HIGHGUID_PLAYER, guid);
    *p_data << fields[1].GetString();                       // name
    *p_data << uint8(pRace);                                // race
    *p_data << uint8(pClass);                               // class
    *p_data << uint8(fields[4].GetUInt8());                 // gender

    uint32 playerBytes = fields[5].GetUInt32();
    *p_data << uint8(playerBytes);                          // skin
    *p_data << uint8(playerBytes >> 8);                     // face
    *p_data << uint8(playerBytes >> 16);                    // hair style
    *p_data << uint8(playerBytes >> 24);                    // hair color

    uint32 playerBytes2 = fields[6].GetUInt32();
    *p_data << uint8(playerBytes2 & 0xFF);                  // facial hair

    *p_data << uint8(fields[7].GetUInt8());                 // level
    *p_data << uint32(fields[8].GetUInt32());               // zone
    *p_data << uint32(fields[9].GetUInt32());               // map

    *p_data << fields[10].GetFloat();                       // x
    *p_data << fields[11].GetFloat();                       // y
    *p_data << fields[12].GetFloat();                       // z

    *p_data << uint32(fields[13].GetUInt32());              // guild id

    uint32 char_flags = 0;
    uint32 playerFlags = fields[14].GetUInt32();
    uint32 atLoginFlags = fields[15].GetUInt32();
    if (playerFlags & PLAYER_FLAGS_HIDE_HELM)
    {
        char_flags |= CHARACTER_FLAG_HIDE_HELM;
    }
    if (playerFlags & PLAYER_FLAGS_HIDE_CLOAK)
    {
        char_flags |= CHARACTER_FLAG_HIDE_CLOAK;
    }
    if (playerFlags & PLAYER_FLAGS_GHOST)
    {
        char_flags |= CHARACTER_FLAG_GHOST;
    }
    if (atLoginFlags & AT_LOGIN_RENAME)
    {
        char_flags |= CHARACTER_FLAG_RENAME;
    }

    *p_data << uint32(char_flags);                          // character flags

    // First login
    *p_data << uint8(atLoginFlags & AT_LOGIN_FIRST ? 1 : 0);

    // Pets info
    {
        uint32 petDisplayId = 0;
        uint32 petLevel   = 0;
        uint32 petFamily  = 0;

        // show pet at selection character in character list only for non-ghost character
        if (result && !(playerFlags & PLAYER_FLAGS_GHOST) && (pClass == CLASS_WARLOCK || pClass == CLASS_HUNTER))
        {
            uint32 entry = fields[16].GetUInt32();
            CreatureInfo const* cInfo = sCreatureStorage.LookupEntry<CreatureInfo>(entry);
            if (cInfo)
            {
                petDisplayId = fields[17].GetUInt32();
                petLevel     = fields[18].GetUInt32();
                petFamily    = cInfo->Family;
            }
        }

        *p_data << uint32(petDisplayId);
        *p_data << uint32(petLevel);
        *p_data << uint32(petFamily);
    }

    Tokens data = StrSplit(fields[19].GetCppString(), " ");
    for (uint8 slot = 0; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        uint32 visualbase = slot * 2;                       // entry, perm ench., temp ench.
        uint32 item_id = GetUInt32ValueFromArray(data, visualbase);
        const ItemPrototype* proto = ObjectMgr::GetItemPrototype(item_id);
        if (!proto)
        {
            *p_data << uint32(0);
            *p_data << uint8(0);
            continue;
        }

        *p_data << uint32(proto->DisplayInfoID);
        *p_data << uint8(proto->InventoryType);
    }

    *p_data << uint32(0);                                   // first bag display id
    *p_data << uint8(0);                                    // first bag inventory type

    return true;
}

/**
 * @brief Toggles AFK status and leaves battlegrounds when entering AFK.
 */
void Player::ToggleAFK()
{
    ToggleFlag(PLAYER_FLAGS, PLAYER_FLAGS_AFK);

    // afk player not allowed in battleground
    if (isAFK() && InBattleGround())
    {
        LeaveBattleground();
    }
}

/**
 * @brief Toggles DND status.
 */
void Player::ToggleDND()
{
    ToggleFlag(PLAYER_FLAGS, PLAYER_FLAGS_DND);
}

/**
 * @brief Gets the active chat tag displayed for the player.
 *
 * @return The chat tag flags for GM, AFK, DND, or none.
 */
ChatTagFlags Player::GetChatTag() const
{
    if (isGMChat())
    {
        return CHAT_TAG_GM;
    }

    if (isAFK())
    {
        return CHAT_TAG_AFK;
    }
    if (isDND())
    {
        return CHAT_TAG_DND;
    }

    return CHAT_TAG_NONE;
}

/**
 * @brief Teleports the player to a target location, handling near and far cases.
 *
 * @param mapid The destination map id.
 * @param x The destination x coordinate.
 * @param y The destination y coordinate.
 * @param z The destination z coordinate.
 * @param orientation The destination orientation.
 * @param options Teleport option flags.
 * @param allowNoDelay true to bypass delayed-teleport deferral when possible.
 * @return true if teleport setup succeeded; otherwise, false.
 */
bool Player::TeleportTo(uint32 mapid, float x, float y, float z, float orientation, uint32 options /*=0*/, bool allowNoDelay /*=false*/)
{
    // Stop cinematic flyover on teleport (body is map-bound)
    if (m_cinematicFlyover && m_cinematicFlyover->IsActive())
    {
        m_cinematicFlyover->Stop();
    }

    if (!MapManager::IsValidMapCoord(mapid, x, y, z, orientation))
    {
        sLog.outError("TeleportTo: invalid map %d or absent instance template.", mapid);
        return false;
    }

    MapEntry const* mEntry = sMapStore.LookupEntry(mapid);  // Validity checked in IsValidMapCoord

    if (!isGameMaster() && DisableMgr::IsDisabledFor(DISABLE_TYPE_MAP, mapid, this))
    {
        sLog.outDebug("Player (GUID: %u, name: %s) tried to enter a forbidden map %u", GetGUIDLow(), GetName(), mapid);
        SendTransferAbortedByLockStatus(mEntry,nullptr, AREA_LOCKSTATUS_NOT_ALLOWED);
        return false;
    }

    // preparing unsummon pet if lost (we must get pet before teleportation or will not find it later)
    Pet* pet = GetPet();

    // don't let enter battlegrounds without assigned battleground id (for example through areatrigger)...
    // don't let gm level > 1 either
    if (!InBattleGround() && mEntry->IsBattleGround())
    {
        return false;
    }

    // Check requirements for teleport
    if (!IsAlive() && mEntry->IsDungeon())    // rare case of teleporting the player into an instance with no areatrigger participation
    {
        ResurrectPlayer(0.5f);
        SpawnCorpseBones();
    }

    // if we were on a transport, leave
    if (!(options & TELE_TO_NOT_LEAVE_TRANSPORT) && m_transport)
    {
        m_transport->RemovePassenger(this);
        m_transport = NULL;
        m_movementInfo.ClearTransportData();
    }

    // The player was ported to another map and looses the duel immediately.
    // We have to perform this check before the teleport, otherwise the
    // ObjectAccessor won't find the flag.
    if (duel && GetMapId() != mapid)
    {
        if (GetMap()->GetGameObject(GetGuidValue(PLAYER_DUEL_ARBITER)))
        {
            DuelComplete(DUEL_FLED);
        }
    }

    // reset movement flags at teleport, because player will continue move with these flags after teleport
    m_movementInfo.SetMovementFlags(MOVEFLAG_NONE);
    DisableSpline();

    if ((GetMapId() == mapid) && (!m_transport))            // TODO the !m_transport might have unexpected effects when teleporting from transport to other place on same map
    {
        // lets reset far teleport flag if it wasn't reset during chained teleports
        SetSemaphoreTeleportFar(false);
        // setup delayed teleport flag
        // if teleport spell is casted in Unit::Update() func
        // then we need to delay it until update process will be finished
        if (!allowNoDelay)
        {
            if (SetDelayedTeleportFlagIfCan())
            {
                SetSemaphoreTeleportNear(true);
                // lets save teleport destination for player
                m_teleport_dest = WorldLocation(mapid, x, y, z, orientation);
                m_teleport_options = options;
                return true;
            }
        }

        if (!(options & TELE_TO_NOT_UNSUMMON_PET))
        {
            // same map, only remove pet if out of range for new position
            if (pet)
            {
                if (!pet->IsWithinDist3d(x, y, z, GetMap()->GetVisibilityDistance()))
                {
                    if (pet->IsAlive())
                    {
                        UnsummonPetTemporaryIfAny();
                    }
                    else
                    {
                        pet->Unsummon(PET_SAVE_NOT_IN_SLOT);
                        pet = GetPet();
                    }
                }
            }
        }

        if (!(options & TELE_TO_NOT_LEAVE_COMBAT))
        {
            CombatStop();
        }

        // this will be used instead of the current location in SaveToDB
        m_teleport_dest = WorldLocation(mapid, x, y, z, orientation);
        SetFallInformation(0, z);

        // code for finish transfer called in WorldSession::HandleMovementOpcodes()
        // at client packet MSG_MOVE_TELEPORT_ACK
        SetSemaphoreTeleportNear(true);
        // near teleport, triggering send MSG_MOVE_TELEPORT_ACK from client at landing
        if (!GetSession()->PlayerLogout())
        {
            WorldPacket data;
            BuildTeleportAckMsg(data, x, y, z, orientation);
            GetSession()->SendPacket(&data);
        }
    }
    else
    {
        // far teleport to another map
        Map* oldmap = IsInWorld() ? GetMap() : NULL;
        // check if we can enter before stopping combat / removing pet / totems / interrupting spells

        // If the map is not created, assume it is possible to enter it.
        // It will be created in the WorldPortAck.
        DungeonPersistentState* state = GetBoundInstanceSaveForSelfOrGroup(mapid);
        Map* map = sMapMgr.FindMap(mapid, state ? state->GetInstanceId() : 0);
        if (!map || map->CanEnter(this))
        {
            // lets reset near teleport flag if it wasn't reset during chained teleports
            SetSemaphoreTeleportNear(false);
            // setup delayed teleport flag
            // if teleport spell is casted in Unit::Update() func
            // then we need to delay it until update process will be finished
            if (!allowNoDelay)
            {
                if (SetDelayedTeleportFlagIfCan())
                {
                    SetSemaphoreTeleportFar(true);
                    // lets save teleport destination for player
                    m_teleport_dest = WorldLocation(mapid, x, y, z, orientation);
                    m_teleport_options = options;
                    return true;
                }
            }

            SetSelectionGuid(ObjectGuid());

            CombatStop();

            ResetContestedPvP();

            // remove player from battleground on far teleport (when changing maps)
            if (BattleGround const* bg = GetBattleGround())
            {
                // Note: at battleground join battleground id set before teleport
                // and we already will found "current" battleground
                // just need check that this is targeted map or leave
                if (bg->GetMapId() != mapid)
                {
                    LeaveBattleground(false); // don't teleport to entry point
                }
            }

            // remove pet on map change
            if (pet)
            {
                if (pet->IsAlive())
                {
                    UnsummonPetTemporaryIfAny();
                }
                else
                {
                    pet->Unsummon(PET_SAVE_NOT_IN_SLOT);
                    pet = GetPet();
                }
            }
            // remove all dyn objects
            RemoveAllDynObjects();

            // stop spellcasting
            // not attempt interrupt teleportation spell at caster teleport
            if (!(options & TELE_TO_SPELL))
            {
                if (IsNonMeleeSpellCasted(true))
                {
                    InterruptNonMeleeSpells(true);
                }
            }

            // remove auras before removing from map...
            RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_CHANGE_MAP | AURA_INTERRUPT_FLAG_MOVE | AURA_INTERRUPT_FLAG_TURNING);

            if (!GetSession()->PlayerLogout())
            {
                // send transfer packet to display load screen
                WorldPacket data(SMSG_TRANSFER_PENDING, (4 + 4 + 4));
                data << uint32(mapid);
                if (m_transport)
                {
                    data << uint32(m_transport->GetEntry());
                    data << uint32(GetMapId());
                }
                GetSession()->SendPacket(&data);
            }

            // remove from old map now
            if (oldmap)
            {
                oldmap->Remove(this, false);
            }

            // new final coordinates
            float final_x = x;
            float final_y = y;
            float final_z = z;
            float final_o = orientation;

            Position const* transportPosition = m_movementInfo.GetTransportPos();

            if (m_transport)
            {
                final_x += transportPosition->x;
                final_y += transportPosition->y;
                final_z += transportPosition->z;
                final_o += transportPosition->o;
            }

            m_teleport_dest = WorldLocation(mapid, final_x, final_y, final_z, final_o);
            SetFallInformation(0, final_z);
            // if the player is saved before worldport ack (at logout for example)
            // this will be used instead of the current location in SaveToDB

            // move packet sent by client always after far teleport
            // code for finish transfer to new map called in WorldSession::HandleMoveWorldportAckOpcode at client packet
            SetSemaphoreTeleportFar(true);

            if (!GetSession()->PlayerLogout())
            {
                // transfer finished, inform client to start load
                WorldPacket data(SMSG_NEW_WORLD, (20));
                data << uint32(mapid);
                if (m_transport)
                {
                    data << float(transportPosition->x);
                    data << float(transportPosition->y);
                    data << float(transportPosition->z);
                    data << float(transportPosition->o);
                }
                else
                {
                    data << float(final_x);
                    data << float(final_y);
                    data << float(final_z);
                    data << float(final_o);
                }

                GetSession()->SendPacket(&data);
                SendSavedInstances();
            }
        }
        else                                                // !map->CanEnter(this)
        {
            return false;
        }
    }
    return true;
}


/**
 * @brief Executes queued delayed player operations.
 */
void Player::ProcessDelayedOperations()
{
    if (m_DelayedOperations == 0)
    {
        return;
    }

    if (m_DelayedOperations & DELAYED_RESURRECT_PLAYER)
    {
        ResurrectPlayer(0.0f, false);

        if (GetMaxHealth() > m_resurrectHealth)
        {
            SetHealth(m_resurrectHealth);
        }
        else
        {
            SetHealth(GetMaxHealth());
        }

        if (GetMaxPower(POWER_MANA) > m_resurrectMana)
        {
            SetPower(POWER_MANA, m_resurrectMana);
        }
        else
        {
            SetPower(POWER_MANA, GetMaxPower(POWER_MANA));
        }

        SetPower(POWER_RAGE, 0);
        SetPower(POWER_ENERGY, GetMaxPower(POWER_ENERGY));

        SpawnCorpseBones();
    }

    if (m_DelayedOperations & DELAYED_SAVE_PLAYER)
    {
        SaveToDB();
    }

    if (m_DelayedOperations & DELAYED_SPELL_CAST_DESERTER)
    {
        CastSpell(this, 26013, true);               // Deserter
    }

    // we have executed ALL delayed ops, so clear the flag
    m_DelayedOperations = 0;
}

/**
 * @brief Adds the player and equipped items to the world.
 */
void Player::AddToWorld()
{
    ///- Do not add/remove the player from the object storage
    ///- It will crash when updating the ObjectAccessor
    ///- The player should only be added when logging in
    Unit::AddToWorld();

    for (int i = PLAYER_SLOT_START; i < PLAYER_SLOT_END; ++i)
    {
        if (m_items[i])
        {
            m_items[i]->AddToWorld();
        }
    }

    // Notifies player about his group status while adding him into the world
    // Restricted to players having a group in raid mode
    if (GetTransport() && GetGroup() && GetGroup()->isRaidGroup())
    {
        SetGroupUpdateFlag(GROUP_UPDATE_FULL);
        GetGroup()->SendUpdateToPlayer(this);
    }
}

/**
 * @brief Removes the player and equipped items from the world.
 */
void Player::RemoveFromWorld()
{
    // cleanup
    if (IsInWorld())
    {
        ///- Release charmed creatures, unsummon totems and remove pets/guardians
        UnsummonAllTotems();
        RemoveMiniPet();
    }

    // Notifies the client that he has left the raid group.
    // Only valid when the player is on the transport.
    if (GetTransport() && GetGroup() && GetGroup()->isRaidGroup())
    {
        WorldPacket data;
        // For client, sending an empty group list is enough to be ungroup.
        data.Initialize(SMSG_GROUP_LIST, 24);
        data << uint64(0) << uint64(0) << uint64(0);
        m_session->SendPacket(&data);
    }

    for (int i = PLAYER_SLOT_START; i < PLAYER_SLOT_END; ++i)
    {
        if (m_items[i])
        {
            m_items[i]->RemoveFromWorld();
        }
    }

    // remove duel before calling Unit::RemoveFromWorld
    // otherwise there will be an existing duel flag pointer but no entry in m_gameObj
    DuelComplete(DUEL_INTERRUPTED);

    ///- Do not add/remove the player from the object storage
    ///- It will crash when updating the ObjectAccessor
    ///- The player should only be removed when logging out
    if (IsInWorld())
    {
        GetCamera().ResetView();
    }

    Unit::RemoveFromWorld();
}





/**
 * @brief Gets an NPC the player can currently interact with.
 *
 * @param guid The target creature GUID.
 * @param npcflagmask Optional NPC flag mask that must be present on the creature.
 * @return The interactable creature, or null if interaction is not allowed.
 */
Creature* Player::GetNPCIfCanInteractWith(ObjectGuid guid, uint32 npcflagmask)
{
    // some basic checks
    if (!guid || !IsInWorld() || IsTaxiFlying())
    {
        return NULL;
    }

    // not in interactive state
    if (hasUnitState(UNIT_STAT_CAN_NOT_REACT_OR_LOST_CONTROL))
    {
        return NULL;
    }

    // exist (we need look pets also for some interaction (quest/etc)
    Creature* unit = GetMap()->GetAnyTypeCreature(guid);
    if (!unit)
    {
        return NULL;
    }

    // appropriate npc type
    if (npcflagmask && !unit->HasFlag(UNIT_NPC_FLAGS, npcflagmask))
    {
        return NULL;
    }

    if (npcflagmask == UNIT_NPC_FLAG_STABLEMASTER)
    {
        if (getClass() != CLASS_HUNTER)
        {
            return NULL;
        }
    }

    // if a dead unit should be able to talk - the creature must be alive and have special flags
    if (!unit->IsAlive())
    {
        return NULL;
    }

    if (IsAlive() && unit->IsInvisibleForAlive())
    {
        return NULL;
    }

    // not allow interaction under control, but allow with own pets
    if (unit->GetCharmerGuid())
    {
        return NULL;
    }

    // not enemy
    if (unit->IsHostileTo(this))
    {
        return NULL;
    }

    // not too far
    if (!unit->IsWithinDistInMap(this, INTERACTION_DISTANCE))
    {
        return NULL;
    }

    return unit;
}

/**
 * @brief Gets a game object the player can currently interact with.
 *
 * @param guid The target game object GUID.
 * @param gameobject_type The required game object type, or MAX_GAMEOBJECT_TYPE for any type.
 * @return The interactable game object, or null if interaction is not allowed.
 */
GameObject* Player::GetGameObjectIfCanInteractWith(ObjectGuid guid, uint32 gameobject_type) const
{
    // some basic checks
    if (!guid || !IsInWorld() || IsTaxiFlying())
    {
        return NULL;
    }

    // not in interactive state
    if (hasUnitState(UNIT_STAT_CAN_NOT_REACT_OR_LOST_CONTROL))
    {
        return NULL;
    }

    if (GameObject* go = GetMap()->GetGameObject(guid))
    {
        if (uint32(go->GetGoType()) == gameobject_type || gameobject_type == MAX_GAMEOBJECT_TYPE)
        {
            float maxdist = go->GetInteractionDistance();
            if (go->IsWithinDistInMap(this, maxdist) && go->isSpawned())
            {
                return go;
            }

            sLog.outError("GetGameObjectIfCanInteractWith: GameObject '%s' [GUID: %u] is too far away from player %s [GUID: %u] to be used by him (distance=%f, maximal %f is allowed)",
                go->GetGOInfo()->name,  go->GetGUIDLow(), GetName(), GetGUIDLow(), go->GetDistance(this), maxdist);
        }
    }
    return NULL;
}

/**
 * @brief Checks whether the player is currently underwater.
 *
 * @return True if the player is underwater; otherwise, false.
 */
bool Player::IsUnderWater() const
{
    return GetMap()->GetTerrain()->IsUnderWater(GetPositionX(), GetPositionY(), GetPositionZ() + 2);
}

bool Player::IsDrowning() const
{
    return (m_MirrorTimerFlags & UNDERWATER_INWATER) &&
            m_MirrorTimer[BREATH_TIMER] != DISABLED_MIRROR_TIMER &&
            !HasAuraType(SPELL_AURA_WATER_BREATHING) &&
            m_MirrorTimer[BREATH_TIMER] < 2000 && IsAlive();
}

/**
 * @brief Updates the player's in-water state.
 *
 * @param apply True if the player is entering water; false if leaving it.
 */
void Player::SetInWater(bool apply)
{
    if (m_isInWater == apply)
    {
        return;
    }

    // define player in water by opcodes
    // move player's guid into HateOfflineList of those mobs
    // which can't swim and move guid back into ThreatList when
    // on surface.
    // TODO: exist also swimming mobs, and function must be symmetric to enter/leave water
    m_isInWater = apply;

    // remove auras that need water/land
    RemoveAurasWithInterruptFlags(apply ? AURA_INTERRUPT_FLAG_NOT_ABOVEWATER : AURA_INTERRUPT_FLAG_NOT_UNDERWATER);

    GetHostileRefManager().updateThreatTables();
}

struct SetGameMasterOnHelper
{
    explicit SetGameMasterOnHelper() {}
    void operator()(Unit* unit) const
    {
        unit->setFaction(35);
        unit->GetHostileRefManager().setOnlineOfflineState(false);
    }
};

struct SetGameMasterOffHelper
{
    explicit SetGameMasterOffHelper(uint32 _faction) : faction(_faction) {}
    void operator()(Unit* unit) const
    {
        unit->setFaction(faction);
        unit->GetHostileRefManager().setOnlineOfflineState(true);
    }
    uint32 faction;
};

/**
 * @brief Enables or disables game master mode for the player.
 *
 * @param on True to enable GM mode; false to disable it.
 */
void Player::SetGameMaster(bool on)
{
    if (on)
    {
        m_ExtraFlags |= PLAYER_EXTRA_GM_ON;
        //setFaction(35);
        SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_UNK_0);
        SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_GM);
        CallForAllControlledUnits(SetGameMasterOnHelper(), CONTROLLED_PET | CONTROLLED_TOTEMS | CONTROLLED_GUARDIANS | CONTROLLED_CHARM);

        SetFFAPvP(false);
        ResetContestedPvP();

        GetHostileRefManager().setOnlineOfflineState(false);
        CombatStopWithPets();

        if (Pet* pet = GetPet())
        {
            if (m_ExtraFlags |= PLAYER_EXTRA_GM_ON)
            {
                pet->setFaction(35);
            }
            pet->GetHostileRefManager().setOnlineOfflineState(false);
        }
    }
    else
    {
        m_ExtraFlags &= ~ PLAYER_EXTRA_GM_ON;
        //setFactionForRace(getRace());
        RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_UNK_0);
        RemoveFlag(PLAYER_FLAGS, PLAYER_FLAGS_GM);

        if (Pet* pet = GetPet())
        {
            pet->setFaction(getFaction());
            pet->GetHostileRefManager().setOnlineOfflineState(true);
        }

        CallForAllControlledUnits(SetGameMasterOffHelper(getFaction()), CONTROLLED_PET | CONTROLLED_TOTEMS | CONTROLLED_GUARDIANS | CONTROLLED_CHARM);

        // restore FFA PvP Server state
        if (sWorld.IsFFAPvPRealm())
        {
            SetFFAPvP(true);
        }

        // restore FFA PvP area state, remove not allowed for GM mounts
        UpdateArea(m_areaUpdateId);

        GetHostileRefManager().setOnlineOfflineState(true);
    }

    m_camera.UpdateVisibilityForOwner();
    UpdateObjectVisibility();
    UpdateForQuestWorldObjects();
}

/**
 * @brief Sets whether a game master is visible to other players.
 *
 * @param on True to make the GM visible; false to hide them.
 */
void Player::SetGMVisible(bool on)
{
    if (on)
    {
        m_ExtraFlags &= ~PLAYER_EXTRA_GM_INVISIBLE;         // remove flag

        // Reapply stealth/invisibility if active or show if not any
        if (HasAuraType(SPELL_AURA_MOD_STEALTH))
        {
            SetVisibility(VISIBILITY_GROUP_STEALTH);
        }
        else if (HasAuraType(SPELL_AURA_MOD_INVISIBILITY))
        {
            SetVisibility(VISIBILITY_GROUP_INVISIBILITY);
        }
        else
        {
            SetVisibility(VISIBILITY_ON);
        }
    }
    else
    {
        m_ExtraFlags |= PLAYER_EXTRA_GM_INVISIBLE;          // add flag

        SetAcceptWhispers(false);
        SetGameMaster(true);

        SetVisibility(VISIBILITY_OFF);
    }
}





/**
 * @brief Sends the experience gain log packet to the client.
 *
 * @param GivenXP The base amount of experience awarded.
 * @param victim The kill source, or null for non-kill experience.
 * @param RestXP The rested bonus experience amount.
 */
void Player::SendLogXPGain(uint32 GivenXP, Unit* victim, uint32 RestXP)
{
    WorldPacket data(SMSG_LOG_XPGAIN, 21);
    data << (victim ? victim->GetObjectGuid() : ObjectGuid());// guid
    data << uint32(GivenXP + RestXP);                       // given experience
    data << uint8(victim ? 0 : 1);                          // 00-kill_xp type, 01-non_kill_xp type
    if (victim)
    {
        data << uint32(GivenXP);                            // experience without rested bonus
        data << float(1);                                   // 1 - none 0 - 100% group bonus output
    }
    GetSession()->SendPacket(&data);
}

/**
 * @brief Awards experience to the player and handles level-ups.
 *
 * @param xp The experience amount to award.
 * @param victim The unit responsible for kill-based experience, if any.
 */
void Player::GiveXP(uint32 xp, Unit* victim)
{
    if (xp < 1)
    {
        return;
    }

    if (!IsAlive())
    {
        return;
    }

    uint32 level = getLevel();

    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (Eluna* e = GetEluna())
    {
        e->OnGiveXP(this, xp, victim);
    }
#endif /* ENABLE_ELUNA */

    // XP to money conversion processed in Player::RewardQuest
    if (level >= sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
    {
        return;
    }

    // XP resting bonus for kill
    uint32 rested_bonus_xp = victim ? GetXPRestBonus(xp) : 0;

    SendLogXPGain(xp, victim, rested_bonus_xp);

    uint32 curXP = GetUInt32Value(PLAYER_XP);
    uint32 nextLvlXP = GetUInt32Value(PLAYER_NEXT_LEVEL_XP);
    uint32 newXP = curXP + xp + rested_bonus_xp;

    while (newXP >= nextLvlXP && level < sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
    {
        newXP -= nextLvlXP;

        if (level < sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
        {
            GiveLevel(level + 1);
        }

        level = getLevel();
        nextLvlXP = GetUInt32Value(PLAYER_NEXT_LEVEL_XP);
    }

    SetUInt32Value(PLAYER_XP, newXP);
}

// Update player to next level
// Current player experience not update (must be update by caller)

/**
 * @brief Advances the player to a new level and reapplies level-based stats.
 *
 * @param level The new level to assign.
 */
void Player::GiveLevel(uint32 level)
{
    uint8 oldLevel = getLevel();
    if (level == getLevel())
    {
        return;
    }

    PlayerLevelInfo info;
    sObjectMgr.GetPlayerLevelInfo(getRace(), getClass(), level, &info);

    PlayerClassLevelInfo classInfo;
    sObjectMgr.GetPlayerClassLevelInfo(getClass(), level, &classInfo);

    // send levelup info to client
    WorldPacket data(SMSG_LEVELUP_INFO, (4 + 4 + MAX_POWERS * 4 + MAX_STATS * 4));
    data << uint32(level);
    data << uint32((int32(classInfo.basehealth) - int32(GetCreateHealth())) +
        ((int32(info.stats[STAT_STAMINA]) - GetCreateStat(STAT_STAMINA)) * 10));
    // for (int i = 0; i < MAX_POWERS; ++i)                  // Powers loop (0-6)
    data << uint32(int32(classInfo.basemana)   - int32(GetCreateMana()));
    data << uint32(0);
    data << uint32(0);
    data << uint32(0);
    data << uint32(0);
    // end for
    for (int i = STAT_STRENGTH; i < MAX_STATS; ++i)         // Stats loop (0-4)
    {
        data << uint32(int32(info.stats[i]) - GetCreateStat(Stats(i)));
    }

    GetSession()->SendPacket(&data);

    SetUInt32Value(PLAYER_NEXT_LEVEL_XP, sObjectMgr.GetXPForLevel(level));

    // update level, max level of skills
    if (getLevel() != level)
    {
        m_Played_time[PLAYED_TIME_LEVEL] = 0; // Level Played Time reset
    }
    SetLevel(level);
    UpdateSkillsForLevel();

    // save base values (bonuses already included in stored stats
    for (int i = STAT_STRENGTH; i < MAX_STATS; ++i)
    {
        SetCreateStat(Stats(i), info.stats[i]);
    }

    SetCreateHealth(classInfo.basehealth);
    SetCreateMana(classInfo.basemana);

    InitTalentForLevel();

    UpdateAllStats();

    // set current level health and mana/energy to maximum after applying all mods.
    if (IsAlive())
    {
        SetHealth(GetMaxHealth());
    }
    SetPower(POWER_MANA, GetMaxPower(POWER_MANA));
    SetPower(POWER_ENERGY, GetMaxPower(POWER_ENERGY));
    if (GetPower(POWER_RAGE) > GetMaxPower(POWER_RAGE))
    {
        SetPower(POWER_RAGE, GetMaxPower(POWER_RAGE));
    }
    SetPower(POWER_FOCUS, 0);
    SetPower(POWER_HAPPINESS, 0);

    // update level to hunter/summon pet
    if (Pet* pet = GetPet())
    {
        pet->SynchronizeLevelWithOwner();
    }

    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (Eluna* e = GetEluna())
    {
        e->OnLevelChanged(this, oldLevel);
    }
#endif /* ENABLE_ELUNA */

}

/**
 * @brief Sets the number of free talent points available to the player.
 *
 * @param points The free talent point count.
 */
void Player::SetFreeTalentPoints(uint32 points)
{
    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (Eluna* e = GetEluna())
    {
        e->OnFreeTalentPointsChanged(this, points);
    }
#endif /* ENABLE_ELUNA */

    SetUInt32Value(PLAYER_CHARACTER_POINTS1, points);
}

/**
 * @brief Recalculates the player's free talent points for the current level.
 *
 * @param resetIfNeed True to reset talents when the allocation is invalid.
 */
void Player::UpdateFreeTalentPoints(bool resetIfNeed)
{
    uint32 level = getLevel();
    // talents base at level diff ( talents = level - 9 but some can be used already)
    if (level < 10)
    {
        // Remove all talent points
        if (m_usedTalentCount > 0)                          // Free any used talents
        {
            if (resetIfNeed)
            {
                resetTalents(true);
            }
            SetFreeTalentPoints(0);
        }
    }
    else
    {
        uint32 talentPointsForLevel = CalculateTalentsPoints();

        // if used more that have then reset
        if (m_usedTalentCount > talentPointsForLevel)
        {
            if (resetIfNeed && GetSession()->GetSecurity() < SEC_ADMINISTRATOR)
            {
                resetTalents(true);
            }
            else
            {
                SetFreeTalentPoints(0);
            }
        }
        // else update amount of free points
        else
        {
            SetFreeTalentPoints(talentPointsForLevel - m_usedTalentCount);
        }
    }
}

/**
 * @brief Initializes level-based talent availability for the player.
 */
void Player::InitTalentForLevel()
{
    UpdateFreeTalentPoints();
}

/**
 * @brief Initializes the player's base stats and resources for the current level.
 *
 * @param reapplyMods True to remove and reapply stat modifiers during initialization.
 */
void Player::InitStatsForLevel(bool reapplyMods)
{
    if (reapplyMods)                                        // reapply stats values only on .reset stats (level) command
    {
        _RemoveAllStatBonuses();
    }

    PlayerClassLevelInfo classInfo;
    sObjectMgr.GetPlayerClassLevelInfo(getClass(), getLevel(), &classInfo);

    PlayerLevelInfo info;
    sObjectMgr.GetPlayerLevelInfo(getRace(), getClass(), getLevel(), &info);

    SetUInt32Value(PLAYER_NEXT_LEVEL_XP, sObjectMgr.GetXPForLevel(getLevel()));

    // reset before any aura state sources (health set/aura apply)
    SetUInt32Value(UNIT_FIELD_AURASTATE, 0);

    UpdateSkillsForLevel();

    // set default cast time multiplier
    SetFloatValue(UNIT_MOD_CAST_SPEED, 1.0f);

    // save base values (bonuses already included in stored stats
    for (int i = STAT_STRENGTH; i < MAX_STATS; ++i)
    {
        SetCreateStat(Stats(i), info.stats[i]);
    }

    for (int i = STAT_STRENGTH; i < MAX_STATS; ++i)
    {
        SetStat(Stats(i), info.stats[i]);
    }

    SetCreateHealth(classInfo.basehealth);

    // set create powers
    SetCreateMana(classInfo.basemana);

    SetArmor(int32(m_createStats[STAT_AGILITY] * 2));

    InitStatBuffMods();

    //[-ZERO] SetUInt32Value(PLAYER_FIELD_MOD_HEALING_DONE_POS,0);
    for (int i = 0; i < MAX_SPELL_SCHOOL; ++i)
    {
        SetUInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_NEG + i, 0);
        SetUInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + i, 0);
        SetFloatValue(PLAYER_FIELD_MOD_DAMAGE_DONE_PCT + i, 1.00f);
    }

    // reset attack power, damage and attack speed fields
    SetFloatValue(UNIT_FIELD_BASEATTACKTIME, 2000.0f);
    SetFloatValue(UNIT_FIELD_BASEATTACKTIME + 1, 2000.0f);  // offhand attack time
    SetFloatValue(UNIT_FIELD_RANGEDATTACKTIME, 2000.0f);

    SetFloatValue(UNIT_FIELD_MINDAMAGE, 0.0f);
    SetFloatValue(UNIT_FIELD_MAXDAMAGE, 0.0f);
    SetFloatValue(UNIT_FIELD_MINOFFHANDDAMAGE, 0.0f);
    SetFloatValue(UNIT_FIELD_MAXOFFHANDDAMAGE, 0.0f);
    SetFloatValue(UNIT_FIELD_MINRANGEDDAMAGE, 0.0f);
    SetFloatValue(UNIT_FIELD_MAXRANGEDDAMAGE, 0.0f);

    SetInt32Value(UNIT_FIELD_ATTACK_POWER,            0);
    SetInt32Value(UNIT_FIELD_ATTACK_POWER_MODS,       0);
    SetFloatValue(UNIT_FIELD_ATTACK_POWER_MULTIPLIER, 0.0f);
    SetInt32Value(UNIT_FIELD_RANGED_ATTACK_POWER,     0);
    SetInt32Value(UNIT_FIELD_RANGED_ATTACK_POWER_MODS, 0);
    SetFloatValue(UNIT_FIELD_RANGED_ATTACK_POWER_MULTIPLIER, 0.0f);

    // Base crit values (will be recalculated in UpdateAllStats() at loading and in _ApplyAllStatBonuses() at reset
    SetFloatValue(PLAYER_CRIT_PERCENTAGE, 0.0f);
    SetFloatValue(PLAYER_RANGED_CRIT_PERCENTAGE, 0.0f);

    // Init spell schools (will be recalculated in UpdateAllStats() at loading and in _ApplyAllStatBonuses() at reset
    for (uint8 i = 0; i < MAX_SPELL_SCHOOL; ++i)
    {
        m_SpellCritPercentage[i] = 0.0f;
    }

    SetFloatValue(PLAYER_PARRY_PERCENTAGE, 0.0f);
    SetFloatValue(PLAYER_BLOCK_PERCENTAGE, 0.0f);

    // Dodge percentage
    SetFloatValue(PLAYER_DODGE_PERCENTAGE, 0.0f);

    // set armor (resistance 0) to original value (create_agility*2)
    SetArmor(int32(m_createStats[STAT_AGILITY] * 2));
    SetResistanceBuffMods(SpellSchools(0), true, 0.0f);
    SetResistanceBuffMods(SpellSchools(0), false, 0.0f);
    // set other resistance to original value (0)
    for (int i = 1; i < MAX_SPELL_SCHOOL; ++i)
    {
        SetResistance(SpellSchools(i), 0);
        SetResistanceBuffMods(SpellSchools(i), true, 0.0f);
        SetResistanceBuffMods(SpellSchools(i), false, 0.0f);
    }

    //[-ZERO]    SetUInt32Value(PLAYER_FIELD_MOD_TARGET_RESISTANCE,0);
    //[-ZERO]    SetUInt32Value(PLAYER_FIELD_MOD_TARGET_PHYSICAL_RESISTANCE,0);
    for (int i = 0; i < MAX_SPELL_SCHOOL; ++i)
    {
        SetUInt32Value(UNIT_FIELD_POWER_COST_MODIFIER + i, 0);
        SetFloatValue(UNIT_FIELD_POWER_COST_MULTIPLIER + i, 0.0f);
    }
    // Init data for form but skip reapply item mods for form
    InitDataForForm(reapplyMods);

    // save new stats
    for (int i = POWER_MANA; i < MAX_POWERS; ++i)
    {
        SetMaxPower(Powers(i),  GetCreatePowers(Powers(i)));
    }

    SetMaxHealth(classInfo.basehealth);                     // stamina bonus will applied later

    // cleanup mounted state (it will set correctly at aura loading if player saved at mount.
    SetUInt32Value(UNIT_FIELD_MOUNTDISPLAYID, 0);

    // cleanup unit flags (will be re-applied if need at aura load).
    RemoveFlag(UNIT_FIELD_FLAGS,
        UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_CLIENT_CONTROL_LOST | UNIT_FLAG_NOT_ATTACKABLE_1 |
        UNIT_FLAG_OOC_NOT_ATTACKABLE | UNIT_FLAG_PASSIVE  | UNIT_FLAG_LOOTING          |
        UNIT_FLAG_PET_IN_COMBAT  | UNIT_FLAG_SILENCED     | UNIT_FLAG_PACIFIED         |
        UNIT_FLAG_STUNNED        | UNIT_FLAG_IN_COMBAT    | UNIT_FLAG_DISARMED         |
        UNIT_FLAG_CONFUSED       | UNIT_FLAG_FLEEING      | UNIT_FLAG_NOT_SELECTABLE   |
        UNIT_FLAG_SKINNABLE      | UNIT_FLAG_TAXI_FLIGHT);
    SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PVP_ATTACKABLE);    // must be set

    // cleanup player flags (will be re-applied if need at aura load), to avoid have ghost flag without ghost aura, for example.
    RemoveFlag(PLAYER_FLAGS, PLAYER_FLAGS_AFK | PLAYER_FLAGS_DND | PLAYER_FLAGS_GM | PLAYER_FLAGS_GHOST | PLAYER_FLAGS_FFA_PVP);

    // one form stealth modified bytes
    RemoveByteFlag(UNIT_FIELD_BYTES_1, 3, UNIT_BYTE1_FLAG_ALL);

    // restore if need some important flags
    SetUInt32Value(PLAYER_FIELD_BYTES2, 0);                 // flags empty by default

    if (reapplyMods)                                        // reapply stats values only on .reset stats (level) command
    {
        _ApplyAllStatBonuses();
    }

    // set current level health and mana/energy to maximum after applying all mods.
    SetHealth(GetMaxHealth());
    SetPower(POWER_MANA, GetMaxPower(POWER_MANA));
    SetPower(POWER_ENERGY, GetMaxPower(POWER_ENERGY));
    if (GetPower(POWER_RAGE) > GetMaxPower(POWER_RAGE))
    {
        SetPower(POWER_RAGE, GetMaxPower(POWER_RAGE));
    }
    SetPower(POWER_FOCUS, 0);
    SetPower(POWER_HAPPINESS, 0);

    // update level to hunter/summon pet
    if (Pet* pet = GetPet())
    {
        pet->SynchronizeLevelWithOwner();
    }
}

struct spell_data
{
    uint16 spell_id; // Sent together with unk2 for a total of 4 bytes per spell
    uint16 on_cooldown;     // zero every time
};

struct spell_cooldown_data
{
    uint16 spell_id;          // ID of spell on cooldown
    uint16 item_id;           // ID of item on cooldown (can be 0)
    uint16 spell_category;    // Category of spell on cooldown (but not the category that IS on cooldown, just the category the spell belongs to)
    uint32 spell_cd_ms;       // Amount of time in ms spell is on cooldown for
    uint32 cat_cd_ms;         // Amount of time in ms that category is on cooldown for
};

/* Used during Player::SendInitialPacketsBeforeAddToMap */

/**
 * @brief Sends the initial spellbook and cooldown state to the client.
 */
void Player::SendInitialSpells()
{
    time_t curTime = time(NULL);
    time_t infTime = curTime + infinityCooldownDelayCheck;

    /** * * * * * * * * * * * * * * * *
     * * START OF PACKET STRUCTURE * *
     * * * * * * * * * * * * * * * * */
    uint16 spellCount = 0;

    WorldPacket data(SMSG_INITIAL_SPELLS, (1 + 2 + 4 * m_spells.size() + 2 + m_spellCooldowns.size() * (2 + 2 + 2 + 4 + 4)));
    data << uint8(0);

    /** * * * * * * * * * * * * * * * *
     * *  END OF PACKET STRUCTURE  * *
     * * * * * * * * * * * * * * * * */
    size_t countPos = data.wpos();
    data << uint16(spellCount);                             // spell count placeholder

    /* For each spell the player knows */
    for (PlayerSpellMap::const_iterator itr = m_spells.begin(); itr != m_spells.end(); ++itr)
    {
        /* If the spell is marked as removed, don't send it */
        PlayerSpell const& playerSpell = itr->second;

        if (playerSpell.state == PLAYERSPELL_REMOVED)
        {
            continue;
        }

        if (!playerSpell.active || playerSpell.disabled)
        {
            continue;
        }

        /* Insert spell into vector for insertion into packet */
        data << uint16(itr->first);
        data << uint16(0);                                  // it's not slot id

        /* Increase spell counter by 1 (sent in packet) */
        spellCount += 1;
    }

    data.put<uint16>(countPos, spellCount);                 // write real count value

    /* For each spell the player has on cooldown */
    uint16 spellCooldowns = m_spellCooldowns.size();
    data << uint16(spellCooldowns);
    for (SpellCooldowns::const_iterator itr = m_spellCooldowns.begin(); itr != m_spellCooldowns.end(); ++itr)
    {
        /* If the spell doesn't exist in the spellbook, just ignore it */
        SpellEntry const* sEntry = sSpellStore.LookupEntry(itr->first);
        if (!sEntry)
        {
            continue;
        }

        SpellCooldown const& spellCooldown = itr->second;

        data << uint16(itr->first);

        data << uint16(spellCooldown.itemid);               // cast item id
        data << uint16(sEntry->Category);                   // spell category

        /* send infinity cooldown in special format */
        if (spellCooldown.end >= infTime)
        {
            data << uint32(1);                              // cooldown
            data << uint32(0x80000000);                     // category cooldown
            continue;
        }

        time_t cooldown = spellCooldown.end > curTime ? (spellCooldown.end - curTime) * IN_MILLISECONDS : 0;

        if (sEntry->Category)                               // may be wrong, but anyway better than nothing...
        {
            data << uint32(0);                              // cooldown
            data << uint32(cooldown);                       // category cooldown
        }
        else
        {
            data << uint32(cooldown);                       // cooldown
            data << uint32(0);                              // category cooldown
        }
    }

    GetSession()->SendPacket(&data);

    DETAIL_LOG("CHARACTER: Sent Initial Spells");
}










/**
 * @brief Removes a cooldown entry for a specific spell.
 *
 * @param spell_id The spell identifier whose cooldown should be removed.
 * @param update True to notify the client that the cooldown was cleared.
 */
void Player::RemoveSpellCooldown(uint32 spell_id, bool update /* = false */)
{
    m_spellCooldowns.erase(spell_id);

    if (update)
    {
        SendClearCooldown(spell_id, this);
    }
}

/**
 * @brief Removes cooldowns for all spells in a cooldown category.
 *
 * @param cat The spell category identifier.
 * @param update True to notify the client for cleared cooldowns.
 */
void Player::RemoveSpellCategoryCooldown(uint32 cat, bool update /* = false */)
{
    SpellCategoryStore::const_iterator ct = sSpellCategoryStore.find(cat);
    if (ct == sSpellCategoryStore.end())
    {
        return;
    }

    const SpellCategorySet& ct_set = ct->second;
    for (SpellCooldowns::const_iterator i = m_spellCooldowns.begin(); i != m_spellCooldowns.end();)
    {
        if (ct_set.find(i->first) != ct_set.end())
        {
            RemoveSpellCooldown((i++)->first, update);
        }
        else
        {
            ++i;
        }
    }
}

/**
 * @brief Clears all tracked spell cooldowns for the player.
 */
void Player::RemoveAllSpellCooldown()
{
    if (!m_spellCooldowns.empty())
    {
        for (SpellCooldowns::const_iterator itr = m_spellCooldowns.begin(); itr != m_spellCooldowns.end(); ++itr)
        {
            SendClearCooldown(itr->first, this);
        }

        m_spellCooldowns.clear();
    }
}

/**
 * @brief Loads persisted spell cooldowns from the database query result.
 *
 * @param result The query result containing cooldown rows.
 */
void Player::_LoadSpellCooldowns(QueryResult* result)
{
    // some cooldowns can be already set at aura loading...

    // QueryResult *result = CharacterDatabase.PQuery("SELECT `spell`,`item`,`time` FROM `character_spell_cooldown` WHERE `guid` = '%u'",GetGUIDLow());

    if (result)
    {
        time_t curTime = time(NULL);

        do
        {
            Field* fields = result->Fetch();

            uint32 spell_id = fields[0].GetUInt32();
            uint32 item_id  = fields[1].GetUInt32();
            time_t db_time  = (time_t)fields[2].GetUInt64();

            if (!sSpellStore.LookupEntry(spell_id))
            {
                sLog.outError("Player %u has unknown spell %u in `character_spell_cooldown`, skipping.", GetGUIDLow(), spell_id);
                continue;
            }

            // skip outdated cooldown
            if (db_time <= curTime)
            {
                continue;
            }

            AddSpellCooldown(spell_id, item_id, db_time);

            DEBUG_LOG("Player (GUID: %u) spell %u, item %u cooldown loaded (%u secs).", GetGUIDLow(), spell_id, item_id, uint32(db_time - curTime));
        }
        while (result->NextRow());

        delete result;
    }
}

/**
 * @brief Saves active spell cooldowns to the database.
 */
void Player::_SaveSpellCooldowns()
{
    static SqlStatementID deleteSpellCooldown ;
    static SqlStatementID insertSpellCooldown ;

    SqlStatement stmt = CharacterDatabase.CreateStatement(deleteSpellCooldown, "DELETE FROM `character_spell_cooldown` WHERE `guid` = ?");
    stmt.PExecute(GetGUIDLow());

    time_t curTime = time(NULL);
    time_t infTime = curTime + infinityCooldownDelayCheck;

    // remove outdated and save active
    for (SpellCooldowns::iterator itr = m_spellCooldowns.begin(); itr != m_spellCooldowns.end();)
    {
        if (itr->second.end <= curTime)
        {
            m_spellCooldowns.erase(itr++);
        }
        else if (itr->second.end <= infTime)                // not save locked cooldowns, it will be reset or set at reload
        {
            stmt = CharacterDatabase.CreateStatement(insertSpellCooldown, "INSERT INTO `character_spell_cooldown` (`guid`,`spell`,`item`,`time`) VALUES( ?, ?, ?, ?)");
            stmt.PExecute(GetGUIDLow(), itr->first, itr->second.itemid, uint64(itr->second.end));
            ++itr;
        }
        else
        {
            ++itr;
        }
    }
}












/**
 * Deletes a character from the database
 *
 * The way, how the characters will be deleted is decided based on the config option.
 *
 * @see Player::DeleteOldCharacters
 *
 * @param playerguid       the low-GUID from the player which should be deleted
 * @param accountId        the account id from the player
 * @param updateRealmChars when this flag is set, the amount of characters on that realm will be updated in the realmlist
 * @param deleteFinally    if this flag is set, the config option will be ignored and the character will be permanently removed from the database
 */
void Player::DeleteFromDB(ObjectGuid playerguid, uint32 accountId, bool updateRealmChars, bool deleteFinally)
{
    //Make sure to delete unresolved tickets so they don't take up place in the open tickets list
    CharacterDatabase.PExecute("DELETE FROM `character_ticket` "
        "WHERE `resolved` = 0 AND `guid` = %u",
        playerguid.GetCounter());

    // for nonexistent account avoid update realm
    if (accountId == 0)
    {
        updateRealmChars = false;
    }

    uint32 charDelete_method = sWorld.getConfig(CONFIG_UINT32_CHARDELETE_METHOD);
    uint32 charDelete_minLvl = sWorld.getConfig(CONFIG_UINT32_CHARDELETE_MIN_LEVEL);

    // if we want to finally delete the character or the character does not meet the level requirement, we set it to mode 0
    if (deleteFinally || Player::GetLevelFromDB(playerguid) < charDelete_minLvl)
    {
        charDelete_method = 0;
    }

    uint32 lowguid = playerguid.GetCounter();

    // convert corpse to bones if exist (to prevent exiting Corpse in World without DB entry)
    // bones will be deleted by corpse/bones deleting thread shortly
    sObjectAccessor.ConvertCorpseForPlayer(playerguid);

    // remove from guild
    if (uint32 guildId = GetGuildIdFromDB(playerguid))
    {
        if (Guild* guild = sGuildMgr.GetGuildById(guildId))
        {
            if (guild->DelMember(playerguid))
            {
                guild->Disband();
                delete guild;
            }
        }
    }

    // the player was uninvited already on logout so just remove from group
    QueryResult* resultGroup = CharacterDatabase.PQuery("SELECT `groupId` FROM `group_member` WHERE `memberGuid`='%u'", lowguid);
    if (resultGroup)
    {
        uint32 groupId = (*resultGroup)[0].GetUInt32();
        delete resultGroup;
        if (Group* group = sObjectMgr.GetGroupById(groupId))
        {
            RemoveFromGroup(group, playerguid);
        }
    }

    // remove signs from petitions (also remove petitions if owner);
    RemovePetitionsAndSigns(playerguid);

    switch (charDelete_method)
    {
        // completely remove from the database
        case 0:
        {
            // return back all mails with COD and Item                  0    1             2                3        4         5      6       7
            QueryResult* resultMail = CharacterDatabase.PQuery("SELECT `id`,`messageType`,`mailTemplateId`,`sender`,`subject`,`body`,`money`,`has_items` FROM `mail` WHERE `receiver`='%u' AND `has_items`<>0 AND `cod`<>0", lowguid);
            if (resultMail)
            {
                do
                {
                    Field* fields = resultMail->Fetch();

                    uint32 mail_id       = fields[0].GetUInt32();
                    uint16 mailType      = fields[1].GetUInt16();
                    uint16 mailTemplateId = fields[2].GetUInt16();
                    uint32 sender        = fields[3].GetUInt32();
                    std::string subject  = fields[4].GetCppString();
                    std::string body     = fields[5].GetCppString();
                    uint32 money         = fields[6].GetUInt32();
                    bool has_items       = fields[7].GetBool();

                    // we can return mail now
                    // so firstly delete the old one
                    CharacterDatabase.PExecute("DELETE FROM `mail` WHERE `id` = '%u'", mail_id);

                    // mail not from player
                    if (mailType != MAIL_NORMAL)
                    {
                        if (has_items)
                        {
                            CharacterDatabase.PExecute("DELETE FROM `mail_items` WHERE `mail_id` = '%u'", mail_id);
                        }
                        continue;
                    }

                    MailDraft draft(subject, body);
                    if (mailTemplateId)
                    {
                        draft.SetMailTemplate(mailTemplateId, false); // items already included
                    }
                    else
                    {
                        draft.SetSubjectAndBody(subject, body);
                    }

                    if (has_items)
                    {
                        // data needs to be at first place for Item::LoadFromDB
                        //                                                           0      1      2           3
                        QueryResult* resultItems = CharacterDatabase.PQuery("SELECT `data`,`text`,`item_guid`,`item_template` FROM `mail_items` JOIN `item_instance` ON `item_guid` = `guid` WHERE `mail_id`='%u'", mail_id);
                        if (resultItems)
                        {
                            do
                            {
                                Field* fields2 = resultItems->Fetch();

                                uint32 item_guidlow = fields2[2].GetUInt32();
                                uint32 item_template = fields2[3].GetUInt32();

                                ItemPrototype const* itemProto = ObjectMgr::GetItemPrototype(item_template);
                                if (!itemProto)
                                {
                                    CharacterDatabase.PExecute("DELETE FROM `item_instance` WHERE `guid` = '%u'", item_guidlow);
                                    continue;
                                }

                                Item* pItem = NewItemOrBag(itemProto);
                                if (!pItem->LoadFromDB(item_guidlow, fields2, playerguid))
                                {
                                    pItem->FSetState(ITEM_REMOVED);
                                    pItem->SaveToDB();      // it also deletes item object !
                                    continue;
                                }

                                draft.AddItem(pItem);
                            }
                            while (resultItems->NextRow());

                            delete resultItems;
                        }
                    }

                    CharacterDatabase.PExecute("DELETE FROM `mail_items` WHERE `mail_id` = '%u'", mail_id);

                    uint32 pl_account = sObjectMgr.GetPlayerAccountIdByGUID(playerguid);

                    draft.SetMoney(money).SendReturnToSender(pl_account, playerguid, ObjectGuid(HIGHGUID_PLAYER, sender));
                }
                while (resultMail->NextRow());

                delete resultMail;
            }

            // unsummon and delete for pets in world is not required: player deleted from CLI or character list with not loaded pet.
            // Get guids of character's pets, will deleted in transaction
            QueryResult* resultPets = CharacterDatabase.PQuery("SELECT `id` FROM `character_pet` WHERE `owner` = '%u'", lowguid);

            // delete char from friends list when selected chars is online (non existing - error)
            QueryResult* resultFriend = CharacterDatabase.PQuery("SELECT DISTINCT `guid` FROM `character_social` WHERE `friend` = '%u'", lowguid);

            // NOW we can finally clear other DB data related to character
            CharacterDatabase.BeginTransaction();
            if (resultPets)
            {
                do
                {
                    Field* fields3 = resultPets->Fetch();
                    uint32 petguidlow = fields3[0].GetUInt32();
                    // do not create separate transaction for pet delete otherwise we will get fatal error!
                    Pet::DeleteFromDB(petguidlow, false);
                }
                while (resultPets->NextRow());
                delete resultPets;
            }

            // cleanup friends for online players, offline case will cleanup later in code
            if (resultFriend)
            {
                do
                {
                    Field* fieldsFriend = resultFriend->Fetch();
                    if (Player* sFriend = sObjectAccessor.FindPlayer(ObjectGuid(HIGHGUID_PLAYER, fieldsFriend[0].GetUInt32())))
                    {
                        if (sFriend->IsInWorld())
                        {
                            sFriend->GetSocial()->RemoveFromSocialList(playerguid, false);
                            sSocialMgr.SendFriendStatus(sFriend, FRIEND_REMOVED, playerguid, false);
                        }
                    }
                }
                while (resultFriend->NextRow());
                delete resultFriend;
            }

            CharacterDatabase.PExecute("DELETE FROM `characters` WHERE `guid` = '%u'", lowguid);
            CharacterDatabase.PExecute("DELETE FROM `character_action` WHERE `guid` = '%u'", lowguid);
            CharacterDatabase.PExecute("DELETE FROM `character_aura` WHERE `guid` = '%u'", lowguid);
            CharacterDatabase.PExecute("DELETE FROM `character_battleground_data` WHERE `guid` = '%u'", lowguid);
            CharacterDatabase.PExecute("DELETE FROM `character_gifts` WHERE `guid` = '%u'", lowguid);
            CharacterDatabase.PExecute("DELETE FROM `character_homebind` WHERE `guid` = '%u'", lowguid);
            CharacterDatabase.PExecute("DELETE FROM `character_instance` WHERE `guid` = '%u'", lowguid);
            CharacterDatabase.PExecute("DELETE FROM `group_instance` WHERE `leaderGuid` = '%u'", lowguid);
            CharacterDatabase.PExecute("DELETE FROM `character_inventory` WHERE `guid` = '%u'", lowguid);
            CharacterDatabase.PExecute("DELETE FROM `character_queststatus` WHERE `guid` = '%u'", lowguid);
            CharacterDatabase.PExecute("DELETE FROM `character_reputation` WHERE `guid` = '%u'", lowguid);
            CharacterDatabase.PExecute("DELETE FROM `character_skills` WHERE `guid` = '%u'", lowguid);
            CharacterDatabase.PExecute("DELETE FROM `character_spell` WHERE `guid` = '%u'", lowguid);
            CharacterDatabase.PExecute("DELETE FROM `character_spell_cooldown` WHERE `guid` = '%u'", lowguid);
            CharacterDatabase.PExecute("DELETE FROM `character_ticket` WHERE `guid` = '%u'", lowguid);
            CharacterDatabase.PExecute("DELETE FROM `item_instance` WHERE `owner_guid` = '%u'", lowguid);
            CharacterDatabase.PExecute("DELETE FROM `character_social` WHERE `guid` = '%u' OR `friend`='%u'", lowguid, lowguid);
            CharacterDatabase.PExecute("DELETE FROM `mail` WHERE `receiver` = '%u'", lowguid);
            CharacterDatabase.PExecute("DELETE FROM `mail_items` WHERE `receiver` = '%u'", lowguid);
            CharacterDatabase.PExecute("DELETE FROM `character_pet` WHERE `owner` = '%u'", lowguid);
            CharacterDatabase.PExecute("DELETE FROM `guild_eventlog` WHERE `PlayerGuid1` = '%u' OR `PlayerGuid2` = '%u'", lowguid, lowguid);
            CharacterDatabase.CommitTransaction();
            break;
        }
        // The character gets unlinked from the account, the name gets freed up and appears as deleted ingame
        case 1:
            CharacterDatabase.PExecute("UPDATE `characters` SET `deleteInfos_Name`=`name`, `deleteInfos_Account`=`account`, `deleteDate`='" UI64FMTD "', `name`='', `account`=0 WHERE `guid`=%u", uint64(time(NULL)), lowguid);
            break;
        default:
            sLog.outError("Player::DeleteFromDB: Unsupported delete method: %u.", charDelete_method);
    }

    if (updateRealmChars)
    {
        sWorld.UpdateRealmCharCount(accountId);
    }
}

/**
 * Characters which were kept back in the database after being deleted and are now too old (see config option "CharDelete.KeepDays"), will be completely deleted.
 *
 * @see Player::DeleteFromDB
 */
void Player::DeleteOldCharacters()
{
    uint32 keepDays = sWorld.getConfig(CONFIG_UINT32_CHARDELETE_KEEP_DAYS);
    if (!keepDays)
    {
        return;
    }

    Player::DeleteOldCharacters(keepDays);
}

/**
 * Characters which were kept back in the database after being deleted and are older than the specified amount of days, will be completely deleted.
 *
 * @see Player::DeleteFromDB
 *
 * @param keepDays overrite the config option by another amount of days
 */
void Player::DeleteOldCharacters(uint32 keepDays)
{
    sLog.outString("Player::DeleteOldChars: Deleting all characters which have been deleted %u days before...", keepDays);

    QueryResult* resultChars = CharacterDatabase.PQuery("SELECT `guid`, `deleteInfos_Account` FROM `characters` WHERE `deleteDate` IS NOT NULL AND `deleteDate` < '" UI64FMTD "'", uint64(time(NULL) - time_t(keepDays * DAY)));
    if (resultChars)
    {
        sLog.outString("Player::DeleteOldChars: Found %u character(s) to delete", uint32(resultChars->GetRowCount()));
        do
        {
            Field* charFields = resultChars->Fetch();
            ObjectGuid guid = ObjectGuid(HIGHGUID_PLAYER, charFields[0].GetUInt32());
            Player::DeleteFromDB(guid, charFields[1].GetUInt32(), true, true);
        }
        while (resultChars->NextRow());
        delete resultChars;
    }
    sLog.outString();
}


























/**
 * @brief Attempts to improve defense skill and refresh defense-derived bonuses.
 */
void Player::UpdateDefense()
{
    uint32 defense_skill_gain = sWorld.getConfig(CONFIG_UINT32_SKILL_GAIN_DEFENSE);

    if (UpdateSkill(SKILL_DEFENSE, defense_skill_gain))
    {
        // update dependent from defense skill part
        UpdateDefenseBonusesMod();
    }
}































/* Called from Player::SendInitialPacketsBeforeAddToMap */





/**
 * @brief Moves the player to a new position and updates related state.
 *
 * @param x The destination X coordinate.
 * @param y The destination Y coordinate.
 * @param z The destination Z coordinate.
 * @param orientation The destination facing angle.
 * @param teleport True if the move should be treated as a teleport.
 * @return True if the position update succeeded; otherwise, false.
 */
bool Player::SetPosition(float x, float y, float z, float orientation, bool teleport)
{
    // prevent crash when a bad coord is sent by the client
    if (!MaNGOS::IsValidMapCoord(x, y, z, orientation))
    {
        DEBUG_LOG("Player::SetPosition(%f, %f, %f, %f, %d) .. bad coordinates for player %d!", x, y, z, orientation, teleport, GetGUIDLow());
        return false;
    }

    Map* m = GetMap();

    const float old_x = GetPositionX();
    const float old_y = GetPositionY();
    const float old_z = GetPositionZ();
    const float old_r = GetOrientation();

    if (teleport || old_x != x || old_y != y || old_z != z || old_r != orientation)
    {
        if (teleport || old_x != x || old_y != y || old_z != z)
        {
            RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_MOVE | AURA_INTERRUPT_FLAG_TURNING);
        }
        else
        {
            RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_TURNING);
        }

        RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);

        // move and update visible state if need
        m->PlayerRelocation(this, x, y, z, orientation);

        // reread after Map::Relocation
        m = GetMap();
        x = GetPositionX();
        y = GetPositionY();
        z = GetPositionZ();

        // group update
        if (GetGroup() && (old_x != x || old_y != y))
        {
            SetGroupUpdateFlag(GROUP_UPDATE_FLAG_POSITION);
        }
        if (GetTrader() && !IsWithinDistInMap(GetTrader(), INTERACTION_DISTANCE))
        {
            GetSession()->SendCancelTrade(); // will close both side trade windows
        }
    }

    if (m_positionStatusUpdateTimer)                        // Update position's state only on interval
    {
        return true;
    }
    m_positionStatusUpdateTimer = 100;

    // code block for underwater state update
    UpdateUnderwaterState(m, x, y, z);

    // code block for outdoor state and area-explore check
    CheckAreaExploreAndOutdoor();

    return true;
}

/**
 * @brief Saves the player's current position as the recall location.
 */
void Player::SaveRecallPosition()
{
    m_recallMap = GetMapId();
    m_recallX = GetPositionX();
    m_recallY = GetPositionY();
    m_recallZ = GetPositionZ();
    m_recallO = GetOrientation();
}

/**
 * @brief Broadcasts a packet to nearby clients and optionally to the player.
 *
 * @param data The packet to send.
 * @param self True to also send the packet to the player session.
 */
void Player::SendMessageToSet(WorldPacket* data, bool self) const
{
    if (IsInWorld())
    {
        GetMap()->MessageBroadcast(this, data, false);
    }

    // if player is not in world and map in not created/already destroyed
    // no need to create one, just send packet for itself!
    if (self)
    {
        GetSession()->SendPacket(data);
    }
}

/**
 * @brief Broadcasts a packet to nearby clients within a distance and optionally to the player.
 *
 * @param data The packet to send.
 * @param dist The maximum broadcast distance.
 * @param self True to also send the packet to the player session.
 */
void Player::SendMessageToSetInRange(WorldPacket* data, float dist, bool self) const
{
    if (IsInWorld())
    {
        GetMap()->MessageDistBroadcast(this, data, dist, false);
    }

    if (self)
    {
        GetSession()->SendPacket(data);
    }
}

/**
 * @brief Broadcasts a packet within range with optional team filtering.
 *
 * @param data The packet to send.
 * @param dist The maximum broadcast distance.
 * @param self True to also send the packet to the player session.
 * @param own_team_only True to restrict delivery to the player's team.
 */
void Player::SendMessageToSetInRange(WorldPacket* data, float dist, bool self, bool own_team_only) const
{
    if (IsInWorld())
    {
        GetMap()->MessageDistBroadcast(this, data, dist, false, own_team_only);
    }

    if (self)
    {
        GetSession()->SendPacket(data);
    }
}

/**
 * @brief Sends a packet directly to the player's session.
 *
 * @param data The packet to send.
 */
void Player::SendDirectMessage(WorldPacket* data) const
{
    GetSession()->SendPacket(data);
}

/**
 * @brief Starts a cinematic sequence for the player client.
 *
 * @param CinematicSequenceId The cinematic sequence identifier.
 */
void Player::SendCinematicStart(uint32 CinematicSequenceId)
{
    WorldPacket data(SMSG_TRIGGER_CINEMATIC, 4);
    data << uint32(CinematicSequenceId);
    SendDirectMessage(&data);
}

#if defined (WOTLK) || defined (CATA) || defined (MISTS)

/**
 * @brief Starts a movie sequence for the player client.
 *
 * @param MovieId The movie identifier.
 */
void Player::SendMovieStart(uint32 MovieId)
{
    WorldPacket data(SMSG_TRIGGER_MOVIE, 4);
    data << uint32(MovieId);
    SendDirectMessage(&data);
}
#endif


/**
 * @brief Gets the faction team associated with a race.
 *
 * @param race The race identifier to evaluate.
 * @return The team assigned to the race.
 */
Team Player::TeamForRace(uint8 race)
{
    ChrRacesEntry const* rEntry = sChrRacesStore.LookupEntry(race);
    if (!rEntry)
    {
        sLog.outError("Race %u not found in DBC: wrong DBC files?", uint32(race));
        return ALLIANCE;
    }

    switch (rEntry->TeamID)
    {
        case 7: return ALLIANCE;
        case 1: return HORDE;
    }

    sLog.outError("Race %u have wrong teamid %u in DBC: wrong DBC files?", uint32(race), rEntry->TeamID);
    return TEAM_NONE;
}

/**
 * @brief Gets the faction template associated with a race.
 *
 * @param race The race identifier to evaluate.
 * @return The faction template identifier for the race.
 */
uint32 Player::getFactionForRace(uint8 race)
{
    ChrRacesEntry const* rEntry = sChrRacesStore.LookupEntry(race);
    if (!rEntry)
    {
        sLog.outError("Race %u not found in DBC: wrong DBC files?", uint32(race));
        return 0;
    }

    return rEntry->FactionID;
}

/**
 * @brief Sets the player's team and faction from the specified race.
 *
 * @param race The race identifier to apply.
 */
void Player::setFactionForRace(uint8 race)
{
    m_team = TeamForRace(race);
    setFaction(getFactionForRace(race));
}





// Update honor fields , cleanKills is only used during char saving
void Player::UpdateHonor()
{
    //uint32 lastweek_honorableKills = 0;
    uint32 today_honorableKills = 0;
    uint32 today_dishonorableKills = 0;
    uint32 yesterdayKills = 0;
    uint32 thisWeekKills = 0;
    uint32 lastWeekKills = 0;

    float yesterdayHonor = 0.0f;
    float thisWeekHonor = 0.0f;
    float lastWeekHonor = 0.0f;
    //float todayLostHonor = 0.0f;

    uint32 today = sWorld.GetDateToday();

    uint32 yesterday     = today - 1;
    uint32 thisWeekBegin = sWorld.GetDateLastMaintenanceDay();
    uint32 thisWeekEnd   = thisWeekBegin + 7;
    uint32 lastWeekBegin = thisWeekBegin - 7;
    uint32 lastWeekEnd   = lastWeekBegin + 7;

    DETAIL_LOG("PLAYER: UpdateHonor");

    uint32 total_dishonorableKills = GetHonorStoredKills(false);
    uint32 total_honorableKills = GetHonorStoredKills(true);

    for (HonorCPMap::iterator itr = m_honorCP.begin(); itr != m_honorCP.end(); ++itr)
    {
        if (itr->state == HK_DELETED)
        {
            continue;
        }

        if (itr->type == HONORABLE)
        {
            if (itr->isKill)
            {
                total_honorableKills++;
            }

            if (itr->isKill && itr->date == today)
            {
                today_honorableKills++;
            }

            if (itr->date == yesterday)
            {
                if (itr->isKill)
                {
                    yesterdayKills++;
                }
                yesterdayHonor += itr->honorPoints;
            }
            if ((itr->date >= thisWeekBegin) && (itr->date <= thisWeekEnd))
            {
                if (itr->isKill)
                {
                    thisWeekKills++;
                }
                thisWeekHonor += itr->honorPoints;
            }
            if ((itr->date >= lastWeekBegin) && (itr->date < lastWeekEnd))
            {
                if (itr->isKill)
                {
                    lastWeekKills++;
                }
                lastWeekHonor += itr->honorPoints;
            }
        }
        else if (itr->isKill && itr->type == DISHONORABLE)
        {
            total_dishonorableKills++;

            if (itr->date == today)
            {
                today_dishonorableKills++;
            }

            if (itr->date > today)
            {
                itr->state = HK_OLD;
            }
        }
    }

    // STANDING
    SetHonorLastWeekStandingPos(sObjectMgr.GetHonorStandingPositionByGUID(GetGUIDLow(), GetTeam()));

    // RANK POINTS
    HonorStanding* standing = sObjectMgr.GetHonorStandingByGUID(GetGUIDLow(), GetTeam());
    float rankP = GetStoredHonor();
    if (standing)
    {
        rankP += standing->rpEarning;
    }

    SetRankPoints(rankP);

    // RIGHEST RANK
    // If the new rank is highest then the old one, then m_highest_rank is updated
    HonorRankInfo prk =  MaNGOS::Honor::CalculateHonorRank(GetRankPoints());
    SetHonorRankInfo(prk);
    if (prk.visualRank > 0 && prk.visualRank > GetHonorHighestRankInfo().visualRank)
    {
        SetHonorHighestRankInfo(prk);
    }

    // rank points is sent to client with same size of uint8(255) for each rank
    // so we set it in correct rate:
    uint32 RP = uint32(GetRankPoints() >= 0 ? GetRankPoints() : -1 * GetRankPoints());
    RP = int8(((RP - prk.minRP) / (prk.maxRP - prk.minRP)) * (prk.positive ? 255 : -255));

    // NEXT RANK BAR
    // PLAYER_FIELD_HONOR_BAR
    SetByteValue(PLAYER_FIELD_BYTES2, 0, RP);
    // RANK (Patent)
    SetByteValue(PLAYER_BYTES_3, 3, GetHonorRankInfo().rank);
    // TODAY
    SetUInt16Value(PLAYER_FIELD_SESSION_KILLS, 0, today_honorableKills);
    SetUInt16Value(PLAYER_FIELD_SESSION_KILLS, 1, today_dishonorableKills);
    // YESTERDAY
    SetUInt32Value(PLAYER_FIELD_YESTERDAY_KILLS, yesterdayKills);
    SetUInt32Value(PLAYER_FIELD_YESTERDAY_CONTRIBUTION, (uint32)(yesterdayHonor > 0.0f ? yesterdayHonor : 0.0f));
    // THIS WEEK
    SetUInt32Value(PLAYER_FIELD_THIS_WEEK_KILLS, thisWeekKills);
    SetUInt32Value(PLAYER_FIELD_THIS_WEEK_CONTRIBUTION, (uint32)(thisWeekHonor > 0.0f ? thisWeekHonor : 0.0f));
    // LAST WEEK
    SetUInt32Value(PLAYER_FIELD_LAST_WEEK_KILLS, lastWeekKills);
    SetUInt32Value(PLAYER_FIELD_LAST_WEEK_CONTRIBUTION, (uint32)(lastWeekHonor > 0.0f ? lastWeekHonor : 0.0f));
    SetUInt32Value(PLAYER_FIELD_LAST_WEEK_RANK, GetHonorLastWeekStandingPos());
    // LIFE TIME
    SetUInt32Value(PLAYER_FIELD_LIFETIME_HONORABLE_KILLS, total_honorableKills);
    SetUInt32Value(PLAYER_FIELD_LIFETIME_DISHONORABLE_KILLS, total_dishonorableKills);
    // TODO: Into what field we need to set it? Fix it!
    // SetUInt32Value(PLAYER_FIELD_PVP_MEDALS/*???*/, (GetHonorHighestRankInfo().rank << 24) | 0x0F0001);
    // ITEM FIELD RANK REQUIRED
    SetByteValue(PLAYER_FIELD_BYTES, 3, GetHonorHighestRankInfo().rank);
}

/**
 * @brief Permanently clears all stored honor contribution data for the player.
 */
void Player::ResetHonor()
{
    // it will delete all honor permanently
    CharacterDatabase.PExecute("DELETE FROM `character_honor_cp` WHERE `guid` = '%u'", GetGUIDLow());
    ClearHonorInfo();
    UpdateHonor();
}

// set all honor info to default
void Player::ClearHonorInfo()
{
    m_honorCP.clear();
    SetHonorStoredKills(0, true);
    SetHonorStoredKills(0, false);
    SetStoredHonor(0);
    SetHonorLastWeekStandingPos(0);
    MaNGOS::Honor::InitRankInfo(m_honor_rank);
    MaNGOS::Honor::InitRankInfo(m_highest_rank);
}

// How many times Player kill pVictim... ( toDate shouldn't be > lastWeekBegin )
uint32 Player::CalculateTotalKills(Unit* Victim, uint32 fromDate, uint32 toDate) const
{
    uint32 total_kills = 0;
    uint32 ID = 0;

    if (!Victim)
    {
        return 0;
    }

    uint8 vType = Victim->GetTypeId();

    switch (vType)
    {
        case TYPEID_PLAYER:
            ID = ((Player*)Victim)->GetGUIDLow();
            break;
        case TYPEID_UNIT: // actually kills count not used for NPCs
            ID = Victim->GetEntry();
            break;
        default: // cheat?
            return 0;
    }

    for (HonorCPMap::const_iterator itr = m_honorCP.begin(); itr != m_honorCP.end(); ++itr)
    {
        if (itr->victimType != TYPEID_OBJECT && itr->victimType == vType && itr->victimID == ID)
        {
            if (itr->date >= fromDate && itr->date <= toDate)
            {
                total_kills ++;
            }
        }
    }
    return total_kills;
}

// How much honor Player gains/loses killing uVictim
bool Player::RewardHonor(Unit* uVictim, uint32 groupsize)
{
    float honor_points = 0;
    //int kill_type = 0;

    DETAIL_LOG("PLAYER: RewardHonor");

    if (!uVictim)
    {
        return false;
    }

    if (uVictim->GetAura(2479, EFFECT_INDEX_0))             // Honorless Target
    {
        return false;
    }

    if (uVictim->GetTypeId() == TYPEID_UNIT)
    {
        Creature* cVictim = (Creature*)uVictim;
        if (cVictim->IsCivilian())
        {
            AddHonorCP(MaNGOS::Honor::DishonorableKillPoints(getLevel()), DISHONORABLE, cVictim->GetEntry(), TYPEID_UNIT);
            return true;
        }

        if (cVictim->IsRacialLeader())
        {
            // maybe uncorrect honor value but no source to get it actually
            AddHonorCP(398.0, HONORABLE, cVictim->GetEntry(), TYPEID_UNIT);
            // Send PvP credit racial leader
            SendPvPCredit(cVictim->GetObjectGuid(), 19, 398);
            return true;
        }
    }
    else if (uVictim->GetTypeId() == TYPEID_PLAYER)
    {
        Player* pVictim = (Player*)uVictim;

        if (GetTeam() == pVictim->GetTeam())
        {
            return false;
        }

        if (getLevel() < (pVictim->getLevel() + 5))
        {
            float hp = MaNGOS::Honor::HonorableKillPoints(this, pVictim, groupsize);
            AddHonorCP(hp, HONORABLE, pVictim->GetGUIDLow(), TYPEID_PLAYER);
            // Send PvP credit
            SendPvPCredit(pVictim->GetObjectGuid(), uint32(pVictim->GetHonorRankInfo().rank), uint32(hp));
            return true;
        }
    }

    return false;
}

/**
 * @brief Sends PvP credit information for an honorable kill or objective.
 *
 * @param guid The credited target GUID.
 * @param rank The displayed honor rank value.
 * @param points The credited honor points.
 */
void Player::SendPvPCredit(ObjectGuid guid, uint32 rank, uint32 points)
{
    WorldPacket data(SMSG_PVP_CREDIT, 4 + 8 + 4);
    data << points;
    data << guid;
    data << rank;
    GetSession()->SendPacket(&data);
}

/**
 * @brief Adds an honor contribution entry and refreshes honor fields.
 *
 * @param honor The honor amount to record.
 * @param type The honor contribution type.
 * @param victim The victim identifier associated with the event.
 * @param victimType The object type of the victim.
 * @return True if honor was recorded; otherwise, false.
 */
bool Player::AddHonorCP(float honor, uint8 type, uint32 victim, uint8 victimType)
{

    if (!honor)
    {
        return false;
    }

    // CharacterDatabase.PExecute("INSERT INTO `character_honor_cp` (`guid`,`victim`,`victim_type`,`honor`,`date`,`type`) VALUES (%u, %u, %u, %f, %u, %u)", (uint32)GetGUIDLow(), (uint32)uVictim->GetEntry(),uVictim->GetType() (float)honor_points, (uint32)today, (uint8)kill_type);

    HonorCP CP;
    CP.date = sWorld.GetDateToday();
    CP.honorPoints = honor;
    CP.victimID = victim;
    CP.victimType = victimType;
    CP.type = type;

    if (type == DISHONORABLE)
    {
        // DK penalties are subtracted from your RP score immediately
        // and are not included in weekly adjustment
        float RP = GetRankPoints() > CP.honorPoints ? GetRankPoints() - CP.honorPoints : 0; // remove this check to have negative ranks
        SetStoredHonor(RP);
    }

    CP.state  =  HK_NEW;
    CP.isKill =  isKill(victimType);

    m_honorCP.push_back(CP);

    UpdateHonor();
    return true;
}






/**
 * @brief Checks whether the player is eligible to interact with a capture point.
 *
 * @return True if the player can use the capture point; otherwise, false.
 */
bool Player::CanUseCapturePoint()
{
    return IsAlive() &&                       // living
        !HasStealthAura() &&                  // not stealthed
        !HasInvisibilityAura() &&             // visible
        (IsPvP() || sWorld.IsPvPRealm()) &&
        !HasMovementFlag(MOVEFLAG_FLYING) &&
        !IsTaxiFlying() &&
        !isGameMaster();
}




//---------------------------------------------------------//















/**  If in a battleground a player dies, and an enemy removes the insignia, the player's bones is lootable
 *   Called by remove insignia spell effect    */






/**
 * @brief Sends a single world state update to the client.
 *
 * @param Field The world state field identifier.
 * @param Value The new field value.
 */
void Player::SendUpdateWorldState(uint32 Field, uint32 Value)
{
    WorldPacket data(SMSG_UPDATE_WORLD_STATE, 8);
    data << Field;
    data << Value;
    GetSession()->SendPacket(&data);
}

/**
 * @brief Sends the initial world state set for the player's current zone.
 *
 * @param zoneid The zone identifier used to select world states.
 */
void Player::SendInitWorldStates(uint32 zoneid)
{
    // data depends on zoneid/mapid...
    BattleGround* bg = GetBattleGround();
    uint32 mapid = GetMapId();

    DEBUG_LOG("Sending SMSG_INIT_WORLD_STATES to Map:%u, Zone: %u", mapid, zoneid);

    uint32 count = 0;                                       // count of world states in packet

    WorldPacket data(SMSG_INIT_WORLD_STATES, (4 + 4 + 2 + 6));
    data << uint32(mapid);                                  // mapid
    data << uint32(zoneid);                                 // zone id
    size_t count_pos = data.wpos();
    data << uint16(0);                                      // count of uint64 blocks, placeholder

    switch (zoneid)
    {
        case 139:                                           // Eastern Plaguelands
        case 1377:                                          // Silithus
            if (OutdoorPvP* outdoorPvP = sOutdoorPvPMgr.GetScript(zoneid))
            {
                outdoorPvP->FillInitialWorldStates(data, count);
            }
            break;
        case 2597:                                          // AV
            if (bg && bg->GetTypeID() == BATTLEGROUND_AV)
            {
                bg->FillInitialWorldStates(data, count);
            }
            break;
        case 3277:                                          // WS
            if (bg && bg->GetTypeID() == BATTLEGROUND_WS)
            {
                bg->FillInitialWorldStates(data, count);
            }
            break;
        case 3358:                                          // AB
            if (bg && bg->GetTypeID() == BATTLEGROUND_AB)
            {
                bg->FillInitialWorldStates(data, count);
            }
            break;
    }

    data.put<uint16>(count_pos, count);                 // set actual world state amount

    GetSession()->SendPacket(&data);
}


/**
 * @brief Sends a bind point confirmation prompt to the client.
 *
 * @param guid The binder NPC GUID.
 */
void Player::SetBindPoint(ObjectGuid guid)
{
    WorldPacket data(SMSG_BINDER_CONFIRM, 8);
    data << ObjectGuid(guid);
    GetSession()->SendPacket(&data);
}

/**
 * @brief Sends a talent reset confirmation prompt to the client.
 *
 * @param guid The trainer or source GUID for the confirmation.
 */
void Player::SendTalentWipeConfirm(ObjectGuid guid)
{
    WorldPacket data(MSG_TALENT_WIPE_CONFIRM, (8 + 4));
    data << ObjectGuid(guid);
    data << uint32(resetTalentsCost());
    GetSession()->SendPacket(&data);
}

/**
 * @brief Sends a pet talent reset confirmation prompt to the client.
 */
void Player::SendPetSkillWipeConfirm()
{
    Pet* pet = GetPet();
    if (!pet)
    {
        return;
    }
    WorldPacket data(SMSG_PET_UNLEARN_CONFIRM, (8 + 4));
    data << ObjectGuid(pet->GetObjectGuid());
    data << uint32(pet->resetTalentsCost());
    GetSession()->SendPacket(&data);
}

/*********************************************************/
/***                    STORAGE SYSTEM                 ***/
/*********************************************************/




/**
 * @brief Lists all viable equipment slots for an item prototype.
 *
 * @param proto The item prototype to evaluate.
 * @param viable_slots Output array receiving candidate slots.
 * @return True if at least one viable slot exists; otherwise, false.
 */
bool Player::ViableEquipSlots(ItemPrototype const* proto, uint8 *viable_slots) const
{
    uint8 pClass;

    //DEBUG_LOG("**** [Player::ViableEquipSlots] Start ****");

    if (!viable_slots)
    {
        //DEBUG_LOG("**** [Player::ViableEquipSlots] Return array is NULL ****");
        return false;
    }

    //DEBUG_LOG("**** [Player::ViableEquipSlots] Initialize return array ****");
    viable_slots[0] = NULL_SLOT;
    viable_slots[1] = NULL_SLOT;
    viable_slots[2] = NULL_SLOT;
    viable_slots[3] = NULL_SLOT;

    if (CanUseItem(proto) == EQUIP_ERR_OK)
    {
        //DEBUG_LOG("**** [Player::ViableEquipSlots] Class/Race/Faction determined viable ****");

        //DEBUG_LOG("**** [Player::ViableEquipSlots] switch (proto->InventoryType) ****");
        switch (proto->InventoryType)
        {
            case INVTYPE_HEAD:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_HEAD ****");
                viable_slots[0] = EQUIPMENT_SLOT_HEAD;
                break;
            case INVTYPE_NECK:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_NECK ****");
                viable_slots[0] = EQUIPMENT_SLOT_NECK;
                break;
            case INVTYPE_SHOULDERS:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_SHOULDERS ****");
                viable_slots[0] = EQUIPMENT_SLOT_SHOULDERS;
                break;
            case INVTYPE_BODY:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_BODY ****");
                viable_slots[0] = EQUIPMENT_SLOT_BODY;
                break;
            case INVTYPE_CHEST:
            case INVTYPE_ROBE:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == %s ****",(INVTYPE_CHEST ? "INVTYPE_CHEST" : "INVTYPE_ROBE"));
                viable_slots[0] = EQUIPMENT_SLOT_CHEST;
                break;
            case INVTYPE_WAIST:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_WAIST ****");
                viable_slots[0] = EQUIPMENT_SLOT_WAIST;
                break;
            case INVTYPE_LEGS:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_LEGS ****");
                viable_slots[0] = EQUIPMENT_SLOT_LEGS;
                break;
            case INVTYPE_FEET:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_FEET ****");
                viable_slots[0] = EQUIPMENT_SLOT_FEET;
                break;
            case INVTYPE_WRISTS:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_WRISTS ****");
                viable_slots[0] = EQUIPMENT_SLOT_WRISTS;
                break;
            case INVTYPE_HANDS:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_HANDS ****");
                viable_slots[0] = EQUIPMENT_SLOT_HANDS;
                break;
            case INVTYPE_FINGER:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_FINGER ****");
                viable_slots[0] = EQUIPMENT_SLOT_FINGER1;
                viable_slots[1] = EQUIPMENT_SLOT_FINGER2;
                break;
            case INVTYPE_TRINKET:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_TRINKET ****");
                viable_slots[0] = EQUIPMENT_SLOT_TRINKET1;
                viable_slots[1] = EQUIPMENT_SLOT_TRINKET2;
                break;
            case INVTYPE_CLOAK:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_CLOAK ****");
                viable_slots[0] = EQUIPMENT_SLOT_BACK;
                break;
            case INVTYPE_WEAPON:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_WEAPON ****");
                viable_slots[0] = EQUIPMENT_SLOT_MAINHAND;

                //DEBUG_LOG("**** [Player::ViableEquipSlots] INVTYPE_WEAPON/Determining if can dual weild ****");
                if (CanDualWield())
                {
                    //DEBUG_LOG("**** [Player::ViableEquipSlots] INVTYPE_WEAPON/CanDualWield() == TRUE  ****");
                    viable_slots[1] = EQUIPMENT_SLOT_OFFHAND;
                }
                break;
            case INVTYPE_SHIELD:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_SHIELD ****");
                viable_slots[0] = EQUIPMENT_SLOT_OFFHAND;
                break;
            case INVTYPE_RANGED:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_RANGED ****");
                viable_slots[0] = EQUIPMENT_SLOT_RANGED;
                break;
            case INVTYPE_2HWEAPON:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_2HWEAPON ****");
                viable_slots[0] = EQUIPMENT_SLOT_MAINHAND;

                //DEBUG_LOG("**** [Player::ViableEquipSlots] INVTYPE_2HWEAPON/Determining if can dual weild and Titian Grip ****");
                if (CanDualWield())
                {
                    //DEBUG_LOG("**** [Player::ViableEquipSlots] INVTYPE_2HWEAPON/CanDualWield() && CanTitanGrip() == TRUE  ****");
                    viable_slots[1] = EQUIPMENT_SLOT_OFFHAND;
                }
                break;
            case INVTYPE_TABARD:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_TABARD ****");
                viable_slots[0] = EQUIPMENT_SLOT_TABARD;
                break;
            case INVTYPE_WEAPONMAINHAND:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_WEAPONMAINHAND ****");
                viable_slots[0] = EQUIPMENT_SLOT_MAINHAND;
                break;
            case INVTYPE_WEAPONOFFHAND:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_WEAPONOFFHAND ****");
                viable_slots[0] = EQUIPMENT_SLOT_OFFHAND;
                break;
            case INVTYPE_HOLDABLE:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_HOLDABLE ****");
                viable_slots[0] = EQUIPMENT_SLOT_OFFHAND;
                break;
            case INVTYPE_THROWN:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_THROWN ****");
                viable_slots[0] = EQUIPMENT_SLOT_RANGED;
                break;
            case INVTYPE_RANGEDRIGHT:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_RANGEDRIGHT ****");
                viable_slots[0] = EQUIPMENT_SLOT_RANGED;
                break;
            case INVTYPE_BAG:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_BAG ****");
                viable_slots[0] = INVENTORY_SLOT_BAG_START + 0;
                viable_slots[1] = INVENTORY_SLOT_BAG_START + 1;
                viable_slots[2] = INVENTORY_SLOT_BAG_START + 2;
                viable_slots[3] = INVENTORY_SLOT_BAG_START + 3;
                break;
            case INVTYPE_RELIC:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_RELIC ****");

                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_RELIC - Determine Play Class ****");
                pClass = getClass();

                if (pClass)
                {
                    //DEBUG_LOG("**** [Player::ViableEquipSlots]  proto->InventoryType == INVTYPE_RELIC - Call to getClass() returned sucess ****");

                    switch (proto->SubClass)
                    {
                        case ITEM_SUBCLASS_ARMOR_LIBRAM:
                            //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_RELIC / proto->SubClass == ITEM_SUBCLASS_ARMOR_LIBRAM ****");
                            if (pClass == CLASS_PALADIN)
                            {
                                viable_slots[0] = EQUIPMENT_SLOT_RANGED;
                            }
                            break;
                        case ITEM_SUBCLASS_ARMOR_IDOL:
                            //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_RELIC / proto->SubClass == ITEM_SUBCLASS_ARMOR_IDOL ****");
                            if (pClass == CLASS_DRUID)
                            {
                                viable_slots[0] = EQUIPMENT_SLOT_RANGED;
                            }
                            break;
                        case ITEM_SUBCLASS_ARMOR_TOTEM:
                            //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_RELIC / proto->SubClass == ITEM_SUBCLASS_ARMOR_TOTEM ****");
                            if (pClass == CLASS_SHAMAN)
                            {
                                viable_slots[0] = EQUIPMENT_SLOT_RANGED;
                            }
                            break;
                        case ITEM_SUBCLASS_ARMOR_MISC:
                            //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_RELIC / proto->SubClass == ITEM_SUBCLASS_ARMOR_MISC ****");
                            if (pClass == CLASS_WARLOCK)
                            {
                                viable_slots[0] = EQUIPMENT_SLOT_RANGED;
                            }
                            break;
                        default:
                            //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_RELIC / proto->SubClass == UNKNOWN ****");
                            break;
                    }
                }
                break;
            default:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == UNKNOWN ****");
                break;
        }
    }
    return (viable_slots[0] != NULL_SLOT);
}
















/**
 * @brief Checks whether the player has a required quantity of an item equipped.
 *
 * @param item The item entry to count.
 * @param count The required equipped quantity.
 * @param except_slot An equipment slot to ignore during the check.
 * @return True if enough copies are equipped; otherwise, false.
 */
bool Player::HasItemWithIdEquipped(uint32 item, uint32 count, uint8 except_slot) const
{
    uint32 tempcount = 0;
    for (int i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
    {
        if (i == int(except_slot))
        {
            continue;
        }

        Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (pItem && pItem->GetEntry() == item)
        {
            tempcount += pItem->GetCount();
            if (tempcount >= count)
            {
                return true;
            }
        }
    }

    return false;
}

/// Runs ONLY the Eluna OnCanUseItem veto (D5). Returns EQUIP_ERR_OK when Eluna
/// is compiled out or there is no veto. Used by the deferred-Eluna browse pass,
/// which has already had every non-Eluna sub-filter enforced worker-side.
InventoryResult Player::CanUseItemEluna(uint32 itemEntry) const
{
#ifdef ENABLE_ELUNA
    if (Eluna* e = GetEluna())
    {
        return e->OnCanUseItem(this, itemEntry);
    }
#else
    (void)itemEntry;
#endif
    return EQUIP_ERR_OK;
}


























/**
 * @brief Sends the main container open packet to the client.
 */
void Player::SendOpenContainer()
{
    DEBUG_LOG("WORLD: Sent SMSG_OPEN_CONTAINER");
    WorldPacket data(SMSG_OPEN_CONTAINER, 8);   // opens the main bag in the UI
    data << GetObjectGuid();
    GetSession()->SendPacket(&data);
}















/*********************************************************/

/***                    GOSSIP SYSTEM                  ***/

/*********************************************************/







/*********************************************************/

/***                    QUEST SYSTEM                   ***/

/*********************************************************/














/**
 * @brief Retrieves a quest template by identifier.
 *
 * @param quest_id The quest identifier to look up.
 * @return The matching quest template, or null if not found.
 */
Quest const* Player::GetQuestTemplate(uint32 quest_id)
{
    return sObjectMgr.GetQuestTemplate(quest_id);
}

























/**
 * @brief Updates whether a quest reward has been claimed.
 *
 * @param quest_id The quest identifier to update.
 * @param rewarded True if the reward has been claimed; otherwise, false.
 */
void Player::SetQuestRewarded(uint32 quest_id, bool rewarded)
{
    if (sObjectMgr.GetQuestTemplate(quest_id))
    {
        QuestStatusData& q_status = mQuestStatus[quest_id];

        q_status.m_rewarded = rewarded;
    }

    UpdateForQuestWorldObjects();
}

















/// Sent when a quest is failed to be given off at questtaker. Specifically handled reasons:
/// INVALIDREASON_QUEST_FAILED_INVENTORY_FULL=4 (or 50)
/// INVALIDREASON_QUEST_FAILED_DUPLICATE_ITEM=17
void Player::SendQuestFailedAtTaker(uint32 quest_id, uint32 reason)
{
    if (quest_id)
    {
        WorldPacket data(SMSG_QUESTGIVER_QUEST_FAILED, 8);
        data << uint32(quest_id);
        data << uint32(reason);
        GetSession()->SendPacket(&data);
        DEBUG_LOG("WORLD: Sent SMSG_QUESTGIVER_QUEST_FAILED");
    }
}








/*********************************************************/
/***                   LOAD SYSTEM                     ***/
/*********************************************************/





/**
 * @brief Checks whether a creature is tapped by this player or the player's group.
 *
 * @param creature The creature to test.
 * @return True if the tap belongs to this player or group; otherwise, false.
 */
bool Player::IsTappedByMeOrMyGroup(Creature* creature)
{
    /* Nobody tapped the monster (solo kill by another NPC) */
    if (!creature->HasFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_TAPPED))
    {
        return false;
    }

    /* If there is a loot recipient, assign it to recipient */
    if (Player* recipient = creature->GetLootRecipient())
    {
        /* See if we're in a group */
        if (Group* plr_group = recipient->GetGroup())
        {
            /* Recipient is in a group... but is it ours? */
            if (Group* my_group = GetGroup())
            {
                /* Check groups are the same */
                if (plr_group != my_group)
                {
                    return false; // Cheater, deny loot
                }
            }
            else
            {
                return false; // We're not in a group, probably cheater
            }

            /* We're in the looters group, so mob is tapped by us */
            return true;
        }
        /* We're not in a group, check to make sure we're the recipient (prevent cheaters) */
        else if (recipient == this)
        {
            return true;
        }
    }
    else
    /** Don't know what happened to the recipient, probably disconnected
     * Either way, it isn't us, so mark as tapped */
    {
        return false;
    }

    return false;
}






/**
 * @brief Loads stored honor contribution points from the database.
 *
 * @param result The query result containing honor contribution rows.
 */
void Player::_LoadHonorCP(QueryResult* result)
{
    if (result)
    {
        m_honorCP.clear();

        do
        {
            Field* fields = result->Fetch();

            HonorCP CP;
            CP.victimType       = fields[0].GetUInt8();
            CP.victimID         = fields[1].GetUInt32();
            CP.honorPoints      = fields[2].GetFloat();
            CP.date             = fields[3].GetUInt32();
            CP.type             = fields[4].GetUInt8();
            CP.state            = HK_UNCHANGED;
            CP.isKill           = isKill(CP.victimType);

            m_honorCP.push_back(CP);
        }
        while (result->NextRow());

        delete result;
    }
}


















/*********************************************************/
/***                   SAVE SYSTEM                     ***/
/*********************************************************/







/**
 * @brief Saves honor contribution point records and prunes obsolete entries.
 */
void Player::_SaveHonorCP()
{
    HonorCPMap tempList;

    for (HonorCPMap::iterator itr = m_honorCP.begin(); itr != m_honorCP.end() ; ++itr)
    {
        switch (itr->state)
        {
            case HK_OLD:
                // pending kills will be stored in characters table
                itr->state = HK_DELETED;
                break;
            case HK_NEW:
                CharacterDatabase.PExecute("INSERT INTO `character_honor_cp` (`guid`,`victim_type`,`victim`,`honor`,`date`,`type`) "
                    " VALUES (%u,%u,%u,%f,%u,%u)", GetGUIDLow(), itr->victimType, itr->victimID, itr->honorPoints , itr->date, itr->type);
                itr->state = HK_UNCHANGED;
                tempList.push_back(*itr);
                break;
            case HK_UNCHANGED:
                tempList.push_back(*itr);
                break;
            default: // deleted case
                break;
        }
    }

    m_honorCP.clear();
    m_honorCP = tempList;
    tempList.clear();
}

/**
 * @brief Saves changed mail records and deletes removed mail from the database.
 */
void Player::SaveMail()
{
    static SqlStatementID updateMail ;
    static SqlStatementID deleteMailItems ;

    static SqlStatementID deleteItem ;
    static SqlStatementID deleteMail ;
    static SqlStatementID deleteItems ;

    for (PlayerMails::iterator itr = m_mail.begin(); itr != m_mail.end(); ++itr)
    {
        Mail* m = (*itr);
        if (m->state == MAIL_STATE_CHANGED)
        {
            SqlStatement stmt = CharacterDatabase.CreateStatement(updateMail, "UPDATE `mail` SET `body` = ?,`has_items` = ?, `expire_time` = ?, `deliver_time` = ?, `money` = ?, `cod` = ?, `checked` = ? WHERE `id` = ?");
            stmt.addString(m->body.c_str());
            stmt.addUInt32(m->HasItems() ? 1 : 0);
            stmt.addUInt64(uint64(m->expire_time));
            stmt.addUInt64(uint64(m->deliver_time));
            stmt.addUInt32(m->money);
            stmt.addUInt32(m->COD);
            stmt.addUInt32(m->checked);
            stmt.addUInt32(m->messageID);
            stmt.Execute();

            if (!m->removedItems.empty())
            {
                stmt = CharacterDatabase.CreateStatement(deleteMailItems, "DELETE FROM `mail_items` WHERE `item_guid` = ?");

                for (std::vector<uint32>::const_iterator itr2 = m->removedItems.begin(); itr2 != m->removedItems.end(); ++itr2)
                {
                    stmt.PExecute(*itr2);
                }

                m->removedItems.clear();
            }
            m->state = MAIL_STATE_UNCHANGED;
        }
        else if (m->state == MAIL_STATE_DELETED)
        {
            if (m->HasItems())
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(deleteItem, "DELETE FROM `item_instance` WHERE `guid` = ?");
                for (MailItemInfoVec::const_iterator itr2 = m->items.begin(); itr2 != m->items.end(); ++itr2)
                {
                    stmt.PExecute(itr2->item_guid);
                }
            }

            SqlStatement stmt = CharacterDatabase.CreateStatement(deleteMail, "DELETE FROM `mail` WHERE `id` = ?");
            stmt.PExecute(m->messageID);

            stmt = CharacterDatabase.CreateStatement(deleteItems, "DELETE FROM `mail_items` WHERE `mail_id` = ?");
            stmt.PExecute(m->messageID);
        }
    }

    // deallocate deleted mails...
    for (PlayerMails::iterator itr = m_mail.begin(); itr != m_mail.end();)
    {
        if ((*itr)->state == MAIL_STATE_DELETED)
        {
            Mail* m = *itr;
            m_mail.erase(itr);
            delete m;
            itr = m_mail.begin();
        }
        else
        {
            ++itr;
        }
    }

    m_mailsUpdated = false;
}






/*********************************************************/
/***               FLOOD FILTER SYSTEM                 ***/
/*********************************************************/



/*********************************************************/
/***              LOW LEVEL FUNCTIONS:Notifiers        ***/
/*********************************************************/




/**
 * @brief Sends the error packet for attempting to attack while not standing.
 */
void Player::SendAttackSwingNotStanding()
{
    WorldPacket data(SMSG_ATTACKSWING_NOTSTANDING, 0);
    GetSession()->SendPacket(&data);
}











/*********************************************************/
/***              Update timers                        ***/
/*********************************************************/



/**
 * @brief Enables or disables the free-for-all PvP player flag.
 *
 * @param state True to enable FFA PvP; false to clear it.
 */
void Player::SetFFAPvP(bool state)
{
    if (state)
    {
        SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_FFA_PVP);
    }
    else
    {
        RemoveFlag(PLAYER_FLAGS, PLAYER_FLAGS_FFA_PVP);
    }
}


/**
 * @brief Unsummons the player's current pet using the requested save mode.
 *
 * @param mode The persistence mode to use when removing the pet.
 */
void Player::RemovePet(PetSaveMode mode)
{
    if (Pet* pet = GetPet())
    {
        pet->Unsummon(mode, this);
    }
}

/**
 * @brief Unsummons the player's active mini-pet.
 */
void Player::RemoveMiniPet()
{
    if (Pet* pet = GetMiniPet())
    {
        pet->Unsummon(PET_SAVE_AS_DELETED);
    }
}

/**
 * @brief Retrieves the player's currently summoned mini-pet.
 *
 * @return The mini-pet instance, or NULL if none is active.
 */
Pet* Player::GetMiniPet() const
{
    if (m_miniPetGuid.IsEmpty())
    {
        return NULL;
    }

    return GetMap()->GetPet(m_miniPetGuid);
}









/**
 * @brief Clears the pet action bar on the client.
 */
void Player::RemovePetActionBar()
{
    WorldPacket data(SMSG_PET_SPELLS, 8);
    data << ObjectGuid();
    SendDirectMessage(&data);
}

/**
 * @brief Checks whether a spell modifier currently applies to a spell.
 *
 * @param spellInfo The spell entry being evaluated.
 * @param mod The modifier to test.
 * @param spell The active spell instance, if any.
 * @return True if the modifier applies; otherwise, false.
 */
bool Player::IsAffectedBySpellmod(SpellEntry const* spellInfo, SpellModifier* mod, Spell const* spell)
{
    if (!mod || !spellInfo)
    {
        return false;
    }

    if (mod->charges == -1 && mod->lastAffected)            // marked as expired but locked until spell casting finish
    {
        // prevent apply to any spell except spell that trigger expire
        if (spell)
        {
            if (mod->lastAffected != spell)
            {
                return false;
            }
        }
        else if (mod->lastAffected != FindCurrentSpellBySpellId(spellInfo->Id))
        {
            return false;
        }
    }

    return mod->isAffectedOnSpell(spellInfo);
}


/**
 * @brief Finds a spell modifier by operation and owning spell.
 *
 * @param op The modifier operation bucket.
 * @param spellId The spell that created the modifier.
 * @return The matching modifier, or NULL if none exists.
 */
SpellModifier* Player::GetSpellMod(SpellModOp op, uint32 spellId) const
{
    for (SpellModList::const_iterator itr = m_spellMods[op].begin(); itr != m_spellMods[op].end(); ++itr)
    {
        if ((*itr)->spellId == spellId)
        {
            return *itr;
        }
    }

    return NULL;
}

/**
 * @brief Removes expired spell modifiers associated with a spell cast.
 *
 * @param spell The spell whose pending modifiers should be consumed.
 */
void Player::RemoveSpellMods(Spell const* spell)
{
    if (!spell || (m_SpellModRemoveCount == 0))
    {
        return;
    }

    for (int i = 0; i < MAX_SPELLMOD; ++i)
    {
        for (SpellModList::const_iterator itr = m_spellMods[i].begin(); itr != m_spellMods[i].end();)
        {
            SpellModifier* mod = *itr;
            ++itr;

            if (mod && mod->charges == -1 && (mod->lastAffected == spell || mod->lastAffected == NULL))
            {
                RemoveAurasDueToSpell(mod->spellId);
                if (m_spellMods[i].empty())
                {
                    break;
                }
                else
                {
                    itr = m_spellMods[i].begin();
                }
            }
        }
    }
}

/**
 * @brief Restores cancellable spell modifiers when a spell cast is interrupted.
 *
 * @param spell The canceled spell instance.
 */
void Player::ResetSpellModsDueToCanceledSpell(Spell const* spell)
{
    for (int i = 0; i < MAX_SPELLMOD; ++i)
    {
        for (SpellModList::const_iterator itr = m_spellMods[i].begin(); itr != m_spellMods[i].end(); ++itr)
        {
            SpellModifier *mod = *itr;

            if (mod->lastAffected != spell)
            {
                continue;
            }

            mod->lastAffected = NULL;

            if (mod->charges == -1)
            {
                mod->charges = 1;
                if (m_SpellModRemoveCount > 0)
                {
                    --m_SpellModRemoveCount;
                }
            }
            else if (mod->charges > 0)
            {
                ++mod->charges;
            }
        }
    }
}








/**
 * @brief Mounts the player and updates pet state for the mounted state.
 *
 * @param mount The mount display identifier.
 * @param spellId The mount spell identifier, or 0 for non-spell mounts.
 */
void Player::Mount(uint32 mount, uint32 spellId)
{
    if (!mount)
    {
        return;
    }

    Unit::Mount(mount, spellId);

    // Called by Taxi system / GM command
    if (!spellId)
    {
        UnsummonPetTemporaryIfAny();
    }
    // Called by mount aura
    else
    {
        // Normal case (Unsummon only permanent pet)
        if (Pet* pet = GetPet())
        {
            if (pet->IsPermanentPetFor((Player*)this) &&
                sWorld.getConfig(CONFIG_BOOL_PET_UNSUMMON_AT_MOUNT))
            {
                UnsummonPetTemporaryIfAny();
            }
            else
            {
                pet->ApplyModeFlags(PET_MODE_DISABLE_ACTIONS, true);
            }
        }
    }
}

/**
 * @brief Unmounts the player and restores any temporary pet state.
 *
 * @param from_aura True when the unmount is caused by aura removal.
 */
void Player::Unmount(bool from_aura)
{
    if (!IsMounted())
    {
        return;
    }

    Unit::Unmount(from_aura);

    // only resummon old pet if the player is already added to a map
    // this prevents adding a pet to a not created map which would otherwise cause a crash
    // (it could probably happen when logging in after a previous crash)
    if (Pet* pet = GetPet())
    {
        pet->ApplyModeFlags(PET_MODE_DISABLE_ACTIONS, false);
    }
    else
    {
        ResummonPetTemporaryUnSummonedIfAny();
    }
}

/**
 * @brief Sends the result of a mount attempt to the client.
 *
 * @param result The mount result code.
 */
void Player::SendMountResult(PlayerMountResult result)
{
    WorldPacket data(SMSG_MOUNTRESULT, 4);
    data << uint32(result);
    GetSession()->SendPacket(&data);
}

/**
 * @brief Sends the result of a dismount attempt to the client.
 *
 * @param result The dismount result code.
 */
void Player::SendDismountResult(PlayerDismountResult result)
{
    WorldPacket data(SMSG_DISMOUNTRESULT, 4);
    data << uint32(result);
    GetSession()->SendPacket(&data);
}

/**
 * @brief Applies cooldown lockouts to spells in the specified school mask.
 *
 * @param idSchoolMask The spell school mask to prohibit.
 * @param unTimeMs The prohibition duration in milliseconds.
 */
void Player::ProhibitSpellSchool(SpellSchoolMask idSchoolMask, uint32 unTimeMs)
{
    // last check 1.12
    WorldPacket data(SMSG_SPELL_COOLDOWN, 8 + m_spells.size() * 8);
    data << GetObjectGuid();
    time_t curTime = time(NULL);
    for (PlayerSpellMap::const_iterator itr = m_spells.begin(); itr != m_spells.end(); ++itr)
    {
        if (itr->second.state == PLAYERSPELL_REMOVED)
        {
            continue;
        }
        uint32 unSpellId = itr->first;
        SpellEntry const* spellInfo = sSpellStore.LookupEntry(unSpellId);
        MANGOS_ASSERT(spellInfo);

        // Not send cooldown for this spells
        if (spellInfo->HasAttribute(SPELL_ATTR_DISABLED_WHILE_ACTIVE))
        {
            continue;
        }

        if ((idSchoolMask & GetSpellSchoolMask(spellInfo)) && GetSpellCooldownDelay(unSpellId) < unTimeMs)
        {
            data << uint32(unSpellId);
            data << uint32(unTimeMs);                       // in m.secs
            AddSpellCooldown(unSpellId, 0, curTime + unTimeMs / IN_MILLISECONDS);
        }
    }
    GetSession()->SendPacket(&data);
}

/**
 * @brief Reinitializes player combat data for the current shapeshift form.
 *
 * @param reapplyMods True when reapplying modifiers without a real form change.
 */
void Player::InitDataForForm(bool reapplyMods)
{
    ShapeshiftForm form = GetShapeshiftForm();

    switch (form)
    {
        case FORM_CAT:
        {
            SetAttackTime(BASE_ATTACK, 1000);               // Speed 1
            SetAttackTime(OFF_ATTACK, 1000);                // Speed 1

            if (GetPowerType() != POWER_ENERGY)
            {
                SetPowerType(POWER_ENERGY);
            }
            break;
        }
        case FORM_BEAR:
        case FORM_DIREBEAR:
        {
            SetAttackTime(BASE_ATTACK, 2500);               // Speed 2.5
            SetAttackTime(OFF_ATTACK, 2500);                // Speed 2.5

            if (GetPowerType() != POWER_RAGE)
            {
                SetPowerType(POWER_RAGE);
            }
            break;
        }
        default:                                            // 0, for example
        {
            SetRegularAttackTime();

            ChrClassesEntry const* cEntry = sChrClassesStore.LookupEntry(getClass());
            if (cEntry && cEntry->powerType < MAX_POWERS && uint32(GetPowerType()) != cEntry->powerType)
            {
                SetPowerType(Powers(cEntry->powerType));
            }

            break;
        }
    }

    // update auras at form change, ignore this at mods reapply (.reset stats/etc) when form not change.
    if (!reapplyMods)
    {
        UpdateEquipSpellsAtFormChange();
    }

    UpdateAttackPowerAndDamage();
    UpdateAttackPowerAndDamage(true);
}

/**
 * @brief Initializes the player's native and current display identifiers.
 */
void Player::InitDisplayIds()
{
    PlayerInfo const* info = sObjectMgr.GetPlayerInfo(getRace(), getClass());
    if (!info)
    {
        sLog.outError("Player %u has incorrect race/class pair. Can't init display ids.", GetGUIDLow());
        return;
    }

    uint8 gender = getGender();
    switch (gender)
    {
        case GENDER_FEMALE:
            // workaround for tauren scale
            if (getRace() == RACE_TAUREN)
            {
                SetObjectScale(DEFAULT_TAUREN_FEMALE_SCALE);
            }
            else
            {
                SetObjectScale(DEFAULT_OBJECT_SCALE);
            }

            SetDisplayId(info->displayId_f);
            SetNativeDisplayId(info->displayId_f);
            break;
        case GENDER_MALE:
            // workaround for tauren scale
            if (getRace() == RACE_TAUREN)
            {
                SetObjectScale(DEFAULT_TAUREN_MALE_SCALE);
            }
            else
            {
                SetObjectScale(DEFAULT_OBJECT_SCALE);
            }

            SetDisplayId(info->displayId_m);
            SetNativeDisplayId(info->displayId_m);
            break;
        default:
            sLog.outError("Invalid gender %u for player", gender);
            return;
    }
}

// Return true is the bought item has a max count to force refresh of window by caller
bool Player::BuyItemFromVendor(ObjectGuid vendorGuid, uint32 item, uint8 count, uint8 bag, uint8 slot)
{
    // cheating attempt
    if (count < 1)
    {
        count = 1;
    }

    // cheating attempt
    if (bag != NULL_BAG && bag != INVENTORY_SLOT_BAG_0 && slot > MAX_BAG_SIZE && slot != NULL_SLOT)
    {
        return false;
    }

    if (!IsAlive())
    {
        return false;
    }

    ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(item);
    if (!pProto)
    {
        SendBuyError(BUY_ERR_CANT_FIND_ITEM, NULL, item, 0);
        return false;
    }

    Creature* pCreature = GetNPCIfCanInteractWith(vendorGuid, UNIT_NPC_FLAG_VENDOR);
    if (!pCreature)
    {
        DEBUG_LOG("WORLD: BuyItemFromVendor - %s not found or you can't interact with him.", vendorGuid.GetString().c_str());
        SendBuyError(BUY_ERR_DISTANCE_TOO_FAR, NULL, item, 0);
        return false;
    }

    VendorItemData const* vItems = pCreature->GetVendorItems();
    VendorItemData const* tItems = pCreature->GetVendorTemplateItems();
    if ((!vItems || vItems->Empty()) && (!tItems || tItems->Empty()))
    {
        SendBuyError(BUY_ERR_CANT_FIND_ITEM, pCreature, item, 0);
        return false;
    }

    uint32 vCount = vItems ? vItems->GetItemCount() : 0;
    uint32 tCount = tItems ? tItems->GetItemCount() : 0;

    size_t vendorslot = vItems ? vItems->FindItemSlot(item) : vCount;
    if (vendorslot >= vCount)
    {
        vendorslot = vCount + (tItems ? tItems->FindItemSlot(item) : tCount);
    }

    if (vendorslot >= vCount + tCount)
    {
        SendBuyError(BUY_ERR_CANT_FIND_ITEM, pCreature, item, 0);
        return false;
    }

    VendorItem const* crItem = vendorslot < vCount ? vItems->GetItem(vendorslot) : tItems->GetItem(vendorslot - vCount);
    if (!crItem || crItem->item != item)                    // store diff item (cheating)
    {
        SendBuyError(BUY_ERR_CANT_FIND_ITEM, pCreature, item, 0);
        return false;
    }

    uint32 totalCount = pProto->BuyCount * count;

    // check current item amount if it limited
    if (crItem->maxcount != 0)
    {
        if (pCreature->GetVendorItemCurrentCount(crItem) < totalCount)
        {
            SendBuyError(BUY_ERR_ITEM_ALREADY_SOLD, pCreature, item, 0);
            return false;
        }
    }

    uint32 reqFaction = pProto->RequiredReputationFaction;
    if (!reqFaction && pProto->RequiredReputationRank > 0)
    {
        reqFaction = pCreature->getFactionTemplateEntry()->faction;
    }

    if (uint32(GetReputationRank(reqFaction)) < pProto->RequiredReputationRank)
    {
        SendBuyError(BUY_ERR_REPUTATION_REQUIRE, pCreature, item, 0);
        return false;
    }

    // not check level requiremnt for normal items (PvP related bonus items is another case)
    if (pProto->RequiredHonorRank && (GetHonorHighestRankInfo().rank < (uint8)pProto->RequiredHonorRank || getLevel() < pProto->RequiredLevel))
    {
        SendBuyError(BUY_ERR_RANK_REQUIRE, pCreature, item, 0);
        return false;
    }

    if (crItem->conditionId && !isGameMaster() && !sObjectMgr.IsPlayerMeetToCondition(crItem->conditionId, this, pCreature->GetMap(), pCreature, CONDITION_FROM_VENDOR))
    {
        SendBuyError(BUY_ERR_CANT_FIND_ITEM, pCreature, item, 0);
        return false;
    }

    uint32 price = pProto->BuyPrice * count;

    // reputation discount
    price = uint32(floor(price * GetReputationPriceDiscount(pCreature)));

    if (GetMoney() < price)
    {
        SendBuyError(BUY_ERR_NOT_ENOUGHT_MONEY, pCreature, item, 0);
        return false;
    }

    Item* pItem = NULL;

    if ((bag == NULL_BAG && slot == NULL_SLOT) || IsInventoryPos(bag, slot))
    {
        ItemPosCountVec dest;
        InventoryResult msg = CanStoreNewItem(bag, slot, dest, item, totalCount);
        if (msg != EQUIP_ERR_OK)
        {
            SendEquipError(msg, NULL, NULL, item);
            return false;
        }

        ModifyMoney(-int32(price));

        pItem = StoreNewItem(dest, item, true);
    }
    else if (IsEquipmentPos(bag, slot))
    {
        if (totalCount != 1)
        {
            SendEquipError(EQUIP_ERR_ITEM_CANT_BE_EQUIPPED, NULL, NULL);
            return false;
        }

        uint16 dest;
        InventoryResult msg = CanEquipNewItem(slot, dest, item, false);
        if (msg != EQUIP_ERR_OK)
        {
            SendEquipError(msg, NULL, NULL, item);
            return false;
        }

        ModifyMoney(-int32(price));

        pItem = EquipNewItem(dest, item, true);

        if (pItem)
        {
            AutoUnequipOffhandIfNeed();
        }
    }
    else
    {
        SendEquipError(EQUIP_ERR_ITEM_DOESNT_GO_TO_SLOT, NULL, NULL);
        return false;
    }

    if (!pItem)
    {
        return false;
    }

    uint32 new_count = pCreature->UpdateVendorItemCurrentCount(crItem, totalCount);

    WorldPacket data(SMSG_BUY_ITEM, 8 + 4 + 4 + 4);
    data << pCreature->GetObjectGuid();
    data << uint32(vendorslot + 1);                 // numbered from 1 at client
    data << uint32(crItem->maxcount > 0 ? new_count : 0xFFFFFFFF);
    data << uint32(count);
    GetSession()->SendPacket(&data);

    SendNewItem(pItem, totalCount, true, false, false);

    return crItem->maxcount != 0;
}



/**
 * @brief Applies personal and category cooldowns for a spell cast.
 *
 * @param spellInfo The spell entry that triggered the cooldown.
 * @param itemId The casting item entry, if any.
 * @param spell The active spell instance.
 * @param infinityCooldown True to apply a long-lived cooldown marker.
 */
void Player::AddSpellAndCategoryCooldowns(SpellEntry const* spellInfo, uint32 itemId, Spell* spell, bool infinityCooldown)
{
    // init cooldown values
    uint32 cat   = 0;
    int32 rec    = -1;
    int32 catrec = -1;

    // some special item spells without correct cooldown in SpellInfo
    // cooldown information stored in item prototype
    // This used in same way in WorldSession::HandleItemQuerySingleOpcode data sending to client.

    if (itemId)
    {
        if (ItemPrototype const* proto = ObjectMgr::GetItemPrototype(itemId))
        {
            for (int idx = 0; idx < MAX_ITEM_PROTO_SPELLS; ++idx)
            {
                if (proto->Spells[idx].SpellId == spellInfo->Id)
                {
                    cat    = proto->Spells[idx].SpellCategory;
                    rec    = proto->Spells[idx].SpellCooldown;
                    catrec = proto->Spells[idx].SpellCategoryCooldown;
                    break;
                }
            }
        }
    }

    // if no cooldown found above then base at DBC data
    if (rec < 0 && catrec < 0)
    {
        cat = spellInfo->Category;
        rec = spellInfo->RecoveryTime;
        catrec = spellInfo->CategoryRecoveryTime;
    }

    time_t curTime = time(NULL);

    time_t catrecTime;
    time_t recTime;

    // overwrite time for selected category
    if (infinityCooldown)
    {
        // use +MONTH as infinity mark for spell cooldown (will checked as MONTH/2 at save ans skipped)
        // but not allow ignore until reset or re-login
        catrecTime = catrec > 0 ? curTime + infinityCooldownDelay : 0;
        recTime    = rec    > 0 ? curTime + infinityCooldownDelay : catrecTime;
    }
    else
    {
        // shoot spells used equipped item cooldown values already assigned in GetAttackTime(RANGED_ATTACK)
        // prevent 0 cooldowns set by another way
        if (rec <= 0 && catrec <= 0 && (cat == 76 || cat == 351))
        {
            rec = GetAttackTime(RANGED_ATTACK);
        }

        // Now we have cooldown data (if found any), time to apply mods
        if (rec > 0)
        {
            ApplySpellMod(spellInfo->Id, SPELLMOD_COOLDOWN, rec, spell);
        }

        if (catrec > 0)
        {
            ApplySpellMod(spellInfo->Id, SPELLMOD_COOLDOWN, catrec, spell);
        }

        // replace negative cooldowns by 0
        if (rec < 0)
        {
            rec = 0;
        }
        if (catrec < 0)
        {
            catrec = 0;
        }

        // no cooldown after applying spell mods
        if (rec == 0 && catrec == 0)
        {
            return;
        }

        catrecTime = catrec ? curTime + catrec / IN_MILLISECONDS : 0;
        recTime    = rec ? curTime + rec / IN_MILLISECONDS : catrecTime;
    }

    // self spell cooldown
    if (recTime > 0)
    {
        AddSpellCooldown(spellInfo->Id, itemId, recTime);
    }

    // category spells
    if (cat && catrec > 0)
    {
        SpellCategoryStore::const_iterator i_scstore = sSpellCategoryStore.find(cat);
        if (i_scstore != sSpellCategoryStore.end())
        {
            for (SpellCategorySet::const_iterator i_scset = i_scstore->second.begin(); i_scset != i_scstore->second.end(); ++i_scset)
            {
                if (*i_scset == spellInfo->Id)              // skip main spell, already handled above
                {
                    continue;
                }

                AddSpellCooldown(*i_scset, itemId, catrecTime);
            }
        }
    }
}

/**
 * @brief Stores a cooldown entry for a spell.
 *
 * @param spellid The spell identifier.
 * @param itemid The associated item identifier, if any.
 * @param end_time The server time when the cooldown ends.
 */
void Player::AddSpellCooldown(uint32 spellid, uint32 itemid, time_t end_time)
{
    SpellCooldown sc;
    sc.end = end_time;
    sc.itemid = itemid;
    m_spellCooldowns[spellid] = sc;
}

/**
 * @brief Applies cooldowns and notifies the client about a spell cooldown event.
 *
 * @param spellInfo The spell entry that triggered the cooldown.
 * @param itemId The associated item entry, if any.
 * @param spell The active spell instance.
 */
void Player::SendCooldownEvent(SpellEntry const* spellInfo, uint32 itemId, Spell* spell)
{
    // start cooldowns at server side, if any
    AddSpellAndCategoryCooldowns(spellInfo, itemId, spell);

    // Send activate cooldown timer (possible 0) at client side
    WorldPacket data(SMSG_COOLDOWN_EVENT, (4 + 8));
    data << uint32(spellInfo->Id);
    data << GetObjectGuid();
    SendDirectMessage(&data);
}









/**
 * @brief Initializes the number of primary professions the player may learn.
 */
void Player::InitPrimaryProfessions()
{
    uint32 maxProfs = GetSession()->GetSecurity() < AccountTypes(sWorld.getConfig(CONFIG_UINT32_TRADE_SKILL_GMIGNORE_MAX_PRIMARY_COUNT))
        ? sWorld.getConfig(CONFIG_UINT32_MAX_PRIMARY_TRADE_SKILL) : 10;
    SetFreePrimaryProfessions(maxProfs);
}

/**
 * @brief Sends the current combo point target and value to the client fields.
 */
void Player::SetComboPoints()
{
    Unit* combotarget = sObjectAccessor.GetUnit(*this, m_comboTargetGuid);
    if (combotarget)
    {
        SetGuidValue(PLAYER_FIELD_COMBO_TARGET, combotarget->GetObjectGuid());
        SetByteValue(PLAYER_FIELD_BYTES, 1, m_comboPoints);
    }
    /*else
    {
        // can be NULL, and then points=0. Use unknown; to reset points of some sort?
        data << PackedGuid();
        data << uint8(0);
        GetSession()->SendPacket(&data);
    }*/
}




/* Called by WorldSession::HandlePlayerLogin */
void Player::SendInitialPacketsBeforeAddToMap()
{
    /** This packet seems useless...
     * TODO: Work out if we need SMSG_SET_REST_START */
    WorldPacket data(SMSG_SET_REST_START, 4);
    data << uint32(0);                                      // unknown, may be rest state time or experience
    GetSession()->SendPacket(&data);

    /* Send information about player's home binding */
    data.Initialize(SMSG_BINDPOINTUPDATE, 5 * 4);
    data << m_homebindX << m_homebindY << m_homebindZ;
    data << (uint32) m_homebindMapId;
    data << (uint32) m_homebindAreaId;
    GetSession()->SendPacket(&data);

    /* Tutorial data */
    GetSession()->SendTutorialsData();
    SendInitialSpells();
    SendInitialActionButtons();

    /* Send player reputations */
    m_reputationMgr.SendInitialReputations();

    /* Update player's honour information (does not send anything) */
    UpdateHonor();

    const float game_time = 0.01666667f; // Game speed

    data.Initialize(SMSG_LOGIN_SETTIMESPEED, 4 + 4);
    data << uint32(secsToTimeBitFields(sWorld.GetGameTime()));
    data << game_time; // Float is 4 bytes here
    GetSession()->SendPacket(&data);

    // Set fly flag if player is on a taxi to avoid falling to the ground
    if (IsTaxiFlying())
    {
        m_movementInfo.AddMovementFlag(MOVEFLAG_FLYING);
    }

    /* Finally, set the player as the active mover */
    SetMover(this);
}

/**
 * @brief Sends map-dependent initialization packets after the player is added to the world.
 */
void Player::SendInitialPacketsAfterAddToMap()
{
    /* Update players zone */
    uint32 newzone, newarea;
    GetZoneAndAreaId(newzone, newarea);
    UpdateZone(newzone, newarea);                           // This calls SendInitWorldStates

    /* Login effect spell */
    CastSpell(this, 836, true);                             // LOGINEFFECT

    /** Sets aura effects that need to be sent after the player is added to the map
     * We use SendMessageToSet so that it's sent to everyone, including the player
     * Some auras lose their state on long teleports, we should reapply them in this case also */
    static const AuraType auratypes[] =
    {
        SPELL_AURA_MOD_FEAR,     SPELL_AURA_TRANSFORM,                 SPELL_AURA_WATER_WALK,
        SPELL_AURA_FEATHER_FALL, SPELL_AURA_HOVER,                     SPELL_AURA_SAFE_FALL,
        SPELL_AURA_NONE
    };

    /* For each aura type */
    for (AuraType const* itr = &auratypes[0]; itr && itr[0] != SPELL_AURA_NONE; ++itr)
    {
        /* Populate iterator with player's auras */
        Unit::AuraList const& auraList = GetAurasByType(*itr);

        /* If the list isn't empty, re-apply the ones we found */
        if (!auraList.empty())
        {
            auraList.front()->ApplyModifier(true, true);
        }
    }

    /* If the player is marked as stunned, root them */
    if (HasAuraType(SPELL_AURA_MOD_STUN) || HasAuraType(SPELL_AURA_MOD_ROOT))
    {
        SetRoot(true);
    }

    /* Must be called after loading the map */
    SendEnchantmentDurations();
    SendItemDurations();
}




/**
 * @brief Applies the default equip cooldown for item use spells.
 *
 * @param pItem The item whose on-use spells should receive equip cooldowns.
 */
void Player::ApplyEquipCooldown(Item* pItem)
{
    if (pItem->GetProto()->Flags & ITEM_FLAG_NO_EQUIP_COOLDOWN)
    {
        return;
    }

    for (int i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
    {
        _Spell const& spellData = pItem->GetProto()->Spells[i];

        // no spell
        if (!spellData.SpellId)
        {
            continue;
        }

        // wrong triggering type (note: ITEM_SPELLTRIGGER_ON_NO_DELAY_USE not have cooldown)
        if (spellData.SpellTrigger != ITEM_SPELLTRIGGER_ON_USE)
        {
            continue;
        }

        //! Don't replace longer cooldowns by equip cooldown if we have any.
        SpellCooldowns::iterator itr = m_spellCooldowns.find(spellData.SpellId);
        if (itr != m_spellCooldowns.end() && itr->second.itemid == pItem->GetEntry() && itr->second.end > time(NULL) + 30)
        {
            break;
        }

        AddSpellCooldown(spellData.SpellId, pItem->GetEntry(), time(NULL) + 30);

        WorldPacket data(SMSG_ITEM_COOLDOWN, 12);
        data << ObjectGuid(pItem->GetObjectGuid());
        data << uint32(spellData.SpellId);
        GetSession()->SendPacket(&data);
    }
}






/**
 * @brief Sends visible aura duration updates for a target to the player.
 *
 * @param target The unit whose aura durations should be sent.
 */
void Player::SendAuraDurationsForTarget(Unit* target)
{
    SpellAuraHolderMap const& auraHolders = target->GetSpellAuraHolderMap();
    for (SpellAuraHolderMap::const_iterator itr = auraHolders.begin(); itr != auraHolders.end(); ++itr)
    {
        SpellAuraHolder* holder = itr->second;

        if (holder->GetAuraSlot() >= MAX_AURAS || holder->IsPassive() || holder->GetCasterGuid() != GetObjectGuid())
        {
            continue;
        }

        holder->SendAuraDurationForCaster(this);
    }
}



/**
 * @brief Gets the minimum level for a battleground bracket.
 *
 * @param bracket_id The battleground bracket identifier.
 * @param bgTypeId The battleground type.
 * @return The minimum level for the bracket.
 */
uint32 Player::GetMinLevelForBattleGroundBracketId(BattleGroundBracketId bracket_id, BattleGroundTypeId bgTypeId)
{
    if (bracket_id < 1)
    {
        return 0;
    }

    if (bracket_id > BG_BRACKET_ID_LAST)
    {
        bracket_id = BG_BRACKET_ID_LAST;
    }

    BattleGround* bg = sBattleGroundMgr.GetBattleGroundTemplate(bgTypeId);
    assert(bg);
    return 10 * bracket_id + bg->GetMinLevel();
}

/**
 * @brief Gets the maximum level for a battleground bracket.
 *
 * @param bracket_id The battleground bracket identifier.
 * @param bgTypeId The battleground type.
 * @return The maximum level for the bracket.
 */
uint32 Player::GetMaxLevelForBattleGroundBracketId(BattleGroundBracketId bracket_id, BattleGroundTypeId bgTypeId)
{
    if (bracket_id >= BG_BRACKET_ID_LAST)
    {
        return 255; // hardcoded max level
    }

    return GetMinLevelForBattleGroundBracketId(bracket_id, bgTypeId) + 10;
}

/**
 * @brief Determines the battleground bracket that matches the player's level.
 *
 * @param bgTypeId The battleground type.
 * @return The matching battleground bracket identifier.
 */
BattleGroundBracketId Player::GetBattleGroundBracketIdFromLevel(BattleGroundTypeId bgTypeId) const
{
    BattleGround* bg = sBattleGroundMgr.GetBattleGroundTemplate(bgTypeId);
    assert(bg);
    if (getLevel() < bg->GetMinLevel())
    {
        return BG_BRACKET_ID_FIRST;
    }

    uint32 bracket_id = (getLevel() - bg->GetMinLevel()) / 10;
    if (bracket_id > MAX_BATTLEGROUND_BRACKETS)
    {
        return BG_BRACKET_ID_LAST;
    }

    return BattleGroundBracketId(bracket_id);
}

/**
 * @brief Calculates the vendor price discount earned from reputation and rank.
 *
 * @param pCreature The vendor creature.
 * @return The price multiplier applied to vendor costs.
 */
float Player::GetReputationPriceDiscount(Creature const* pCreature) const
{
    FactionTemplateEntry const* vendor_faction = pCreature->getFactionTemplateEntry();
    if (!vendor_faction || !vendor_faction->faction)
    {
        return 1.0f;
    }

    uint32 discount = 100;
    ReputationRank rank = GetReputationRank(vendor_faction->faction);   // get repution rank for that specific vendor faction
    if (rank >= REP_HONORED)                                            // give 10% reduction if rank is at least honored
    {
        discount -= 10;
    }

    if (GetHonorRankInfo().visualRank >= 3)                             // get pvp grade
    {
        if (FactionTemplateEntry const* player_faction = getFactionTemplateEntry())
        {
            if (player_faction->IsFriendlyTo(*vendor_faction))          // check if its friendly faction (not neutral)
            {
                discount -=10; // give 10% discount if grade is at least sergent
            }
        }
    }
    return float (discount / 100.0f);
}

/**
 * Check spell availability for training base at SkillLineAbility/SkillRaceClassInfo data.
 * Checked allowed race/class and dependent from race/class allowed min level
 *
 * @param spell_id  checked spell id
 * @param pReqlevel if arg provided then function work in view mode (level check not applied but detected minlevel returned to var by arg pointer.
 *                  if arg not provided then considered train action mode and level checked
 * @return          true if spell available for show in trainer list (with skip level check) or training.
 */
bool Player::IsSpellFitByClassAndRace(uint32 spell_id, uint32* pReqlevel /*= NULL*/) const
{
    uint32 racemask  = getRaceMask();
    uint32 classmask = getClassMask();

    SkillLineAbilityMapBounds bounds = sSpellMgr.GetSkillLineAbilityMapBounds(spell_id);
    if (bounds.first == bounds.second)
    {
        return true;
    }

    for (SkillLineAbilityMap::const_iterator _spell_idx = bounds.first; _spell_idx != bounds.second; ++_spell_idx)
    {
        SkillLineAbilityEntry const* abilityEntry = _spell_idx->second;
        // skip wrong race skills
        if (abilityEntry->racemask && (abilityEntry->racemask & racemask) == 0)
        {
            continue;
        }

        // skip wrong class skills
        if (abilityEntry->classmask && (abilityEntry->classmask & classmask) == 0)
        {
            continue;
        }

        SkillRaceClassInfoMapBounds raceBounds = sSpellMgr.GetSkillRaceClassInfoMapBounds(abilityEntry->skillId);
        for (SkillRaceClassInfoMap::const_iterator itr = raceBounds.first; itr != raceBounds.second; ++itr)
        {
            SkillRaceClassInfoEntry const* skillRCEntry = itr->second;
            if ((skillRCEntry->raceMask & racemask) && (skillRCEntry->classMask & classmask))
            {
                if (skillRCEntry->flags & ABILITY_SKILL_NONTRAINABLE)
                {
                    return false;
                }

                if (pReqlevel)                              // show trainers list case
                {
                    if (skillRCEntry->reqLevel)
                    {
                        *pReqlevel = skillRCEntry->reqLevel;
                        return true;
                    }
                }
                else                                        // check availble case at train
                {
                    // for riding spells, override the required level with the level from the configuration file
                    switch (spell_id)
                    {
                        case 33388: // Riding
                        case 33389: // Apprentice Riding
                            if (getLevel() < uint32(sWorld.getConfig(CONFIG_UINT32_MIN_TRAIN_MOUNT_LEVEL)))
                            {
                                return false;
                            }
                            break;
                        case 33391: // Riding
                        case 33392: // Journeyman Riding
                            if (getLevel() < uint32(sWorld.getConfig(CONFIG_UINT32_MIN_TRAIN_EPIC_MOUNT_LEVEL)))
                            {
                                return false;
                            }
                            break;
                        default: // any other spell
                            if (skillRCEntry->reqLevel && getLevel() < skillRCEntry->reqLevel)
                            {
                                return false;
                            }
                            break;
                    }
                }
            }
        }

        return true;
    }

    return false;
}



/**
 * @brief Accepts or declines a pending summon and teleports when valid.
 *
 * @param agree True to accept the summon; false to decline it.
 */
void Player::SummonIfPossible(bool agree)
{
    if (!agree)
    {
        m_summon_expire = 0;
        return;
    }

    // expire and auto declined
    if (m_summon_expire < time(NULL))
    {
        return;
    }

    // stop taxi flight at summon
    if (IsTaxiFlying())
    {
        GetMotionMaster()->MovementExpired();
        m_taxi.ClearTaxiDestinations();
    }

    // drop flag at summon
    // this code can be reached only when GM is summoning player who carries flag, because player should be immune to summoning spells when he carries flag
    if (BattleGround* bg = GetBattleGround())
    {
        bg->EventPlayerDroppedFlag(this);
    }

    m_summon_expire = 0;

    TeleportTo(m_summon_mapid, m_summon_x, m_summon_y, m_summon_z, GetOrientation());
}

/**
 * @brief Removes an item from the list of items with active duration tracking.
 *
 * @param item The item to stop tracking.
 */
void Player::RemoveItemDurations(Item* item)
{
    for (ItemDurationList::iterator itr = m_itemDuration.begin(); itr != m_itemDuration.end(); ++itr)
    {
        if (*itr == item)
        {
            m_itemDuration.erase(itr);
            break;
        }
    }
}

/**
 * @brief Adds an item to the list of items with active duration tracking.
 *
 * @param item The item to track.
 */
void Player::AddItemDurations(Item* item)
{
    if (item->GetUInt32Value(ITEM_FIELD_DURATION))
    {
        m_itemDuration.push_back(item);
        item->SendTimeUpdate(this);
    }
}

/**
 * @brief Automatically unequips the offhand item when a two-handed weapon requires it.
 */
void Player::AutoUnequipOffhandIfNeed()
{
    Item* offItem = GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
    if (!offItem)
    {
        return;
    }

    // need unequip offhand for 2h-weapon
    if (!IsTwoHandUsed())
    {
        return;
    }

    ItemPosCountVec off_dest;
    uint8 off_msg = CanStoreItem(NULL_BAG, NULL_SLOT, off_dest, offItem, false);
    if (off_msg == EQUIP_ERR_OK)
    {
        RemoveItem(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND, true);
        StoreItem(off_dest, offItem, true);
    }
    else
    {
        MoveItemFromInventory(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND, true);
        CharacterDatabase.BeginTransaction();
        offItem->DeleteFromInventoryDB();                   // deletes item from character's inventory
        offItem->SaveToDB();                                // recursive and not have transaction guard into self, item not in inventory and can be save standalone
        CharacterDatabase.CommitTransaction();

        std::string subject = GetSession()->GetMangosString(LANG_NOT_EQUIPPED_ITEM);
        MailDraft(subject, "There's were problems with equipping this item.").AddItem(offItem).SendMailTo(this, MailSender(this, MAIL_STATIONERY_GM), MAIL_CHECK_MASK_COPIED);
    }
}

/**
 * @brief Checks whether the player has an equipped item that satisfies a spell requirement.
 *
 * @param spellInfo The spell entry defining the equipment requirement.
 * @param ignoreItem An equipped item to ignore during the search.
 * @return True if a valid item is equipped; otherwise, false.
 */
bool Player::HasItemFitToSpellReqirements(SpellEntry const* spellInfo, Item const* ignoreItem)
{
    if (spellInfo->EquippedItemClass < 0)
    {
        return true;
    }

    // scan other equipped items for same requirements (mostly 2 daggers/etc)
    // for optimize check 2 used cases only
    switch (spellInfo->EquippedItemClass)
    {
        case ITEM_CLASS_WEAPON:
        {
            for (int i = EQUIPMENT_SLOT_MAINHAND; i < EQUIPMENT_SLOT_TABARD; ++i)
            {
                if (Item* item = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
                {
                    if (item != ignoreItem && item->IsFitToSpellRequirements(spellInfo))
                    {
                        return true;
                    }
                }
            }
            break;
        }
        case ITEM_CLASS_ARMOR:
        {
            // tabard not have dependent spells
            for (int i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_MAINHAND; ++i)
            {
                if (Item* item = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
                {
                    if (item != ignoreItem && item->IsFitToSpellRequirements(spellInfo))
                    {
                        return true;
                    }
                }
            }

            // shields can be equipped to offhand slot
            if (Item* item = GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND))
            {
                if (item != ignoreItem && item->IsFitToSpellRequirements(spellInfo))
                {
                    return true;
                }
            }

            // ranged slot can have some armor subclasses
            if (Item* item = GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_RANGED))
            {
                if (item != ignoreItem && item->IsFitToSpellRequirements(spellInfo))
                {
                    return true;
                }
            }

            break;
        }
        default:
            sLog.outError("HasItemFitToSpellReqirements: Not handled spell requirement for item class %u", spellInfo->EquippedItemClass);
            break;
    }

    return false;
}

/**
 * @brief Checks whether the player may cast a spell without consuming reagents.
 *
 * @param spellInfo The spell being cast.
 * @return True if reagents can be ignored; otherwise, false.
 */
bool Player::CanNoReagentCast(SpellEntry const* /*spellInfo*/) const
{
    // don't take reagents for spells with SPELL_ATTR_EX5_NO_REAGENT_WHILE_PREP

    return false;
}

/**
 * @brief Removes auras and interrupts casts that depend on a removed item.
 *
 * @param pItem The item being removed or invalidated.
 */
void Player::RemoveItemDependentAurasAndCasts(Item* pItem)
{
    SpellAuraHolderMap& auras = GetSpellAuraHolderMap();
    for (SpellAuraHolderMap::const_iterator itr = auras.begin(); itr != auras.end();)
    {
        SpellAuraHolder* holder = itr->second;

        // skip passive (passive item dependent spells work in another way) and not self applied auras
        SpellEntry const* spellInfo = holder->GetSpellProto();
        if (holder->IsPassive() ||  holder->GetCasterGuid() != GetObjectGuid())
        {
            ++itr;
            continue;
        }

        // skip if not item dependent or have alternative item
        if (HasItemFitToSpellReqirements(spellInfo, pItem))
        {
            ++itr;
            continue;
        }

        // no alt item, remove aura, restart check
        RemoveAurasDueToSpell(holder->GetId());
        itr = auras.begin();
    }

    // currently casted spells can be dependent from item
    for (uint32 i = 0; i < CURRENT_MAX_SPELL; ++i)
    {
        if (Spell* spell = GetCurrentSpell(CurrentSpellTypes(i)))
        {
            if (spell->getState() != SPELL_STATE_DELAYED && !HasItemFitToSpellReqirements(spell->m_spellInfo, pItem))
            {
                InterruptSpell(CurrentSpellTypes(i));
            }
        }
    }
}

/**
 * @brief Chooses the resurrection spell currently available to the player.
 *
 * @return The resurrection spell identifier, or 0 if none is available.
 */
uint32 Player::GetResurrectionSpellId()
{
    // search priceless resurrection possibilities
    uint32 prio = 0;
    uint32 spell_id = 0;
    AuraList const& dummyAuras = GetAurasByType(SPELL_AURA_DUMMY);
    for (AuraList::const_iterator itr = dummyAuras.begin(); itr != dummyAuras.end(); ++itr)
    {
        // Soulstone Resurrection                           // prio: 3 (max, non death persistent)
        if (prio < 2 && (*itr)->GetSpellProto()->SpellVisual == 99 && (*itr)->GetSpellProto()->SpellIconID == 92)
        {
            switch ((*itr)->GetId())
            {
                case 20707: spell_id =  3026; break;        // rank 1
                case 20762: spell_id = 20758; break;        // rank 2
                case 20763: spell_id = 20759; break;        // rank 3
                case 20764: spell_id = 20760; break;        // rank 4
                case 20765: spell_id = 20761; break;        // rank 5
                case 27239: spell_id = 27240; break;        // rank 6
                default:
                    sLog.outError("Unhandled spell %u: S.Resurrection", (*itr)->GetId());
                    continue;
            }

            prio = 3;
        }
        // Twisting Nether                                  // prio: 2 (max)
        else if ((*itr)->GetId() == 23701 && roll_chance_i(10))
        {
            prio = 2;
            spell_id = 23700;
        }
    }

    // Reincarnation (passive spell)                        // prio: 1
    if (prio < 1 && HasSpell(20608) && !HasSpellCooldown(21169) && HasItemCount(17030, EFFECT_INDEX_1))
    {
        spell_id = 21169;
    }

    return spell_id;
}






/**
 * @brief Gets the player's base weapon skill for an attack type.
 *
 * @param attType The attack type to evaluate.
 * @return The corresponding base weapon skill value.
 */
uint32 Player::GetBaseWeaponSkillValue(WeaponAttackType attType) const
{
    Item* item = GetWeaponForAttack(attType, true, true);

    // unarmed only with base attack
    if (attType != BASE_ATTACK && !item)
    {
        return 0;
    }

    // weapon skill or (unarmed for base attack)
    uint32  skill = item ? item->GetSkill() : uint32(SKILL_UNARMED);
    return GetPureSkillValue(skill);
}

/**
 * @brief Resurrects the player using the pending resurrection request data.
 */
void Player::ResurectUsingRequestData()
{
    /// Teleport before resurrecting by player, otherwise the player might get attacked from creatures near his corpse
    if (m_resurrectGuid.IsPlayer())
    {
        TeleportTo(m_resurrectMap, m_resurrectX, m_resurrectY, m_resurrectZ, GetOrientation());
    }

    // we can not resurrect player when we triggered far teleport
    // player will be resurrected upon teleportation
    if (IsBeingTeleportedFar())
    {
        ScheduleDelayedOperation(DELAYED_RESURRECT_PLAYER);
        return;
    }

    ResurrectPlayer(0.0f, false);

    if (GetMaxHealth() > m_resurrectHealth)
    {
        SetHealth(m_resurrectHealth);
    }
    else
    {
        SetHealth(GetMaxHealth());
    }

    if (GetMaxPower(POWER_MANA) > m_resurrectMana)
    {
        SetPower(POWER_MANA, m_resurrectMana);
    }
    else
    {
        SetPower(POWER_MANA, GetMaxPower(POWER_MANA));
    }

    SetPower(POWER_RAGE, 0);

    SetPower(POWER_ENERGY, GetMaxPower(POWER_ENERGY));

    SpawnCorpseBones();
}

/**
 * @brief Checks whether the player currently has valid client control over a unit.
 *
 * @param target The controlled unit to verify.
 * @return True if the client should control the unit; otherwise, false.
 */
bool Player::IsClientControl(Unit* target) const
{
    return (target && !target->IsFleeing() && !target->IsConfused() && !target->IsTaxiFlying() &&
        (target->GetTypeId() != TYPEID_PLAYER ||
        !((Player*)target)->InBattleGround() || ((Player*)target)->GetBattleGround()->GetStatus() != STATUS_WAIT_LEAVE) &&
        target->GetCharmerOrOwnerOrOwnGuid() == GetObjectGuid());
}

/**
 * @brief Sends a client-control state update for a unit.
 *
 * @param target The unit whose control state is being updated.
 * @param allowMove Nonzero to allow movement; zero to disable it.
 */
void Player::SetClientControl(Unit* target, uint8 allowMove)
{
    WorldPacket data(SMSG_CLIENT_CONTROL_UPDATE, target->GetPackGUID().size() + 1);
    data << target->GetPackGUID();
    data << uint8(allowMove);
    GetSession()->SendPacket(&data);
}











/**
 * @brief Updates liquid auras and mirror timers based on the player's position.
 *
 * @param m The current map.
 * @param x The X coordinate.
 * @param y The Y coordinate.
 * @param z The Z coordinate.
 */
void Player::UpdateUnderwaterState(Map* m, float x, float y, float z)
{
    GridMapLiquidData liquid_status;
    GridMapLiquidStatus res = m->GetTerrain()->getLiquidStatus(x, y, z, MAP_ALL_LIQUIDS, &liquid_status);
    if (!res)
    {
        m_MirrorTimerFlags &= ~(UNDERWATER_INWATER | UNDERWATER_INLAVA | UNDERWATER_INSLIME | UNDERWATER_INDARKWATER);
        if (m_lastLiquid && m_lastLiquid->SpellId)
        {
            RemoveAurasDueToSpell(m_lastLiquid->SpellId);
        }
        m_lastLiquid = NULL;
        return;
    }

    if (uint32 liqEntry = liquid_status.entry)
    {
        LiquidTypeEntry const* liquid = sLiquidTypeStore.LookupEntry(liqEntry);
        if (m_lastLiquid && m_lastLiquid->SpellId && m_lastLiquid->Id != liqEntry)
        {
            RemoveAurasDueToSpell(m_lastLiquid->SpellId);
        }

        if (liquid && liquid->SpellId)
        {
            if (res & (LIQUID_MAP_UNDER_WATER | LIQUID_MAP_IN_WATER))
            {
                if (!HasAura(liquid->SpellId))
                {
                    CastSpell(this, liquid->SpellId, true);
                }
            }
            else
            {
                RemoveAurasDueToSpell(liquid->SpellId);
            }
        }

        m_lastLiquid = liquid;
    }
    else if (m_lastLiquid && m_lastLiquid->SpellId)
    {
        RemoveAurasDueToSpell(m_lastLiquid->SpellId);
        m_lastLiquid = NULL;
    }

    // All liquids type - check under water position
    if (liquid_status.type_flags & (MAP_LIQUID_TYPE_WATER | MAP_LIQUID_TYPE_OCEAN | MAP_LIQUID_TYPE_MAGMA | MAP_LIQUID_TYPE_SLIME))
    {
        if (res & LIQUID_MAP_UNDER_WATER)
        {
            m_MirrorTimerFlags |= UNDERWATER_INWATER;
        }
        else
        {
            m_MirrorTimerFlags &= ~UNDERWATER_INWATER;
        }
    }

    // Allow travel in dark water on taxi or transport
    if ((liquid_status.type_flags & MAP_LIQUID_TYPE_DARK_WATER) && !IsTaxiFlying() && !GetTransport())
    {
        m_MirrorTimerFlags |= UNDERWATER_INDARKWATER;
    }
    else
    {
        m_MirrorTimerFlags &= ~UNDERWATER_INDARKWATER;
    }

    // in lava check, anywhere in lava level
    if (liquid_status.type_flags & MAP_LIQUID_TYPE_MAGMA)
    {
        if (res & (LIQUID_MAP_UNDER_WATER | LIQUID_MAP_IN_WATER | LIQUID_MAP_WATER_WALK))
        {
            m_MirrorTimerFlags |= UNDERWATER_INLAVA;
        }
        else
        {
            m_MirrorTimerFlags &= ~UNDERWATER_INLAVA;
        }
    }
    // in slime check, anywhere in slime level
    if (liquid_status.type_flags & MAP_LIQUID_TYPE_SLIME)
    {
        if (res & (LIQUID_MAP_UNDER_WATER | LIQUID_MAP_IN_WATER | LIQUID_MAP_WATER_WALK))
        {
            m_MirrorTimerFlags |= UNDERWATER_INSLIME;
        }
        else
        {
            m_MirrorTimerFlags &= ~UNDERWATER_INSLIME;
        }
    }
}

/**
 * @brief Enables or disables the player's ability to parry.
 *
 * @param value True to allow parry; false to disable it.
 */
void Player::SetCanParry(bool value)
{
    if (m_canParry == value)
    {
        return;
    }

    m_canParry = value;
    UpdateParryPercentage();
}

/**
 * @brief Enables or disables the player's ability to block.
 *
 * @param value True to allow block; false to disable it.
 */
void Player::SetCanBlock(bool value)
{
    if (m_canBlock == value)
    {
        return;
    }

    m_canBlock = value;
    UpdateBlockPercentage();
}

/**
 * @brief Checks whether this item position entry exists in a vector of positions.
 *
 * @param vec The vector of item positions to search.
 * @return True if an entry with the same position exists; otherwise, false.
 */
bool ItemPosCount::isContainedIn(ItemPosCountVec const& vec) const
{
    for (ItemPosCountVec::const_iterator itr = vec.begin(); itr != vec.end(); ++itr)
    {
        if (itr->pos == pos)
        {
            return true;
        }
    }

    return false;
}


/**
 * @brief Checks whether the player is immune to all spell schools.
 *
 * @return True if total immunity is active; otherwise, false.
 */
bool Player::isTotalImmune()
{
    AuraList const& immune = GetAurasByType(SPELL_AURA_SCHOOL_IMMUNITY);

    uint32 immuneMask = 0;
    for (AuraList::const_iterator itr = immune.begin(); itr != immune.end(); ++itr)
    {
        immuneMask |= (*itr)->GetModifier()->m_miscvalue;
        if (immuneMask & SPELL_SCHOOL_MASK_ALL)             // total immunity
        {
            return true;
        }
    }
    return false;
}

/**
 * @brief Builds temporary loot data and stores all eligible loot automatically.
 *
 * @param lootTarget The object owning the loot.
 * @param loot_id The loot template identifier.
 * @param store The loot store to use.
 * @param broadcast True to broadcast item gains.
 * @param bag The preferred destination bag.
 * @param slot The preferred destination slot.
 */
void Player::AutoStoreLoot(WorldObject const* lootTarget, uint32 loot_id, LootStore const& store, bool broadcast, uint8 bag, uint8 slot)
{
    Loot loot(lootTarget);
    loot.FillLoot(loot_id, store, this, true);

    AutoStoreLoot(loot, broadcast, bag, slot);
}

/**
 * @brief Stores all eligible loot entries directly into the player's inventory.
 *
 * @param loot The loot container to process.
 * @param broadcast True to broadcast item gains.
 * @param bag The preferred destination bag.
 * @param slot The preferred destination slot.
 */
void Player::AutoStoreLoot(Loot& loot, bool broadcast, uint8 bag, uint8 slot)
{
    uint32 max_slot = loot.GetMaxSlotInLootFor(this);
    for (uint32 i = 0; i < max_slot; ++i)
    {
        LootItem* lootItem = loot.LootItemInSlot(i, this);

        ItemPosCountVec dest;
        InventoryResult msg = CanStoreNewItem(bag, slot, dest, lootItem->itemid, lootItem->count);
        if (msg != EQUIP_ERR_OK && slot != NULL_SLOT)
        {
            msg = CanStoreNewItem(bag, NULL_SLOT, dest, lootItem->itemid, lootItem->count);
        }
        if (msg != EQUIP_ERR_OK && bag != NULL_BAG)
        {
            msg = CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, lootItem->itemid, lootItem->count);
        }
        if (msg != EQUIP_ERR_OK)
        {
            SendEquipError(msg, NULL, NULL, lootItem->itemid);
            continue;
        }

        Item* pItem = StoreNewItem(dest, lootItem->itemid, true, lootItem->randomPropertyId);
        SendNewItem(pItem, lootItem->count, false, false, broadcast);
    }
}

/**
 * @brief Replaces an item with another item while preserving transferable state.
 *
 * @param item The original item.
 * @param newItemId The new item entry identifier.
 * @return The converted item, or NULL if conversion failed.
 */
Item* Player::ConvertItem(Item* item, uint32 newItemId)
{
    uint16 pos = item->GetPos();

    Item* pNewItem = Item::CreateItem(newItemId, 1, this);
    if (!pNewItem)
    {
        return NULL;
    }

    // copy enchantments
    for (uint8 j = PERM_ENCHANTMENT_SLOT; j <= TEMP_ENCHANTMENT_SLOT; ++j)
    {
        if (item->GetEnchantmentId(EnchantmentSlot(j)))
        {
            pNewItem->SetEnchantment(EnchantmentSlot(j), item->GetEnchantmentId(EnchantmentSlot(j)),
                item->GetEnchantmentDuration(EnchantmentSlot(j)), item->GetEnchantmentCharges(EnchantmentSlot(j)));
        }
    }

    // copy durability
    if (item->GetUInt32Value(ITEM_FIELD_DURABILITY) < item->GetUInt32Value(ITEM_FIELD_MAXDURABILITY))
    {
        double loosePercent = 1 - item->GetUInt32Value(ITEM_FIELD_DURABILITY) / double(item->GetUInt32Value(ITEM_FIELD_MAXDURABILITY));
        DurabilityLoss(pNewItem, loosePercent);
    }

    if (IsInventoryPos(pos))
    {
        ItemPosCountVec dest;
        InventoryResult msg = CanStoreItem(item->GetBagSlot(), item->GetSlot(), dest, pNewItem, true);
        // ignore cast/combat time restriction
        if (msg == EQUIP_ERR_OK)
        {
            DestroyItem(item->GetBagSlot(), item->GetSlot(), true);
            return StoreItem(dest, pNewItem, true);
        }
    }
    else if (IsBankPos(pos))
    {
        ItemPosCountVec dest;
        InventoryResult msg = CanBankItem(item->GetBagSlot(), item->GetSlot(), dest, pNewItem, true);
        // ignore cast/combat time restriction
        if (msg == EQUIP_ERR_OK)
        {
            DestroyItem(item->GetBagSlot(), item->GetSlot(), true);
            return BankItem(dest, pNewItem, true);
        }
    }
    else if (IsEquipmentPos(pos))
    {
        uint16 dest;
        InventoryResult msg = CanEquipItem(item->GetSlot(), dest, pNewItem, true, false);
        // ignore cast/combat time restriction
        if (msg == EQUIP_ERR_OK)
        {
            DestroyItem(item->GetBagSlot(), item->GetSlot(), true);
            pNewItem = EquipItem(dest, pNewItem, true);
            AutoUnequipOffhandIfNeed();
            return pNewItem;
        }
    }

    // fail
    delete pNewItem;
    return NULL;
}

/**
 * @brief Calculates the total talent points available for the player's level.
 *
 * @return The number of talent points granted by level and rate settings.
 */
uint32 Player::CalculateTalentsPoints() const
{
    uint32 talentPointsForLevel = getLevel() < 10 ? 0 : getLevel() - 9;
    return uint32(talentPointsForLevel * sWorld.getConfig(CONFIG_FLOAT_RATE_TALENT));
}

struct DoPlayerLearnSpell
{
    DoPlayerLearnSpell(Player& _player) : player(_player) {}
    void operator()(uint32 spell_id) { player.learnSpell(spell_id, false); }
    Player& player;
};

/**
 * @brief Learns a spell and all higher ranks linked in its rank chain.
 *
 * @param spellid The base spell identifier.
 */
void Player::learnSpellHighRank(uint32 spellid)
{
    learnSpell(spellid, false);

    DoPlayerLearnSpell worker(*this);
    sSpellMgr.doForHighRanks(spellid, worker);
}

/**
 * @brief Loads skill values from the database and initializes related rewards.
 *
 * @param result The query result containing saved skill rows.
 */
void Player::_LoadSkills(QueryResult* result)
{
    //                                                           0      1      2
    // SetPQuery(PLAYER_LOGIN_QUERY_LOADSKILLS,          "SELECT `skill`, `value`, `max` FROM `character_skills` WHERE `guid` = '%u'", GUID_LOPART(m_guid));

    uint32 count = 0;
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();

            uint16 skill    = fields[0].GetUInt16();
            uint16 value    = fields[1].GetUInt16();
            uint16 max      = fields[2].GetUInt16();

            SkillLineEntry const* pSkill = sSkillLineStore.LookupEntry(skill);
            if (!pSkill)
            {
                sLog.outError("Character %u has skill %u that does not exist.", GetGUIDLow(), skill);
                continue;
            }

            // set fixed skill ranges
            switch (GetSkillRangeType(pSkill, false))
            {
                case SKILL_RANGE_LANGUAGE:                  // 300..300
                    value = max = 300;
                    break;
                case SKILL_RANGE_MONO:                      // 1..1, grey monolite bar
                    value = max = 1;
                    break;
                case SKILL_RANGE_LEVEL:
                    max = GetMaxSkillValueForLevel();       // max value can be wrong for the actual level
                    break;
                default:
                    break;
            }

            if (value == 0)
            {
                sLog.outError("Character %u has skill %u with value 0. Will be deleted.", GetGUIDLow(), skill);
                CharacterDatabase.PExecute("DELETE FROM `character_skills` WHERE `guid` = '%u' AND `skill` = '%u' ", GetGUIDLow(), skill);
                continue;
            }

            SetUInt32Value(PLAYER_SKILL_INDEX(count), MAKE_PAIR32(skill, 0));
            SetUInt32Value(PLAYER_SKILL_VALUE_INDEX(count), MAKE_SKILL_VALUE(value, max));
            SetUInt32Value(PLAYER_SKILL_BONUS_INDEX(count), 0);

            mSkillStatus.insert(SkillStatusMap::value_type(skill, SkillStatusData(count, SKILL_UNCHANGED)));

            learnSkillRewardedSpells(skill, value);

            ++count;

            if (count >= PLAYER_MAX_SKILLS)                 // client limit
            {
                sLog.outError("Character %u has more than %u skills.", GetGUIDLow(), PLAYER_MAX_SKILLS);
                break;
            }
        }
        while (result->NextRow());
        delete result;
    }

    for (; count < PLAYER_MAX_SKILLS; ++count)
    {
        SetUInt32Value(PLAYER_SKILL_INDEX(count), 0);
        SetUInt32Value(PLAYER_SKILL_VALUE_INDEX(count), 0);
        SetUInt32Value(PLAYER_SKILL_BONUS_INDEX(count), 0);
    }
}

/**
 * @brief Checks whether a concrete item can be equipped under unique-equip rules.
 *
 * @param pItem The item to test.
 * @param eslot The equipment slot being considered.
 * @return The inventory result describing whether equipping is allowed.
 */
InventoryResult Player::CanEquipUniqueItem(Item* pItem, uint8 eslot) const
{
    ItemPrototype const* pProto = pItem->GetProto();

    // proto based limitations
    if (InventoryResult res = CanEquipUniqueItem(pProto, eslot))
    {
        return res;
    }

    return EQUIP_ERR_OK;
}

/**
 * @brief Checks whether an item prototype can be equipped under unique-equip rules.
 *
 * @param itemProto The item prototype to test.
 * @param except_slot An equipment slot to ignore during the check.
 * @return The inventory result describing whether equipping is allowed.
 */
InventoryResult Player::CanEquipUniqueItem(ItemPrototype const* itemProto, uint8 except_slot) const
{
    // check unique-equipped on item
    if (itemProto->Flags & ITEM_FLAG_UNIQUE_EQUIPPED)
    {
        // there is an equip limit on this item
        if (HasItemWithIdEquipped(itemProto->ItemId, 1, except_slot))
        {
            return EQUIP_ERR_ITEM_CANT_BE_EQUIPPED;
        }
    }

    return EQUIP_ERR_OK;
}

/**
 * @brief Calculates and applies fall damage from movement updates.
 *
 * @param movementInfo The movement packet information containing fall data.
 */
void Player::HandleFall(MovementInfo const& movementInfo)
{
    // calculate total z distance of the fall
    Position const* position = movementInfo.GetPos();
    float z_diff = m_lastFallZ - position->z;
    DEBUG_LOG("zDiff = %f", z_diff);

    // Players with low fall distance, Feather Fall or physical immunity (charges used) are ignored
    // 14.57 can be calculated by resolving damageperc formula below to 0
    if (z_diff >= 14.57f && !IsDead() && !isGameMaster() && !HasMovementFlag(MOVEFLAG_ONTRANSPORT) &&
        !HasAuraType(SPELL_AURA_HOVER) && !HasAuraType(SPELL_AURA_FEATHER_FALL) &&
        !IsImmuneToDamage(SPELL_SCHOOL_MASK_NORMAL))
    {
        // Safe fall, fall height reduction
        int32 safe_fall = GetTotalAuraModifier(SPELL_AURA_SAFE_FALL);

        float damageperc = 0.018f * (z_diff - safe_fall) - 0.2426f;

        if (damageperc > 0)
        {
            uint32 damage = (uint32)(damageperc * GetMaxHealth() * sWorld.getConfig(CONFIG_FLOAT_RATE_DAMAGE_FALL));

            float height = position->z;
            UpdateAllowedPositionZ(position->x, position->y, height);

            if (damage > 0)
            {
                // Prevent fall damage from being more than the player maximum health
                if (damage > GetMaxHealth())
                {
                    damage = GetMaxHealth();
                }

                // Gust of Wind
                if (GetDummyAura(43621))
                {
                    damage = GetMaxHealth() / 2;
                }

                EnvironmentalDamage(DAMAGE_FALL, damage);
            }

            // Z given by moveinfo, LastZ, FallTime, WaterZ, MapZ, Damage, Safefall reduction
            DEBUG_LOG("FALLDAMAGE z=%f sz=%f pZ=%f FallTime=%d mZ=%f damage=%d SF=%d" , position->z, height, GetPositionZ(), movementInfo.GetFallTime(), height, damage, safe_fall);
        }
    }
}



/**
 * @brief Temporarily unsummons the current pet when the player's state requires it.
 */
void Player::UnsummonPetTemporaryIfAny()
{
    Pet* pet = GetPet();
    if (!pet)
    {
        return;
    }

    if (!m_temporaryUnsummonedPetNumber && pet->isControlled() && !pet->isTemporarySummoned())
    {
        m_temporaryUnsummonedPetNumber = pet->GetCharmInfo()->GetPetNumber();
    }

    if (Transport* petTransport = pet->GetTransport())
    {
        petTransport->RemovePassenger(pet);
        pet->SetTransport(nullptr);
    }

    pet->Unsummon(PET_SAVE_AS_CURRENT, this);
}

/**
 * @brief Resummons a pet that was temporarily unsummoned earlier.
 */
void Player::ResummonPetTemporaryUnSummonedIfAny()
{
    if (!m_temporaryUnsummonedPetNumber)
    {
        return;
    }

    // not resummon in not appropriate state
    if (IsPetNeedBeTemporaryUnsummoned())
    {
        return;
    }

    if (GetPetGuid())
    {
        return;
    }

    Pet* NewPet = new Pet;
    if (!NewPet->LoadPetFromDB(this, 0, m_temporaryUnsummonedPetNumber, true))
    {
        delete NewPet;
    }

    m_temporaryUnsummonedPetNumber = 0;
}



/**
 * @brief Adds or removes money from the player while clamping to valid limits.
 *
 * @param d The signed money delta.
 */
void Player::ModifyMoney(int32 d)
{
    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (Eluna* e = GetEluna())
    {
        e->OnMoneyChanged(this, d);
    }
#endif /* ENABLE_ELUNA */

    if (d < 0)
    {
        SetMoney(GetMoney() > uint32(-d) ? GetMoney() + d : 0);
    }
    else
    {
        SetMoney(GetMoney() < uint32(MAX_MONEY_AMOUNT - d) ? GetMoney() + d : MAX_MONEY_AMOUNT);
    }

}

/**
 * @brief Clears an at-login flag from the player and optionally from the database.
 *
 * @param f The flag to remove.
 * @param in_db_also True to persist the removal to the database immediately.
 */
void Player::RemoveAtLoginFlag(AtLoginFlags f, bool in_db_also /*= false*/)
{
    m_atLoginFlags &= ~f;

    if (in_db_also)
    {
        CharacterDatabase.PExecute("UPDATE `characters` set `at_login` = `at_login` & ~ %u WHERE `guid` ='%u'", uint32(f), GetGUIDLow());
    }
}

/**
 * @brief Sends a packet that clears a spell cooldown on the client.
 *
 * @param spell_id The spell whose cooldown is being cleared.
 * @param target The target unit associated with the cooldown clear.
 */
void Player::SendClearCooldown(uint32 spell_id, Unit* target)
{
    WorldPacket data(SMSG_CLEAR_COOLDOWN, 4 + 8);
    data << uint32(spell_id);
    data << target->GetObjectGuid();
    SendDirectMessage(&data);
}

/**
 * @brief Builds a teleport acknowledgement packet using a destination position.
 *
 * @param data The packet to populate.
 * @param x The destination X coordinate.
 * @param y The destination Y coordinate.
 * @param z The destination Z coordinate.
 * @param ang The destination orientation.
 */
void Player::BuildTeleportAckMsg(WorldPacket& data, float x, float y, float z, float ang) const
{
    MovementInfo mi = m_movementInfo;
    mi.ChangePosition(x, y, z, ang);

    data.Initialize(MSG_MOVE_TELEPORT_ACK, 41);
    data << GetPackGUID();
    data << uint32(0);                                      // this value increments every time
    data << mi;
}

/**
 * @brief Checks whether the player's movement info currently contains a flag.
 *
 * @param f The movement flag to test.
 * @return True if the flag is present; otherwise, false.
 */
bool Player::HasMovementFlag(MovementFlags f) const
{
    return m_movementInfo.HasMovementFlag(f);
}

/**
 * @brief Sets the player's home bind location and persists it to the database.
 *
 * @param loc The new home bind world location.
 * @param area_id The associated area identifier.
 */
void Player::SetHomebindToLocation(WorldLocation const& loc, uint32 area_id)
{
    m_homebindMapId = loc.mapid;
    m_homebindAreaId = area_id;
    m_homebindX = loc.coord_x;
    m_homebindY = loc.coord_y;
    m_homebindZ = loc.coord_z;

    // update sql homebind
    CharacterDatabase.PExecute("UPDATE `character_homebind` SET `map` = '%u', `zone` = '%u', `position_x` = '%f', `position_y` = '%f', `position_z` = '%f' WHERE `guid` = '%u'",
        m_homebindMapId, m_homebindAreaId, m_homebindX, m_homebindY, m_homebindZ, GetGUIDLow());
}

/**
 * @brief Resolves an object GUID to a world object constrained by a type mask.
 *
 * @param guid The object GUID to resolve.
 * @param typemask The allowed object type mask.
 * @return The matching object, or NULL if not found or disallowed.
 */
Object* Player::GetObjectByTypeMask(ObjectGuid guid, TypeMask typemask)
{
    switch (guid.GetHigh())
    {
        case HIGHGUID_ITEM:
            if (typemask & TYPEMASK_ITEM)
            {
                return GetItemByGuid(guid);
            }
            break;
        case HIGHGUID_PLAYER:
            if (GetObjectGuid() == guid)
            {
                return this;
            }
            if ((typemask & TYPEMASK_PLAYER) && IsInWorld())
            {
                return sObjectAccessor.FindPlayer(guid);
            }
            break;
        case HIGHGUID_GAMEOBJECT:
            if ((typemask & TYPEMASK_GAMEOBJECT) && IsInWorld())
            {
                return GetMap()->GetGameObject(guid);
            }
            break;
        case HIGHGUID_UNIT:
            if ((typemask & TYPEMASK_UNIT) && IsInWorld())
            {
                return GetMap()->GetCreature(guid);
            }
            break;
        case HIGHGUID_PET:
            if ((typemask & TYPEMASK_UNIT) && IsInWorld())
            {
                return GetMap()->GetPet(guid);
            }
            break;
        case HIGHGUID_DYNAMICOBJECT:
            if ((typemask & TYPEMASK_DYNAMICOBJECT) && IsInWorld())
            {
                return GetMap()->GetDynamicObject(guid);
            }
            break;
        case HIGHGUID_TRANSPORT:
        case HIGHGUID_CORPSE:
        case HIGHGUID_MO_TRANSPORT:
            break;
    }

    return NULL;
}



/**
 * @brief Checks whether the player is immune to a specific spell effect.
 *
 * @param spellInfo The spell entry being evaluated.
 * @param index The effect index within the spell.
 * @param castOnSelf True when the spell is being cast on the player by the player.
 * @return True if the effect is immune; otherwise, false.
 */
bool Player::IsImmuneToSpellEffect(SpellEntry const* spellInfo, SpellEffectIndex index, bool castOnSelf) const
{
    switch (spellInfo->Effect[index])
    {
        case SPELL_EFFECT_ATTACK_ME:
            return true;
        default:
            break;
    }
    switch (spellInfo->EffectApplyAuraName[index])
    {
        case SPELL_AURA_MOD_TAUNT:
            return true;
        default:
            break;
    }
    return Unit::IsImmuneToSpellEffect(spellInfo, index, castOnSelf);
}

/**
 * @brief Sends a knockback effect away from a target or from the player itself.
 *
 * @param target The source unit of the knockback.
 * @param horizontalSpeed The horizontal knockback speed.
 * @param verticalSpeed The vertical knockback speed.
 */
void Player::KnockBackFrom(Unit* target, float horizontalSpeed, float verticalSpeed)
{
    float angle = this == target ? GetOrientation() + M_PI_F : target->GetAngle(this);
    GetSession()->SendKnockBack(angle, horizontalSpeed, verticalSpeed);
}


