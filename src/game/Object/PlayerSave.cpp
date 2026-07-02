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
#include "CreatureAI.h"
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
#include "SpellAuras.h"
#include "DBCStores.h"
#include "SQLStorages.h"
#include "LFGMgr.h"
#include "DisableMgr.h"
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */
#ifdef ENABLE_PLAYERBOTS
#include "playerbot.h"
#endif /* ENABLE_PLAYERBOTS */

#define PLAYER_SKILL_INDEX(x)       (PLAYER_SKILL_INFO_1_1 + ((x)*3))

#define PLAYER_SKILL_VALUE_INDEX(x) (PLAYER_SKILL_INDEX(x)+1)

#define PLAYER_SKILL_BONUS_INDEX(x) (PLAYER_SKILL_INDEX(x)+2)

#define SKILL_VALUE(x)         PAIR32_LOPART(x)

#define SKILL_MAX(x)           PAIR32_HIPART(x)

#define MAKE_SKILL_VALUE(v, m) MAKE_PAIR32(v,m)

#define SKILL_TEMP_BONUS(x)    int16(PAIR32_LOPART(x))

#define SKILL_PERM_BONUS(x)    int16(PAIR32_HIPART(x))

#define MAKE_SKILL_BONUS(t, p) MAKE_PAIR32(t,p)

void Player::SaveToDB()
{
    // we should assure this: ASSERT((m_nextSave != sWorld.getConfig(CONFIG_UINT32_INTERVAL_SAVE)));
    // delay auto save at any saves (manual, in code, or autosave)
    m_nextSave = sWorld.getConfig(CONFIG_UINT32_INTERVAL_SAVE);

    // lets allow only players in world to be saved
    if (IsBeingTeleportedFar())
    {
        ScheduleDelayedOperation(DELAYED_SAVE_PLAYER);
        return;
    }

    DEBUG_FILTER_LOG(LOG_FILTER_PLAYER_STATS, "The value of player %s at save: ", m_name.c_str());
    outDebugStatsValues();

    CharacterDatabase.BeginTransaction();

    UpdateHonor();

#ifdef ENABLE_ELUNA
    // Hack to check that this is not on create save
    if (Eluna* e = GetEluna())
    {
        if (!HasAtLoginFlag(AT_LOGIN_FIRST))
        {
            e->OnSave(this);
        }
    }
#endif /* ENABLE_ELUNA */

    static SqlStatementID delChar ;
    static SqlStatementID insChar ;

    SqlStatement stmt = CharacterDatabase.CreateStatement(delChar, "DELETE FROM `characters` WHERE `guid` = ?");
    stmt.PExecute(GetGUIDLow());

    SqlStatement uberInsert = CharacterDatabase.CreateStatement(insChar, "INSERT INTO `characters` (`guid`,`account`,`name`,`race`,`class`,`gender`, "
        "`level`,`xp`,`money`,`playerBytes`,`playerBytes2`,`playerFlags`,"
        "`map`, `position_x`, `position_y`, `position_z`, `orientation`, "
        "`taximask`, `online`, `cinematic`, "
        "`totaltime`, `leveltime`, `rest_bonus`, `logout_time`, `is_logout_resting`, `resettalents_cost`, `resettalents_time`, "
        "`trans_x`, `trans_y`, `trans_z`, `trans_o`, `transguid`, `extra_flags`, `stable_slots`, `at_login`, `zone`, "
        "`death_expire_time`, `taxi_path`, "
        "`honor_highest_rank`, `honor_standing`, `stored_honor_rating`, `stored_dishonorable_kills`, `stored_honorable_kills`, "
        "`watchedFaction`, `drunk`, `health`, `power1`, `power2`, `power3`, "
        "`power4`, `power5`, `exploredZones`, `equipmentCache`, `ammoId`, `actionBars`, `createdDate`) "
        "VALUES ( ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, "
        "?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, "
        "?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?) ");

    uberInsert.addUInt32(GetGUIDLow());
    uberInsert.addUInt32(GetSession()->GetAccountId());
    uberInsert.addString(m_name.c_str());
    uberInsert.addUInt8(getRace());
    uberInsert.addUInt8(getClass());
    uberInsert.addUInt8(getGender());
    uberInsert.addUInt32(getLevel());
    uberInsert.addUInt32(GetUInt32Value(PLAYER_XP));
    uberInsert.addUInt32(GetMoney());
    uberInsert.addUInt32(GetUInt32Value(PLAYER_BYTES));
    uberInsert.addUInt32(GetUInt32Value(PLAYER_BYTES_2));
    uberInsert.addUInt32(GetUInt32Value(PLAYER_FLAGS));

    if (!IsBeingTeleported())
    {
        uberInsert.addUInt32(GetMapId());
        uberInsert.addFloat(finiteAlways(GetPositionX()));
        uberInsert.addFloat(finiteAlways(GetPositionY()));
        uberInsert.addFloat(finiteAlways(GetPositionZ()));
        uberInsert.addFloat(finiteAlways(GetOrientation()));
    }
    else
    {
        uberInsert.addUInt32(GetTeleportDest().mapid);
        uberInsert.addFloat(finiteAlways(GetTeleportDest().coord_x));
        uberInsert.addFloat(finiteAlways(GetTeleportDest().coord_y));
        uberInsert.addFloat(finiteAlways(GetTeleportDest().coord_z));
        uberInsert.addFloat(finiteAlways(GetTeleportDest().orientation));
    }

    std::ostringstream ss;
    ss << m_taxi;                                   // string with TaxiMaskSize numbers
    uberInsert.addString(ss);

    uberInsert.addUInt32(IsInWorld() ? 1 : 0);

    uberInsert.addUInt32(m_cinematic);

    uberInsert.addUInt32(m_Played_time[PLAYED_TIME_TOTAL]);
    uberInsert.addUInt32(m_Played_time[PLAYED_TIME_LEVEL]);

    uberInsert.addFloat(finiteAlways(m_rest_bonus));
    uberInsert.addUInt64(uint64(time(NULL)));
    uberInsert.addUInt32(HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_RESTING) ? 1 : 0);
    // save, far from tavern/city
    // save, but in tavern/city
    uberInsert.addUInt32(m_resetTalentsCost);
    uberInsert.addUInt64(uint64(m_resetTalentsTime));

    Position const* transportPosition = m_movementInfo.GetTransportPos();
    uberInsert.addFloat(finiteAlways(transportPosition->x));
    uberInsert.addFloat(finiteAlways(transportPosition->y));
    uberInsert.addFloat(finiteAlways(transportPosition->z));
    uberInsert.addFloat(finiteAlways(transportPosition->o));

    if (m_transport)
    {
        uberInsert.addUInt32(m_transport->GetGUIDLow());
    }
    else
    {
        uberInsert.addUInt32(0);
    }

    uberInsert.addUInt32(m_ExtraFlags);

    uberInsert.addUInt32(uint32(GetStableSlots()));         // to prevent save uint8 as char

    uberInsert.addUInt32(uint32(m_atLoginFlags));

    uberInsert.addUInt32(IsInWorld() ? GetZoneId() : GetCachedZoneId());

    uberInsert.addUInt64(uint64(m_deathExpireTime));

    ss << m_taxi.SaveTaxiDestinationsToString();       // string
    uberInsert.addString(ss);

    uberInsert.addUInt32(uint32(m_highest_rank.rank));
    uberInsert.addInt32(m_standing_pos);
    uberInsert.addFloat(finiteAlways(m_stored_honor));
    uberInsert.addUInt32(m_stored_dishonorableKills);
    uberInsert.addUInt32(m_stored_honorableKills);

    // FIXME: at this moment send to DB as unsigned, including unit32(-1)
    uberInsert.addUInt32(GetUInt32Value(PLAYER_FIELD_WATCHED_FACTION_INDEX));

    uberInsert.addUInt16(uint16(GetUInt32Value(PLAYER_BYTES_3) & 0xFFFE));   // DrunkState

    uberInsert.addUInt32(GetHealth());

    for (uint32 i = 0; i < MAX_POWERS; ++i) // power1 to power5
    {
        uberInsert.addUInt32(GetPower(Powers(i)));
    }

    for (uint32 i = 0; i < PLAYER_EXPLORED_ZONES_SIZE; ++i) // string
    {
        ss << GetUInt32Value(PLAYER_EXPLORED_ZONES_1 + i) << " ";
    }
    uberInsert.addString(ss); // exploredZOnes

    for (uint32 i = 0; i < EQUIPMENT_SLOT_END; ++i)         // string: item id, ench (perm/temp)
    {
        ss << GetUInt32Value(PLAYER_VISIBLE_ITEM_1_0 + i * MAX_VISIBLE_ITEM_OFFSET) << " ";

        uint32 ench1 = GetUInt32Value(PLAYER_VISIBLE_ITEM_1_0 + i * MAX_VISIBLE_ITEM_OFFSET + 1 + PERM_ENCHANTMENT_SLOT);
        uint32 ench2 = GetUInt32Value(PLAYER_VISIBLE_ITEM_1_0 + i * MAX_VISIBLE_ITEM_OFFSET + 1 + TEMP_ENCHANTMENT_SLOT);
        ss << uint32(MAKE_PAIR32(ench1, ench2)) << " ";
    }
    uberInsert.addString(ss); // EquipmentCache

    uberInsert.addUInt32(GetUInt32Value(PLAYER_AMMO_ID));

    uberInsert.addUInt32(uint32(GetByteValue(PLAYER_FIELD_BYTES, 2))); // actionbars
    uberInsert.addUInt32(GetCreatedDate());

    uberInsert.Execute();

    if (m_mailsUpdated)                                     // save mails only when needed
    {
        SaveMail();
    }

    _SaveBGData();
    _SaveInventory();
    _SaveQuestStatus();
    _SaveSpells();
    _SaveSpellCooldowns();
    _SaveActions();
    _SaveAuras();
    _SaveSkills();
    m_reputationMgr.SaveToDB();
    _SaveHonorCP();
    GetSession()->SaveTutorialsData();                      // changed only while character in game

    CharacterDatabase.CommitTransaction();

    // check if stats should only be saved on logout
    // save stats can be out of transaction
    if (m_session->isLogingOut() || !sWorld.getConfig(CONFIG_BOOL_STATS_SAVE_ONLY_ON_LOGOUT))
    {
        _SaveStats();
    }

    // save pet (hunter pet level and experience and all type pets health/mana).
    if (Pet* pet = GetPet())
    {
        pet->SavePetToDB(PET_SAVE_AS_CURRENT);
    }
}

// fast save function for item/money cheating preventing - save only inventory and money state
void Player::SaveInventoryAndGoldToDB()
{
    _SaveInventory();
    SaveGoldToDB();
}

/**
 * @brief Persists the player's current money value to the database.
 */
void Player::SaveGoldToDB()
{
    static SqlStatementID updateGold ;

    SqlStatement stmt = CharacterDatabase.CreateStatement(updateGold, "UPDATE `characters` SET `money` = ? WHERE `guid` = ?");
    stmt.PExecute(GetMoney(), GetGUIDLow());
}

/**
 * @brief Saves changed action bar bindings to the database.
 */
void Player::_SaveActions()
{
    static SqlStatementID insertAction ;
    static SqlStatementID updateAction ;
    static SqlStatementID deleteAction ;

    for (ActionButtonList::iterator itr = m_actionButtons.begin(); itr != m_actionButtons.end();)
    {
        switch (itr->second.uState)
        {
            case ACTIONBUTTON_NEW:
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(insertAction, "INSERT INTO `character_action` (`guid`,`button`,`action`,`type`) VALUES (?, ?, ?, ?)");
                stmt.addUInt32(GetGUIDLow());
                stmt.addUInt32(uint32(itr->first));
                stmt.addUInt32(itr->second.GetAction());
                stmt.addUInt32(uint32(itr->second.GetType()));
                stmt.Execute();
                itr->second.uState = ACTIONBUTTON_UNCHANGED;
                ++itr;
            }
            break;
            case ACTIONBUTTON_CHANGED:
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(updateAction, "UPDATE `character_action` SET `action` = ?, `type` = ? WHERE `guid` = ? AND `button` = ?");
                stmt.addUInt32(itr->second.GetAction());
                stmt.addUInt32(uint32(itr->second.GetType()));
                stmt.addUInt32(GetGUIDLow());
                stmt.addUInt32(uint32(itr->first));
                stmt.Execute();
                itr->second.uState = ACTIONBUTTON_UNCHANGED;
                ++itr;
            }
            break;
            case ACTIONBUTTON_DELETED:
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(deleteAction, "DELETE FROM `character_action` WHERE `guid` = ? AND `button` = ?");
                stmt.addUInt32(GetGUIDLow());
                stmt.addUInt32(uint32(itr->first));
                stmt.Execute();
                m_actionButtons.erase(itr++);
            }
            break;
            default:
                ++itr;
                break;
        }
    }
}

/**
 * @brief Saves eligible active aura state to the database.
 */
void Player::_SaveAuras()
{
    static SqlStatementID deleteAuras ;
    static SqlStatementID insertAuras ;

    SqlStatement stmt = CharacterDatabase.CreateStatement(deleteAuras, "DELETE FROM `character_aura` WHERE `guid` = ?");
    stmt.PExecute(GetGUIDLow());

    SpellAuraHolderMap const& auraHolders = GetSpellAuraHolderMap();

    if (auraHolders.empty())
    {
        return;
    }

    stmt = CharacterDatabase.CreateStatement(insertAuras, "INSERT INTO `character_aura` (`guid`, `caster_guid`, `item_guid`, `spell`, `stackcount`, `remaincharges`, "
        "`basepoints0`, `basepoints1`, `basepoints2`, `periodictime0`, `periodictime1`, `periodictime2`, `maxduration`, `remaintime`, `effIndexMask`) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

    for (SpellAuraHolderMap::const_iterator itr = auraHolders.begin(); itr != auraHolders.end(); ++itr)
    {
        SpellAuraHolder* holder = itr->second;
        // skip all holders from spells that are passive or channeled
        // save singleTarget auras if self cast.
        bool selfCastHolder = holder->GetCasterGuid() == GetObjectGuid();
        TrackedAuraType trackedType = holder->GetTrackedAuraType();
        if (!holder->IsPassive() && !IsChanneledSpell(holder->GetSpellProto()) &&
            (trackedType == TRACK_AURA_TYPE_NOT_TRACKED || (trackedType == TRACK_AURA_TYPE_SINGLE_TARGET && selfCastHolder)))
        {
            int32  damage[MAX_EFFECT_INDEX];
            uint32 periodicTime[MAX_EFFECT_INDEX];
            uint32 effIndexMask = 0;

            for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                damage[i] = 0;
                periodicTime[i] = 0;

                if (Aura* aur = holder->GetAuraByEffectIndex(SpellEffectIndex(i)))
                {
                    // don't save not own area auras
                    if (aur->IsAreaAura() && holder->GetCasterGuid() != GetObjectGuid())
                    {
                        continue;
                    }

                    damage[i] = aur->GetModifier()->m_amount;
                    periodicTime[i] = aur->GetModifier()->periodictime;
                    effIndexMask |= (1 << i);
                }
            }

            if (!effIndexMask)
            {
                continue;
            }

            stmt.addUInt32(GetGUIDLow());
            stmt.addUInt64(holder->GetCasterGuid().GetRawValue());
            stmt.addUInt32(holder->GetCastItemGuid().GetCounter());
            stmt.addUInt32(holder->GetId());
            stmt.addUInt32(holder->GetStackAmount());
            stmt.addUInt8(holder->GetAuraCharges());

            for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                stmt.addInt32(damage[i]);
            }

            for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                stmt.addUInt32(periodicTime[i]);
            }

            stmt.addInt32(holder->GetAuraMaxDuration());
            stmt.addInt32(holder->GetAuraDuration());
            stmt.addUInt32(effIndexMask);
            stmt.Execute();
        }
    }
}

/**
 * @brief Saves inventory state changes and queued item records to the database.
 */
void Player::_SaveInventory()
{
    // force items in buyback slots to new state
    // and remove those that aren't already
    for (uint8 i = BUYBACK_SLOT_START; i < BUYBACK_SLOT_END; ++i)
    {
        Item* item = m_items[i];
        if (!item || item->GetState() == ITEM_NEW)
        {
            continue;
        }

        static SqlStatementID delInv ;
        static SqlStatementID delItemInst ;

        SqlStatement stmt = CharacterDatabase.CreateStatement(delInv, "DELETE FROM `character_inventory` WHERE `item` = ?");
        stmt.PExecute(item->GetGUIDLow());

        stmt = CharacterDatabase.CreateStatement(delItemInst, "DELETE FROM `item_instance` WHERE `guid` = ?");
        stmt.PExecute(item->GetGUIDLow());

        m_items[i]->FSetState(ITEM_NEW);
    }

    // update enchantment durations
    for (EnchantDurationList::const_iterator itr = m_enchantDuration.begin(); itr != m_enchantDuration.end(); ++itr)
    {
        itr->item->SetEnchantmentDuration(itr->slot, itr->leftduration);
    }

    // if no changes
    if (m_itemUpdateQueue.empty())
    {
        return;
    }

    // do not save if the update queue is corrupt
    bool error = false;
    for (size_t i = 0; i < m_itemUpdateQueue.size(); ++i)
    {
        Item* item = m_itemUpdateQueue[i];
        if (!item || item->GetState() == ITEM_REMOVED)
        {
            continue;
        }
        Item* test = GetItemByPos(item->GetBagSlot(), item->GetSlot());

        if (test == NULL)
        {
            sLog.outError("Player(GUID: %u Name: %s)::_SaveInventory - the bag(%d) and slot(%d) values for the item with guid %d are incorrect, the player doesn't have an item at that position!", GetGUIDLow(), GetName(), item->GetBagSlot(), item->GetSlot(), item->GetGUIDLow());
            error = true;
        }
        else if (test != item)
        {
            sLog.outError("Player(GUID: %u Name: %s)::_SaveInventory - the bag(%d) and slot(%d) values for the item with guid %d are incorrect, the item with guid %d is there instead!", GetGUIDLow(), GetName(), item->GetBagSlot(), item->GetSlot(), item->GetGUIDLow(), test->GetGUIDLow());
            error = true;
        }
    }

    if (error)
    {
        sLog.outError("Player::_SaveInventory - one or more errors occurred save aborted!");
        ChatHandler(this).SendSysMessage(LANG_ITEM_SAVE_FAILED);
        return;
    }

    static SqlStatementID insertInventory ;
    static SqlStatementID updateInventory ;
    static SqlStatementID deleteInventory ;

    for (size_t i = 0; i < m_itemUpdateQueue.size(); ++i)
    {
        Item* item = m_itemUpdateQueue[i];
        if (!item)
        {
            continue;
        }

        Bag* container = item->GetContainer();
        uint32 bag_guid = container ? container->GetGUIDLow() : 0;

        switch (item->GetState())
        {
            case ITEM_NEW:
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(insertInventory, "INSERT INTO `character_inventory` (`guid`,`bag`,`slot`,`item`,`item_template`) VALUES (?, ?, ?, ?, ?)");
                stmt.addUInt32(GetGUIDLow());
                stmt.addUInt32(bag_guid);
                stmt.addUInt8(item->GetSlot());
                stmt.addUInt32(item->GetGUIDLow());
                stmt.addUInt32(item->GetEntry());
                stmt.Execute();
            }
            break;
            case ITEM_CHANGED:
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(updateInventory, "UPDATE `character_inventory` SET `guid` = ?, `bag` = ?, `slot` = ?, `item_template` = ? WHERE `item` = ?");
                stmt.addUInt32(GetGUIDLow());
                stmt.addUInt32(bag_guid);
                stmt.addUInt8(item->GetSlot());
                stmt.addUInt32(item->GetEntry());
                stmt.addUInt32(item->GetGUIDLow());
                stmt.Execute();
            }
            break;
            case ITEM_REMOVED:
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(deleteInventory, "DELETE FROM `character_inventory` WHERE `item` = ?");
                stmt.PExecute(item->GetGUIDLow());
            }
            break;
            case ITEM_UNCHANGED:
                break;
        }

        item->SaveToDB();                                   // item have unchanged inventory record and can be save standalone
    }
    m_itemUpdateQueue.clear();
}

/**
 * @brief Saves tracked quest status progress to the database.
 */
void Player::_SaveQuestStatus()
{
    static SqlStatementID insertQuestStatus ;

    static SqlStatementID updateQuestStatus ;

    // we don't need transactions here.
    for (QuestStatusMap::iterator i = mQuestStatus.begin(); i != mQuestStatus.end(); ++i)
    {
        QuestStatusData &questStatus = i->second;
        switch (questStatus.uState)
        {
            case QUEST_NEW :
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(insertQuestStatus, "INSERT INTO `character_queststatus` (`guid`,`quest`,`status`,`rewarded`,`explored`,`timer`,`mobcount1`,`mobcount2`,`mobcount3`,`mobcount4`,`itemcount1`,`itemcount2`,`itemcount3`,`itemcount4`) "
                    "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

                stmt.addUInt32(GetGUIDLow());
                stmt.addUInt32(i->first);
                stmt.addUInt8(questStatus.m_status);
                stmt.addUInt8(questStatus.m_rewarded);
                stmt.addUInt8(questStatus.m_explored);
                stmt.addUInt64(uint64(questStatus.m_timer / IN_MILLISECONDS + sWorld.GetGameTime()));
                for (int k = 0; k < QUEST_OBJECTIVES_COUNT; ++k)
                {
                    stmt.addUInt32(questStatus.m_creatureOrGOcount[k]);
                }
                for (int k = 0; k < QUEST_OBJECTIVES_COUNT; ++k)
                {
                    stmt.addUInt32(questStatus.m_itemcount[k]);
                }
                stmt.Execute();
            }
            break;
            case QUEST_CHANGED :
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(updateQuestStatus, "UPDATE `character_queststatus` SET `status` = ?,`rewarded` = ?,`explored` = ?,`timer` = ?,"
                    "`mobcount1` = ?,`mobcount2` = ?,`mobcount3` = ?,`mobcount4` = ?,`itemcount1` = ?,`itemcount2` = ?,`itemcount3` = ?,`itemcount4` = ?  WHERE `guid` = ? AND `quest` = ?");

                stmt.addUInt8(questStatus.m_status);
                stmt.addUInt8(questStatus.m_rewarded);
                stmt.addUInt8(questStatus.m_explored);
                stmt.addUInt64(uint64(questStatus.m_timer / IN_MILLISECONDS + sWorld.GetGameTime()));
                for (int k = 0; k < QUEST_OBJECTIVES_COUNT; ++k)
                {
                    stmt.addUInt32(questStatus.m_creatureOrGOcount[k]);
                }
                for (int k = 0; k < QUEST_OBJECTIVES_COUNT; ++k)
                {
                    stmt.addUInt32(questStatus.m_itemcount[k]);
                }
                stmt.addUInt32(GetGUIDLow());
                stmt.addUInt32(i->first);
                stmt.Execute();
            }
            break;
            case QUEST_UNCHANGED:
                break;
        };
        questStatus.uState = QUEST_UNCHANGED;
    }
}

/**
 * @brief Saves skill value changes to the database.
 */
void Player::_SaveSkills()
{
    static SqlStatementID delSkills ;
    static SqlStatementID insSkills ;
    static SqlStatementID updSkills ;

    // we don't need transactions here.
    for (SkillStatusMap::iterator itr = mSkillStatus.begin(); itr != mSkillStatus.end();)
    {
        if (itr->second.uState == SKILL_UNCHANGED)
        {
            ++itr;
            continue;
        }

        if (itr->second.uState == SKILL_DELETED)
        {
            SqlStatement stmt = CharacterDatabase.CreateStatement(delSkills, "DELETE FROM `character_skills` WHERE `guid` = ? AND `skill` = ?");
            stmt.PExecute(GetGUIDLow(), itr->first);
            mSkillStatus.erase(itr++);
            continue;
        }

        uint32 valueData = GetUInt32Value(PLAYER_SKILL_VALUE_INDEX(itr->second.pos));
        uint16 value = SKILL_VALUE(valueData);
        uint16 max = SKILL_MAX(valueData);

        switch (itr->second.uState)
        {
            case SKILL_NEW:
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(insSkills, "INSERT INTO `character_skills` (`guid`, `skill`, `value`, `max`) VALUES (?, ?, ?, ?)");
                stmt.PExecute(GetGUIDLow(), itr->first, value, max);
            }
            break;
            case SKILL_CHANGED:
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(updSkills, "UPDATE `character_skills` SET `value` = ?, `max` = ? WHERE `guid` = ? AND `skill` = ?");
                stmt.PExecute(value, max, GetGUIDLow(), itr->first);
            }
            break;
            case SKILL_UNCHANGED:
            case SKILL_DELETED:
                MANGOS_ASSERT(false);
                break;
        };
        itr->second.uState = SKILL_UNCHANGED;

        ++itr;
    }
}

/**
 * @brief Saves learned spell state changes to the database.
 */
void Player::_SaveSpells()
{
    static SqlStatementID delSpells ;
    static SqlStatementID insSpells ;

    SqlStatement stmtDel = CharacterDatabase.CreateStatement(delSpells, "DELETE FROM `character_spell` WHERE `guid` = ? and `spell` = ?");
    SqlStatement stmtIns = CharacterDatabase.CreateStatement(insSpells, "INSERT INTO `character_spell` (`guid`,`spell`,`active`,`disabled`) VALUES (?, ?, ?, ?)");

    for (PlayerSpellMap::iterator itr = m_spells.begin(), next = m_spells.begin(); itr != m_spells.end();)
    {
        PlayerSpell& playerSpell = itr->second;

        if (playerSpell.state == PLAYERSPELL_REMOVED || playerSpell.state == PLAYERSPELL_CHANGED)
        {
            stmtDel.PExecute(GetGUIDLow(), itr->first);
        }

        // add only changed/new not dependent spells
        if (!playerSpell.dependent && (playerSpell.state == PLAYERSPELL_NEW || playerSpell.state == PLAYERSPELL_CHANGED))
        {
            stmtIns.PExecute(GetGUIDLow(), itr->first, uint8(playerSpell.active ? 1 : 0), uint8(playerSpell.disabled ? 1 : 0));
        }

        if (playerSpell.state == PLAYERSPELL_REMOVED)
        {
            m_spells.erase(itr++);
        }
        else
        {
            playerSpell.state = PLAYERSPELL_UNCHANGED;
            ++itr;
        }
    }
}

// save player stats -- only for external usage
// real stats will be recalculated on player login
void Player::_SaveStats()
{
    // check if stat saving is enabled and if char level is high enough
    if (!sWorld.getConfig(CONFIG_UINT32_MIN_LEVEL_STAT_SAVE) || getLevel() < sWorld.getConfig(CONFIG_UINT32_MIN_LEVEL_STAT_SAVE))
    {
        return;
    }

    static SqlStatementID delStats ;
    static SqlStatementID insertStats ;

    SqlStatement stmt = CharacterDatabase.CreateStatement(delStats, "DELETE FROM `character_stats` WHERE `guid` = ?");
    stmt.PExecute(GetGUIDLow());

    stmt = CharacterDatabase.CreateStatement(insertStats, "INSERT INTO `character_stats` (`guid`, `maxhealth`, `maxpower1`, `maxpower2`, `maxpower3`, `maxpower4`, `maxpower5`, "
        "`strength`, `agility`, `stamina`, `intellect`, `spirit`, `armor`, `resHoly`, `resFire`, `resNature`, `resFrost`, `resShadow`, `resArcane`, "
        "`blockPct`, `dodgePct`, `parryPct`, `critPct`, `rangedCritPct`, `attackPower`, `rangedAttackPower`) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

    stmt.addUInt32(GetGUIDLow());
    stmt.addUInt32(GetMaxHealth());
    for (int i = 0; i < MAX_POWERS; ++i)
    {
        stmt.addUInt32(GetMaxPower(Powers(i)));
    }
    for (int i = 0; i < MAX_STATS; ++i)
    {
        stmt.addFloat(GetStat(Stats(i)));
    }
    // armor + school resistances
    for (int i = 0; i < MAX_SPELL_SCHOOL; ++i)
    {
        stmt.addUInt32(GetResistance(SpellSchools(i)));
    }
    stmt.addFloat(GetFloatValue(PLAYER_BLOCK_PERCENTAGE));
    stmt.addFloat(GetFloatValue(PLAYER_DODGE_PERCENTAGE));
    stmt.addFloat(GetFloatValue(PLAYER_PARRY_PERCENTAGE));
    stmt.addFloat(GetFloatValue(PLAYER_CRIT_PERCENTAGE));
    stmt.addFloat(GetFloatValue(PLAYER_RANGED_CRIT_PERCENTAGE));
    stmt.addUInt32(GetUInt32Value(UNIT_FIELD_ATTACK_POWER));
    stmt.addUInt32(GetUInt32Value(UNIT_FIELD_RANGED_ATTACK_POWER));

    stmt.Execute();
}

/**
 * @brief Writes the player's current combat and stat values to the debug log.
 */
void Player::outDebugStatsValues() const
{
    // optimize disabled debug output
    if (!sLog.HasLogLevelOrHigher(LOG_LVL_DEBUG) || sLog.HasLogFilter(LOG_FILTER_PLAYER_STATS))
    {
        return;
    }

    sLog.outDebug("HP is: \t\t\t%u\t\tMP is: \t\t\t%u", GetMaxHealth(), GetMaxPower(POWER_MANA));
    sLog.outDebug("AGILITY is: \t\t%f\t\tSTRENGTH is: \t\t%f", GetStat(STAT_AGILITY), GetStat(STAT_STRENGTH));
    sLog.outDebug("INTELLECT is: \t\t%f\t\tSPIRIT is: \t\t%f", GetStat(STAT_INTELLECT), GetStat(STAT_SPIRIT));
    sLog.outDebug("STAMINA is: \t\t%f", GetStat(STAT_STAMINA));
    sLog.outDebug("Armor is: \t\t%u\t\tBlock is: \t\t%f", GetArmor(), GetFloatValue(PLAYER_BLOCK_PERCENTAGE));
    sLog.outDebug("HolyRes is: \t\t%u\t\tFireRes is: \t\t%u", GetResistance(SPELL_SCHOOL_HOLY), GetResistance(SPELL_SCHOOL_FIRE));
    sLog.outDebug("NatureRes is: \t\t%u\t\tFrostRes is: \t\t%u", GetResistance(SPELL_SCHOOL_NATURE), GetResistance(SPELL_SCHOOL_FROST));
    sLog.outDebug("ShadowRes is: \t\t%u\t\tArcaneRes is: \t\t%u", GetResistance(SPELL_SCHOOL_SHADOW), GetResistance(SPELL_SCHOOL_ARCANE));
    sLog.outDebug("MIN_DAMAGE is: \t\t%f\tMAX_DAMAGE is: \t\t%f", GetFloatValue(UNIT_FIELD_MINDAMAGE), GetFloatValue(UNIT_FIELD_MAXDAMAGE));
    sLog.outDebug("MIN_OFFHAND_DAMAGE is: \t%f\tMAX_OFFHAND_DAMAGE is: \t%f", GetFloatValue(UNIT_FIELD_MINOFFHANDDAMAGE), GetFloatValue(UNIT_FIELD_MAXOFFHANDDAMAGE));
    sLog.outDebug("MIN_RANGED_DAMAGE is: \t%f\tMAX_RANGED_DAMAGE is: \t%f", GetFloatValue(UNIT_FIELD_MINRANGEDDAMAGE), GetFloatValue(UNIT_FIELD_MAXRANGEDDAMAGE));
    sLog.outDebug("ATTACK_TIME is: \t%u\t\tRANGE_ATTACK_TIME is: \t%u", GetAttackTime(BASE_ATTACK), GetAttackTime(RANGED_ATTACK));
}

void Player::UpdateSpeakTime()
{
    // ignore chat spam protection for GMs in any mode
    if (GetSession()->GetSecurity() > SEC_PLAYER)
    {
        return;
    }

    time_t current = time(NULL);
    if (m_speakTime > current)
    {
        uint32 max_count = sWorld.getConfig(CONFIG_UINT32_CHATFLOOD_MESSAGE_COUNT);
        if (!max_count)
        {
            return;
        }

        ++m_speakCount;
        if (m_speakCount >= max_count)
        {
            // prevent overwrite mute time, if message send just before mutes set, for example.
            time_t new_mute = current + sWorld.getConfig(CONFIG_UINT32_CHATFLOOD_MUTE_TIME);
            if (GetSession()->m_muteTime < new_mute)
            {
                GetSession()->m_muteTime = new_mute;
            }

            m_speakCount = 0;
        }
    }
    else
    {
        m_speakCount = 0;
    }

    m_speakTime = current + sWorld.getConfig(CONFIG_UINT32_CHATFLOOD_MESSAGE_DELAY);
}

/**
 * @brief Checks whether the player is currently allowed to send chat messages.
 *
 * @return True if the player's mute timer has expired; otherwise, false.
 */
bool Player::CanSpeak() const
{
    return  GetSession()->m_muteTime <= time(NULL);
}

void Player::SendAttackSwingNotInRange()
{
    WorldPacket data(SMSG_ATTACKSWING_NOTINRANGE, 0);
    GetSession()->SendPacket(&data);
}

/**
 * @brief Writes a character position directly to the database.
 *
 * @param guid The character GUID to update.
 * @param mapid The destination map identifier.
 * @param x The X coordinate.
 * @param y The Y coordinate.
 * @param z The Z coordinate.
 * @param o The orientation.
 * @param zone The zone identifier.
 */
void Player::SavePositionInDB(ObjectGuid guid, uint32 mapid, float x, float y, float z, float o, uint32 zone)
{
    std::ostringstream ss;
    ss << "UPDATE `characters` SET `position_x`='" << x << "',`position_y`='" << y
       << "',`position_z`='" << z << "',`orientation`='" << o << "',`map`='" << mapid
       << "',`zone`='" << zone << "',`trans_x`='0',`trans_y`='0',`trans_z`='0',"
       << "`transguid`='0',`taxi_path`='' WHERE `guid`='" << guid.GetCounter() << "'";
    DEBUG_LOG("%s", ss.str().c_str());
    CharacterDatabase.Execute(ss.str().c_str());
}

/**
 * @brief Replaces a tokenized uint32 field value in a serialized array.
 *
 * @param tokens The token array to modify.
 * @param index The token index to replace.
 * @param value The new uint32 value.
 */
void Player::SetUInt32ValueInArray(Tokens& tokens, uint16 index, uint32 value)
{
    char buf[11];
    snprintf(buf, 11, "%u", value);

    if (index >= tokens.size())
    {
        return;
    }

    tokens[index] = buf;
}

/**
 * @brief Sends the error packet for attempting to attack a dead target.
 */
void Player::SendAttackSwingDeadTarget()
{
    WorldPacket data(SMSG_ATTACKSWING_DEADTARGET, 0);
    GetSession()->SendPacket(&data);
}

/**
 * @brief Sends the error packet for a general inability to attack the target.
 */
void Player::SendAttackSwingCantAttack()
{
    WorldPacket data(SMSG_ATTACKSWING_CANT_ATTACK, 0);
    GetSession()->SendPacket(&data);
}

/**
 * @brief Sends the packet that cancels the player's current attack.
 */
void Player::SendAttackSwingCancelAttack()
{
    WorldPacket data(SMSG_CANCEL_COMBAT, 0);
    GetSession()->SendPacket(&data);
}

/**
 * @brief Sends the error packet for attempting to attack while facing the wrong direction.
 */
void Player::SendAttackSwingBadFacingAttack()
{
    WorldPacket data(SMSG_ATTACKSWING_BADFACING, 0);
    GetSession()->SendPacket(&data);
}

/**
 * @brief Sends the packet that cancels auto-repeat attacks for the client.
 */
void Player::SendAutoRepeatCancel()
{
    WorldPacket data(SMSG_CANCEL_AUTO_REPEAT, 0);
    GetSession()->SendPacket(&data);
}

/**
 * @brief Sends an exploration experience reward packet to the client.
 *
 * @param Area The explored area identifier.
 * @param Experience The awarded experience amount.
 */
void Player::SendExplorationExperience(uint32 Area, uint32 Experience)
{
    WorldPacket data(SMSG_EXPLORATION_EXPERIENCE, 8);
    data << uint32(Area);
    data << uint32(Experience);
    GetSession()->SendPacket(&data);
}

/**
 * @brief Sends a reset-failed notification for an instance map.
 *
 * @param mapid The map identifier that failed to reset.
 */
void Player::SendResetFailedNotify(uint32 mapid)
{
    WorldPacket data(SMSG_RESET_FAILED_NOTIFY, 4);
    data << uint32(mapid);
    GetSession()->SendPacket(&data);
}

/// Reset all solo instances and optionally send a message on success for each
void Player::ResetInstances(InstanceResetMethod method)
{
    // method can be INSTANCE_RESET_ALL, INSTANCE_RESET_GROUP_JOIN

    for (BoundInstancesMap::iterator itr = m_boundInstances.begin(); itr != m_boundInstances.end();)
    {
        DungeonPersistentState* state = itr->second.state;
        const MapEntry* entry = sMapStore.LookupEntry(itr->first);
        if (!entry || !state->CanReset())
        {
            ++itr;
            continue;
        }

        if (method == INSTANCE_RESET_ALL)
        {
            // the "reset all instances" method can only reset normal maps
            if (entry->map_type == MAP_RAID)
            {
                ++itr;
                continue;
            }
        }

        // if the map is loaded, reset it
        if (Map* map = sMapMgr.FindMap(state->GetMapId(), state->GetInstanceId()))
        {
            if (map->IsDungeon())
            {
                ((DungeonMap*)map)->Reset(method);
            }
        }

        // since this is a solo instance there should not be any players inside
        if (method == INSTANCE_RESET_ALL)
        {
            SendResetInstanceSuccess(state->GetMapId());
        }

        state->DeleteFromDB();
        m_boundInstances.erase(itr++);

        // the following should remove the instance save from the manager and delete it as well
        state->RemovePlayer(this);
    }
}

/**
 * @brief Sends a successful instance reset notification to the client.
 *
 * @param MapId The reset map identifier.
 */
void Player::SendResetInstanceSuccess(uint32 MapId)
{
    WorldPacket data(SMSG_INSTANCE_RESET, 4);
    data << uint32(MapId);
    GetSession()->SendPacket(&data);
}

/**
 * @brief Sends an instance reset failure message to the client.
 *
 * @param reason The reset failure reason code.
 * @param MapId The map identifier that failed to reset.
 */
void Player::SendResetInstanceFailed(uint32 reason, uint32 MapId)
{
    // reason: see enum InstanceResetFailReason
    WorldPacket data(SMSG_INSTANCE_RESET_FAILED, 8);
    data << uint32(reason);
    data << uint32(MapId);
    GetSession()->SendPacket(&data);
}

void Player::UpdateContestedPvP(uint32 diff)
{
    if (!m_contestedPvPTimer || IsInCombat())
    {
        return;
    }
    if (m_contestedPvPTimer <= diff)
    {
        ResetContestedPvP();
    }
    else
    {
        m_contestedPvPTimer -= diff;
    }
}

/**
 * @brief Updates and clears the player's PvP flag when the timeout expires.
 *
 * @param currTime The current server time.
 */
void Player::UpdatePvPFlag(time_t currTime)
{
    if (!IsPvP())
    {
        return;
    }
    if (pvpInfo.endTimer == 0 || currTime < (pvpInfo.endTimer + 300))
    {
        return;
    }

    UpdatePvP(false);
}

/**
 * @brief Starts an active duel once the duel countdown completes.
 *
 * @param currTime The current server time.
 */
void Player::UpdateDuelFlag(time_t currTime)
{
    if (!duel || duel->startTimer == 0 || currTime < duel->startTimer + 3)
    {
        return;
    }

    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (Eluna* e = GetEluna())
    {
        e->OnDuelStart(this, duel->opponent);
    }
#endif /* ENABLE_ELUNA */

    SetUInt32Value(PLAYER_DUEL_TEAM, 1);
    duel->opponent->SetUInt32Value(PLAYER_DUEL_TEAM, 2);

    duel->startTimer = 0;
    duel->startTime  = currTime;
    duel->opponent->duel->startTimer = 0;
    duel->opponent->duel->startTime  = currTime;
}

/**
 * @brief Sends a say chat message from the player to nearby listeners.
 *
 * @param text The message text.
 * @param language The language used for the chat packet.
 */
void Player::Say(const std::string& text, const uint32 language)
{
    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_SAY, text.c_str(), Language(language), GetChatTag(), GetObjectGuid(), GetName());
    SendMessageToSetInRange(&data, sWorld.getConfig(CONFIG_FLOAT_LISTEN_RANGE_SAY), true);
}

/**
 * @brief Sends a yell chat message from the player to nearby listeners.
 *
 * @param text The message text.
 * @param language The language used for the chat packet.
 */
void Player::Yell(const std::string& text, const uint32 language)
{
    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_YELL, text.c_str(), Language(language), GetChatTag(), GetObjectGuid(), GetName());
    SendMessageToSetInRange(&data, sWorld.getConfig(CONFIG_FLOAT_LISTEN_RANGE_YELL), true);
}

/**
 * @brief Sends a text emote message from the player to nearby listeners.
 *
 * @param text The emote text.
 */
void Player::TextEmote(const std::string& text)
{
    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_EMOTE, text.c_str(), LANG_UNIVERSAL, GetChatTag(), GetObjectGuid(), GetName());
    SendMessageToSetInRange(&data, sWorld.getConfig(CONFIG_FLOAT_LISTEN_RANGE_TEXTEMOTE), true, !sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHAT));
}

/**
 * @brief Logs a whisper to the database when whisper logging is enabled.
 *
 * @param text The whisper text.
 * @param receiver The recipient player GUID.
 */
void Player::LogWhisper(const std::string& text, ObjectGuid receiver)
{
    WhisperLoggingLevels loggingLevel = WhisperLoggingLevels(sWorld.getConfig(CONFIG_UINT32_LOG_WHISPERS));

    if (loggingLevel == WHISPER_LOGGING_NONE)
    {
        return;
    }

    //Try to find ticket by either this player or the receiver
    GMTicket* ticket = sTicketMgr.GetGMTicket(GetObjectGuid());
    if (!ticket)
    {
        ticket = sTicketMgr.GetGMTicket(receiver);
    }

    uint32 ticketId = 0;
    if (ticket)
    {
        ticketId = ticket->GetId();
    }

    bool isSomeoneGM = false;

    //Find out if at least one of them is a GM for ticket logging
    if (GetSession()->GetSecurity() >= SEC_GAMEMASTER)
    {
        isSomeoneGM = true;
    }
    else
    {
        Player* pRecvPlayer = sObjectMgr.GetPlayer(receiver);
        if (pRecvPlayer && pRecvPlayer->GetSession()->GetSecurity() >= SEC_GAMEMASTER)
        {
            isSomeoneGM = true;
        }
    }

    if ((loggingLevel == WHISPER_LOGGING_TICKETS && ticket && isSomeoneGM) ||
        loggingLevel == WHISPER_LOGGING_EVERYTHING)
    {
        static SqlStatementID wlog;
        SqlStatement stmt = CharacterDatabase.CreateStatement(wlog, "INSERT INTO `character_whispers` (`to_guid`, `from_guid`, `message`, `regarding_ticket_id`) VALUES (?, ?, ?, ?)");
        stmt.addUInt32(receiver.GetCounter());          // to_guid
        stmt.addUInt32(GetObjectGuid().GetCounter());   // from_guid
        stmt.addString(text.c_str());                   // message
        stmt.addUInt32(ticketId);                       // regarding_ticket_id
        stmt.Execute();
    }
}

/**
 * @brief Sends a whisper to another player and handles local response messages.
 *
 * @param text The whisper text.
 * @param language The requested chat language.
 * @param receiver The recipient player GUID.
 */
void Player::Whisper(const std::string& text, uint32 language, ObjectGuid receiver)
{
    if (language != LANG_ADDON)                             // if not addon data
    {
        language = LANG_UNIVERSAL; // whispers should always be readable
    }

    Player* rPlayer = sObjectMgr.GetPlayer(receiver);

    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, text.c_str(), Language(language), GetChatTag(), GetObjectGuid(), GetName());
    rPlayer->GetSession()->SendPacket(&data);

    // not send confirmation for addon messages
    if (language != LANG_ADDON)
    {
        data.clear();
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER_INFORM, text.c_str(), Language(language), CHAT_TAG_NONE, rPlayer->GetObjectGuid());
        LogWhisper(text, receiver);
        GetSession()->SendPacket(&data);
    }

    if (!isAcceptWhispers())
    {
        SetAcceptWhispers(true);
        ChatHandler(this).SendSysMessage(LANG_COMMAND_WHISPERON);
    }

    if (rPlayer->isAFK())
    {
        /* Announce to the player that the person they're whispering to is afk */
        ChatHandler(this).PSendSysMessage(LANG_PLAYER_AFK, rPlayer->GetName(), rPlayer->autoReplyMsg.c_str());
    }
    else if (rPlayer->isDND())
    {
        /* Announce to the player that the person they're whispering to is dnd */
        ChatHandler(this).PSendSysMessage(LANG_PLAYER_DND, rPlayer->GetName(), rPlayer->autoReplyMsg.c_str());
    }
}

/**
 * @brief Sends the controlled pet's spell bar and cooldown state to the client.
 */
void Player::PetSpellInitialize()
{
    Pet* pet = GetPet();

    if (!pet)
    {
        return;
    }

    DEBUG_LOG("Pet Spells Groups");

    CharmInfo* charmInfo = pet->GetCharmInfo();

    WorldPacket data(SMSG_PET_SPELLS, 8 + 4 + 1 + 1 + 2 + 4 * MAX_UNIT_ACTION_BAR_INDEX + 1 + 1);
    data << pet->GetObjectGuid();
    data << uint32(0);
    data << uint8(charmInfo->GetReactState()) << uint8(charmInfo->GetCommandState()) << uint16(0);

    // action bar loop
    charmInfo->BuildActionBar(&data);

    size_t spellsCountPos = data.wpos();

    // spells count
    uint8 addlist = 0;
    data << uint8(addlist);                                 // placeholder

    if (pet->IsPermanentPetFor(this))
    {
        // spells loop
        for (PetSpellMap::const_iterator itr = pet->m_spells.begin(); itr != pet->m_spells.end(); ++itr)
        {
            if (itr->second.state == PETSPELL_REMOVED)
            {
                continue;
            }

            data << uint32(MAKE_UNIT_ACTION_BUTTON(itr->first, itr->second.active));
            ++addlist;
        }
    }

    data.put<uint8>(spellsCountPos, addlist);

    uint8 cooldownsCount = pet->m_CreatureSpellCooldowns.size() + pet->m_CreatureCategoryCooldowns.size();
    data << uint8(cooldownsCount);

    time_t curTime = time(NULL);

    for (CreatureSpellCooldowns::const_iterator itr = pet->m_CreatureSpellCooldowns.begin(); itr != pet->m_CreatureSpellCooldowns.end(); ++itr)
    {
        time_t cooldown = (itr->second > curTime) ? (itr->second - curTime) * IN_MILLISECONDS : 0;

        data << uint16(itr->first);                         // spellid
        data << uint16(0);                                  // spell category?
        data << uint32(cooldown);                           // cooldown
        data << uint32(0);                                  // category cooldown
    }

    for (CreatureSpellCooldowns::const_iterator itr = pet->m_CreatureCategoryCooldowns.begin(); itr != pet->m_CreatureCategoryCooldowns.end(); ++itr)
    {
        time_t cooldown = (itr->second > curTime) ? (itr->second - curTime) * IN_MILLISECONDS : 0;

        data << uint16(itr->first);                         // spellid
        data << uint16(0);                                  // spell category?
        data << uint32(0);                                  // cooldown
        data << uint32(cooldown);                           // category cooldown
    }

    GetSession()->SendPacket(&data);
}

/**
 * @brief Sends the possessed unit's action bar state to the client.
 */
void Player::PossessSpellInitialize()
{
    Unit* charm = GetCharm();

    if (!charm)
    {
        return;
    }

    CharmInfo* charmInfo = charm->GetCharmInfo();

    if (!charmInfo)
    {
        sLog.outError("Player::PossessSpellInitialize(): charm (GUID: %u TypeId: %u) has no charminfo!", charm->GetGUIDLow(), charm->GetTypeId());
        return;
    }

    WorldPacket data(SMSG_PET_SPELLS, 8 + 4 + 4 + 4 * MAX_UNIT_ACTION_BAR_INDEX + 1 + 1);
    data << charm->GetObjectGuid();
    data << uint32(0);
    data << uint32(0);

    charmInfo->BuildActionBar(&data);

    data << uint8(0);                                       // spells count
    data << uint8(0);                                       // cooldowns count

    GetSession()->SendPacket(&data);
}

/**
 * @brief Sends the charmed unit's available actions and spells to the client.
 */
void Player::CharmSpellInitialize()
{
    Unit* charm = GetCharm();

    if (!charm)
    {
        return;
    }

    CharmInfo* charmInfo = charm->GetCharmInfo();
    if (!charmInfo)
    {
        sLog.outError("Player::CharmSpellInitialize(): the player's charm (GUID: %u TypeId: %u) has no charminfo!", charm->GetGUIDLow(), charm->GetTypeId());
        return;
    }

    uint8 addlist = 0;

    if (charm->GetTypeId() != TYPEID_PLAYER)
    {
        CreatureInfo const* cinfo = ((Creature*)charm)->GetCreatureInfo();

        if (cinfo && cinfo->CreatureType == CREATURE_TYPE_DEMON && getClass() == CLASS_WARLOCK)
        {
            for (uint32 i = 0; i < CREATURE_MAX_SPELLS; ++i)
            {
                if (charmInfo->GetCharmSpell(i)->GetAction())
                {
                    ++addlist;
                }
            }
        }
    }

    WorldPacket data(SMSG_PET_SPELLS, 8 + 4 + 1 + 1 + 2 + 4 * MAX_UNIT_ACTION_BAR_INDEX + 1 + 4 * addlist + 1);
    data << charm->GetObjectGuid();
    data << uint32(0x00000000);

    if (charm->GetTypeId() != TYPEID_PLAYER)
    {
        data << uint8(charmInfo->GetReactState()) << uint8(charmInfo->GetCommandState()) << uint16(0);
    }
    else
    {
        data << uint8(0) << uint8(0) << uint16(0); // TODO it is exactly the same as uint32(PetModeFlags) from SMSG_PET_MODE
    }

    charmInfo->BuildActionBar(&data);

    data << uint8(addlist);

    if (addlist)
    {
        for (uint32 i = 0; i < CREATURE_MAX_SPELLS; ++i)
        {
            CharmSpellEntry* cspell = charmInfo->GetCharmSpell(i);
            if (cspell->GetAction())
            {
                data << uint32(cspell->packedData);
            }
        }
    }

    data << uint8(0);                                       // cooldowns count

    GetSession()->SendPacket(&data);
}

/**
 * @brief Applies or removes a spell modifier and notifies the client.
 *
 * @param mod The modifier to add or remove.
 * @param apply True to apply the modifier; false to remove it.
 */
void Player::AddSpellMod(SpellModifier* mod, bool apply)
{
    uint16 opcode = (mod->type == SPELLMOD_FLAT) ? SMSG_SET_FLAT_SPELL_MODIFIER : SMSG_SET_PCT_SPELL_MODIFIER;

    for (int eff = 0; eff < 64; ++eff)
    {
        uint64 _mask = uint64(1) << eff;
        if (mod->mask.IsFitToFamilyMask(_mask))
        {
            int32 val = 0;
            for (SpellModList::const_iterator itr = m_spellMods[mod->op].begin(); itr != m_spellMods[mod->op].end(); ++itr)
            {
                if ((*itr)->type == mod->type && ((*itr)->mask.IsFitToFamilyMask(_mask)))
                {
                    val += (*itr)->value;
                }
            }
            val += apply ? mod->value : -(mod->value);
            WorldPacket data(opcode, (1 + 1 + 4));
            data << uint8(eff);
            data << uint8(mod->op);
            data << int32(val);
            SendDirectMessage(&data);
        }
    }

    if (apply)
    {
        m_spellMods[mod->op].push_back(mod);
    }
    else
    {
        if (mod->charges == -1)
        {
            --m_SpellModRemoveCount;
        }
        m_spellMods[mod->op].remove(mod);
        delete mod;
    }
}

// send Proficiency
void Player::SendProficiency(ItemClass itemClass, uint32 itemSubclassMask)
{
    WorldPacket data(SMSG_SET_PROFICIENCY, 1 + 4);
    data << uint8(itemClass) << uint32(itemSubclassMask);
    GetSession()->SendPacket(&data);
}

/**
 * @brief Removes petition ownership and signatures associated with a player.
 *
 * @param guid The player GUID whose petition data should be removed.
 */
void Player::RemovePetitionsAndSigns(ObjectGuid guid)
{
    uint32 lowguid = guid.GetCounter();

    QueryResult* result = CharacterDatabase.PQuery("SELECT `ownerguid`,`petitionguid` FROM `petition_sign` WHERE `playerguid` = '%u'", lowguid);
    if (result)
    {
        do                                                  // this part effectively does nothing, since the deletion / modification only takes place _after_ the PetitionQuery. Though I don't know if the result remains intact if I execute the delete query beforehand.
        {
            // and SendPetitionQueryOpcode reads data from the DB
            Field* fields = result->Fetch();
            ObjectGuid ownerguid   = ObjectGuid(HIGHGUID_PLAYER, fields[0].GetUInt32());
            ObjectGuid petitionguid = ObjectGuid(HIGHGUID_ITEM, fields[1].GetUInt32());

            // send update if charter owner in game
            Player* owner = sObjectMgr.GetPlayer(ownerguid);
            if (owner)
            {
                owner->GetSession()->SendPetitionQueryOpcode(petitionguid);
            }
        }
        while (result->NextRow());

        delete result;

        CharacterDatabase.PExecute("DELETE FROM `petition_sign` WHERE `playerguid` = '%u'", lowguid);
    }

    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute("DELETE FROM `petition` WHERE `ownerguid` = '%u'", lowguid);
    CharacterDatabase.PExecute("DELETE FROM `petition_sign` WHERE `ownerguid` = '%u'", lowguid);
    CharacterDatabase.CommitTransaction();
}

/**
 * @brief Sets the player's accumulated rested experience bonus.
 *
 * @param rest_bonus_new The new rested bonus amount.
 */
void Player::SetRestBonus(float rest_bonus_new)
{
    // Prevent resting on max level
    if (getLevel() >= sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
    {
        rest_bonus_new = 0;
    }

    if (rest_bonus_new < 0)
    {
        rest_bonus_new = 0;
    }

    float rest_bonus_max = (float)GetUInt32Value(PLAYER_NEXT_LEVEL_XP) * 1.5f / 2.0f;

    if (rest_bonus_new > rest_bonus_max)
    {
        m_rest_bonus = rest_bonus_max;
    }
    else
    {
        m_rest_bonus = rest_bonus_new;
    }

    // update data for client
    if (m_rest_bonus > 10)
    {
        SetByteValue(PLAYER_BYTES_2, 3, REST_STATE_RESTED);
    }
    else if (m_rest_bonus <= 1)
    {
        SetByteValue(PLAYER_BYTES_2, 3, REST_STATE_NORMAL);
    }

    // RestTickUpdate
    SetUInt32Value(PLAYER_REST_STATE_EXPERIENCE, uint32(m_rest_bonus));
}

/**
 * @brief Updates visibility of nearby stealthed units based on detection checks.
 */
void Player::HandleStealthedUnitsDetection()
{
    std::list<Unit*> stealthedUnits;

    MaNGOS::AnyStealthedCheck u_check(this);
    MaNGOS::UnitListSearcher<MaNGOS::AnyStealthedCheck > searcher(stealthedUnits, u_check);
    Cell::VisitAllObjects(this, searcher, MAX_PLAYER_STEALTH_DETECT_RANGE);

    WorldObject const* viewPoint = GetCamera().GetBody();

    for (std::list<Unit*>::const_iterator i = stealthedUnits.begin(); i != stealthedUnits.end(); ++i)
    {
        if ((*i) == this)
        {
            continue;
        }

        bool hasAtClient = HaveAtClient((*i));
        bool hasDetected = (*i)->IsVisibleForOrDetect(this, viewPoint, true);

        if (hasDetected)
        {
            if (!hasAtClient)
            {
                ObjectGuid i_guid = (*i)->GetObjectGuid();
                (*i)->SendCreateUpdateToPlayer(this);
                m_clientGUIDs.insert(i_guid);

                DEBUG_FILTER_LOG(LOG_FILTER_VISIBILITY_CHANGES, "UpdateVisibilityOf(): %s is detected in stealth by player %u. Distance = %f", i_guid.GetString().c_str(), GetGUIDLow(), GetDistance(*i));

                // target aura duration for caster show only if target exist at caster client
                // send data at target visibility change (adding to client)
                if ((*i) != this && (*i)->isType(TYPEMASK_UNIT))
                {
                    SendAuraDurationsForTarget(*i);
                }
            }
        }
        else
        {
            if (hasAtClient)
            {
                (*i)->DestroyForPlayer(this);
                m_clientGUIDs.erase((*i)->GetObjectGuid());
            }
        }
    }
}

/**
 * @brief Starts a taxi flight across a sequence of taxi nodes.
 *
 * @param nodes The ordered taxi node path to travel.
 * @param npc The taxi master providing the route, or NULL for spell/scripted travel.
 * @param spellid The spell initiating the taxi flight, if any.
 * @return True if the flight started successfully; otherwise, false.
 */
bool Player::ActivateTaxiPathTo(std::vector<uint32> const& nodes, Creature* npc /*= NULL*/, uint32 spellid /*= 0*/)
{
    if (nodes.size() < 2)
    {
        return false;
    }

    // not let cheating with start flight in time of logout process || if casting not finished || while in combat || if not use Spell's with EffectSendTaxi
    if (GetSession()->isLogingOut() || IsInCombat())
    {
        GetSession()->SendActivateTaxiReply(ERR_TAXIPLAYERBUSY);
        return false;
    }

    if (HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_CLIENT_CONTROL_LOST))
    {
        return false;
    }

    // taximaster case
    if (npc)
    {
        // not let cheating with start flight mounted
        if (IsMounted())
        {
            GetSession()->SendActivateTaxiReply(ERR_TAXIPLAYERALREADYMOUNTED);
            return false;
        }

        if (IsInDisallowedMountForm())
        {
            GetSession()->SendActivateTaxiReply(ERR_TAXIPLAYERSHAPESHIFTED);
            return false;
        }

        // not let cheating with start flight in time of logout process || if casting not finished || while in combat || if not use Spell's with EffectSendTaxi
        if (IsNonMeleeSpellCasted(false))
        {
            GetSession()->SendActivateTaxiReply(ERR_TAXIPLAYERBUSY);
            return false;
        }
    }
    // cast case or scripted call case
    else
    {
        RemoveSpellsCausingAura(SPELL_AURA_MOUNTED);

        if (IsInDisallowedMountForm())
        {
            RemoveSpellsCausingAura(SPELL_AURA_MOD_SHAPESHIFT);
        }

        if (Spell* spell = GetCurrentSpell(CURRENT_GENERIC_SPELL))
        {
            if (spell->m_spellInfo->Id != spellid)
            {
                InterruptSpell(CURRENT_GENERIC_SPELL, false);
            }
        }

        InterruptSpell(CURRENT_AUTOREPEAT_SPELL, false);

        if (Spell* spell = GetCurrentSpell(CURRENT_CHANNELED_SPELL))
        {
            if (spell->m_spellInfo->Id != spellid)
            {
                InterruptSpell(CURRENT_CHANNELED_SPELL, true);
            }
        }
    }

    uint32 sourcenode = nodes[0];

    // starting node too far away (cheat?)
    TaxiNodesEntry const* node = sTaxiNodesStore.LookupEntry(sourcenode);
    if (!node)
    {
        GetSession()->SendActivateTaxiReply(ERR_TAXINOSUCHPATH);
        return false;
    }

    // check node starting pos data set case if provided
    if (node->x != 0.0f || node->y != 0.0f || node->z != 0.0f)
    {
        if (node->map_id != GetMapId() ||
            (node->x - GetPositionX()) * (node->x - GetPositionX()) +
            (node->y - GetPositionY()) * (node->y - GetPositionY()) +
            (node->z - GetPositionZ()) * (node->z - GetPositionZ()) >
            (2 * INTERACTION_DISTANCE) * (2 * INTERACTION_DISTANCE) * (2 * INTERACTION_DISTANCE))
        {
            GetSession()->SendActivateTaxiReply(ERR_TAXITOOFARAWAY);
            return false;
        }
    }
    // node must have pos if taxi master case (npc != NULL)
    else if (npc)
    {
        GetSession()->SendActivateTaxiReply(ERR_TAXIUNSPECIFIEDSERVERERROR);
        return false;
    }

    // Prepare to flight start now

    // stop combat at start taxi flight if any
    CombatStop();

    // stop trade (client cancel trade at taxi map open but cheating tools can be used for reopen it)
    TradeCancel(true);

    // clean not finished taxi path if any
    m_taxi.ClearTaxiDestinations();

    // 0 element current node
    m_taxi.AddTaxiDestination(sourcenode);

    // fill destinations path tail
    uint32 sourcepath = 0;
    uint32 totalcost = 0;

    uint32 prevnode = sourcenode;

    for (uint32 i = 1; i < nodes.size(); ++i)
    {
        uint32 path, cost;

        uint32 lastnode = nodes[i];
        sObjectMgr.GetTaxiPath(prevnode, lastnode, path, cost);

        if (!path)
        {
            m_taxi.ClearTaxiDestinations();
            return false;
        }

        totalcost += cost;

        if (prevnode == sourcenode)
        {
            sourcepath = path;
        }

        m_taxi.AddTaxiDestination(lastnode);

        prevnode = lastnode;
    }

    // get mount model (in case non taximaster (npc==NULL) allow more wide lookup)
    uint32 mount_display_id = sObjectMgr.GetTaxiMountDisplayId(sourcenode, GetTeam(), npc == NULL);

    // in spell case allow 0 model
    if ((mount_display_id == 0 && spellid == 0) || sourcepath == 0)
    {
        GetSession()->SendActivateTaxiReply(ERR_TAXIUNSPECIFIEDSERVERERROR);

        m_taxi.ClearTaxiDestinations();
        return false;
    }

    uint32 money = GetMoney();

    if (npc)
    {
        totalcost = (uint32)ceil(totalcost * GetReputationPriceDiscount(npc));
    }

    if (money < totalcost)
    {
        GetSession()->SendActivateTaxiReply(ERR_TAXINOTENOUGHMONEY);

        m_taxi.ClearTaxiDestinations();
        return false;
    }

    // Checks and preparations done, DO FLIGHT
    ModifyMoney(-(int32)totalcost);

    // prevent stealth flight
    RemoveSpellsCausingAura(SPELL_AURA_MOD_STEALTH);

    if (sWorld.getConfig(CONFIG_BOOL_INSTANT_TAXI))
    {
        TaxiNodesEntry const* lastnode = sTaxiNodesStore.LookupEntry(nodes[nodes.size() - 1]);
        m_taxi.ClearTaxiDestinations();
        TeleportTo(lastnode->map_id, lastnode->x, lastnode->y, lastnode->z, GetOrientation());
        return false;
    }
    else
    {
        GetSession()->SendActivateTaxiReply(ERR_TAXIOK);
        GetSession()->SendDoFlight(mount_display_id, sourcepath);
    }

    return true;
}

/**
 * @brief Starts a taxi flight using a direct taxi path identifier.
 *
 * @param taxi_path_id The taxi path identifier to use.
 * @param spellid The spell initiating the taxi flight, if any.
 * @return True if the flight started successfully; otherwise, false.
 */
bool Player::ActivateTaxiPathTo(uint32 taxi_path_id, uint32 spellid /*= 0*/)
{
    TaxiPathEntry const* entry = sTaxiPathStore.LookupEntry(taxi_path_id);
    if (!entry)
    {
        return false;
    }

    std::vector<uint32> nodes;

    nodes.resize(2);
    nodes[0] = entry->from;
    nodes[1] = entry->to;

    return ActivateTaxiPathTo(nodes, NULL, spellid);
}

/**
 * @brief Resumes an interrupted taxi flight from the nearest path node.
 */
void Player::ContinueTaxiFlight()
{
    uint32 sourceNode = m_taxi.GetTaxiSource();
    if (!sourceNode)
    {
        return;
    }

    DEBUG_LOG("WORLD: Restart character %u taxi flight", GetGUIDLow());

    uint32 mountDisplayId = sObjectMgr.GetTaxiMountDisplayId(sourceNode, GetTeam(), true);
    uint32 path = m_taxi.GetCurrentTaxiPath();

    // search appropriate start path node
    uint32 startNode = 0;

    TaxiPathNodeList const& nodeList = sTaxiPathNodesByPath[path];

    float distNext =
        (nodeList[0].x - GetPositionX()) * (nodeList[0].x - GetPositionX()) +
        (nodeList[0].y - GetPositionY()) * (nodeList[0].y - GetPositionY()) +
        (nodeList[0].z - GetPositionZ()) * (nodeList[0].z - GetPositionZ());

    for (uint32 i = 1; i < nodeList.size(); ++i)
    {
        TaxiPathNodeEntry const& node = nodeList[i];
        TaxiPathNodeEntry const& prevNode = nodeList[i - 1];

        // skip nodes at another map
        if (node.mapid != GetMapId())
        {
            continue;
        }

        float distPrev = distNext;

        distNext =
            (node.x - GetPositionX()) * (node.x - GetPositionX()) +
            (node.y - GetPositionY()) * (node.y - GetPositionY()) +
            (node.z - GetPositionZ()) * (node.z - GetPositionZ());

        float distNodes =
            (node.x - prevNode.x) * (node.x - prevNode.x) +
            (node.y - prevNode.y) * (node.y - prevNode.y) +
            (node.z - prevNode.z) * (node.z - prevNode.z);

        if (distNext + distPrev < distNodes)
        {
            startNode = i;
            break;
        }
    }

    GetSession()->SendDoFlight(mountDisplayId, path, startNode);
}

/**
 * @brief Saves battleground return position and instance data to the database.
 */
void Player::_SaveBGData()
{
    // nothing save
    if (!m_bgData.m_needSave)
    {
        return;
    }

    static SqlStatementID delBGData ;
    static SqlStatementID insBGData ;

    SqlStatement stmt =  CharacterDatabase.CreateStatement(delBGData, "DELETE FROM `character_battleground_data` WHERE `guid` = ?");

    stmt.PExecute(GetGUIDLow());

    if (m_bgData.bgInstanceID)
    {
        stmt = CharacterDatabase.CreateStatement(insBGData, "INSERT INTO `character_battleground_data` VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
        /* guid, bgInstanceID, bgTeam, x, y, z, o, map */
        stmt.addUInt32(GetGUIDLow());
        stmt.addUInt32(m_bgData.bgInstanceID);
        stmt.addUInt32(uint32(m_bgData.bgTeam));
        stmt.addFloat(m_bgData.joinPos.coord_x);
        stmt.addFloat(m_bgData.joinPos.coord_y);
        stmt.addFloat(m_bgData.joinPos.coord_z);
        stmt.addFloat(m_bgData.joinPos.orientation);
        stmt.addUInt32(m_bgData.joinPos.mapid);

        stmt.Execute();
    }

    m_bgData.m_needSave = false;
}
