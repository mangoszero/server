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

#include "PlayerLogger.h"
#include "ObjectAccessor.h"
#include "Database/DatabaseEnv.h"
#include "World.h"
#include "Log.h"

/**
 * @brief Creates a player logger for a specific player.
 *
 * @param guid The GUID of the player whose events are tracked.
 */
PlayerLogger::PlayerLogger(ObjectGuid const & guid) : logActiveMask(0), playerGuid(guid.GetCounter())
{
    for (uint8 i = 0; i < MAX_PLAYER_LOG_ENTITIES; ++i)
    {
        data[i] = NULL;
    }
}

/**
 * @brief Destroys the player logger and frees all buffered log data.
 */
PlayerLogger::~PlayerLogger()
{
    for (uint8 i = 0; i < MAX_PLAYER_LOG_ENTITIES; ++i)
    {
        if (data[i])
        {
            data[i]->clear();
            delete data[i];
        }
    }
}

/**
 * @brief Initializes storage for a log entity type.
 *
 * @param entity The log entity type to initialize.
 * @param maxLength Optional reservation size for the backing container.
 */
void PlayerLogger::Initialize(PlayerLogEntity entity, uint32 maxLength)
{
    if (data[entity])
    {
        data[entity]->clear();
    }
    else
    {
        if (IsLoggingActive(entity))
            sLog.outDebug("PlayerLogger: no data but activity flag set for log type %u!", entity);
        switch (entity)
        {
        case PLAYER_LOG_DAMAGE_GET:
        case PLAYER_LOG_DAMAGE_DONE:
            data[entity] = (PlayerLogBaseType*)(new std::vector<PlayerLogDamage>);
            break;
        case PLAYER_LOG_LOOTING:
            data[entity] = (PlayerLogBaseType*)(new std::vector<PlayerLogLooting>);
            break;
        case PLAYER_LOG_TRADE:
            data[entity] = (PlayerLogBaseType*)(new std::vector<PlayerLogTrading>);
            break;
        case PLAYER_LOG_KILL:
            data[entity] = (PlayerLogBaseType*)(new std::vector<PlayerLogKilling>);
            break;
        case PLAYER_LOG_POSITION:
            data[entity] = (PlayerLogBaseType*)(new std::vector<PlayerLogPosition>);
            break;
        case PLAYER_LOG_PROGRESS:
            data[entity] = (PlayerLogBaseType*)(new std::vector<PlayerLogProgress>);
            break;
        default:
            sLog.outError("PlayerLogger: unknown logging type %u initiated, ignoring.", entity);
            break;
        }
    }
    if (maxLength)
    {
        data[entity]->reserve(maxLength);
    }
}

/**
 * @brief Clears stored data for the selected log mask.
 *
 * @param mask The set of log entity types to clear.
 */
void PlayerLogger::Clean(PlayerLogMask mask)
{
    for (uint8 i = 0; i < MAX_PLAYER_LOG_ENTITIES; ++i)
    {
        if ((mask & CalcLogMask(PlayerLogEntity(i))) == 0)    // note that actual data presence is not checked here!
        {
            continue;
        }
        if (data[i] == NULL)
        {
            sLog.outError("PlayerLogging: flag for logtype %u set but no init was called! Ignored.", i);
            continue;
        }
        SetLogActiveMask(PlayerLogEntity(i), false);
        data[i]->clear();
    }
}

/**
 * @brief Saves selected log buffers to the database.
 *
 * @param mask The set of log entity types to persist.
 * @param removeSaved true to clear buffers after successful save.
 * @param insideTransaction true if the caller already manages the transaction.
 * @return true if any data was written; otherwise, false.
 */
bool PlayerLogger::SaveToDB(PlayerLogMask mask, bool removeSaved, bool insideTransaction)
{
    bool written = false;
    uint64 serverStart = uint64(sWorld.GetStartTime());
    for (uint8 i = 0; i < MAX_PLAYER_LOG_ENTITIES; ++i)
    {
        if ((mask & CalcLogMask(PlayerLogEntity(i))) == 0 || data[i] == NULL)
        {
            continue;
        }

        if (!insideTransaction)
        {
            CharacterDatabase.BeginTransaction();
        }
        written = true;
        for (uint8 id = 0; id < data[i]->size(); ++id)
        {
            switch (PlayerLogEntity(i))
            {
            case PLAYER_LOG_DAMAGE_GET:
                {
                PlayerLogDamage info = *(PlayerLogDamage*)(&data[i]->at(id));
                static SqlStatementID dmgGetStmt;
                SqlStatement stmt = CharacterDatabase.CreateStatement(dmgGetStmt, "INSERT INTO `playerlog_damage_get` SET `guid` = ?, `time` = ?, `aggressor` = ?, `isPlayer` = ?, `damage` = ?, `spell` = ?");
                stmt.addUInt32(playerGuid);
                stmt.addUInt64(info.timestamp + serverStart);
                stmt.addUInt32(info.GetId());
                stmt.addBool(info.IsPlayer());
                stmt.addInt32(info.damage);
                stmt.addUInt16(info.spell);
                stmt.Execute();
                }
                break;
            case PLAYER_LOG_DAMAGE_DONE:
                {
                PlayerLogDamage info = *(PlayerLogDamage*)(&data[i]->at(id));
                static SqlStatementID dmgDoneStmt;
                SqlStatement stmt = CharacterDatabase.CreateStatement(dmgDoneStmt, "INSERT INTO `playerlog_damage_done` SET `guid` = ?, `time` = ?, `victim` = ?, `isPlayer` = ?, `damage` = ?, `spell` = ?");
                stmt.addUInt32(playerGuid);
                stmt.addUInt64(info.timestamp + serverStart);
                stmt.addUInt32(info.GetId());
                stmt.addBool(info.IsPlayer());
                stmt.addInt32(info.damage);
                stmt.addUInt16(info.spell);
                stmt.Execute();
                }
                break;
            case PLAYER_LOG_LOOTING:
                {
                PlayerLogLooting info = *(PlayerLogLooting*)(&data[i]->at(id));
                static SqlStatementID lootStmt;
                SqlStatement stmt = CharacterDatabase.CreateStatement(lootStmt, "INSERT INTO `playerlog_looting` SET `guid` = ?, `time`= ?, `item` = ?, `sourceType` = ?, `sourceEntry` = ?");
                stmt.addUInt32(playerGuid);
                stmt.addUInt64(info.timestamp + serverStart);
                stmt.addUInt32(info.GetItemEntry());
                stmt.addUInt8(uint8(info.GetLootSourceType()));
                stmt.addUInt32(info.droppedBy);
                stmt.Execute();
                }
                break;
            case PLAYER_LOG_TRADE:
                {
                PlayerLogTrading info = *(PlayerLogTrading*)(&data[i]->at(id));
                static SqlStatementID tradeStmt;
                SqlStatement stmt = CharacterDatabase.CreateStatement(tradeStmt, "INSERT INTO `playerlog_trading` SET `guid` = ?, `time`= ?, `itemEntry` = ?, `itemGuid` = ?, `aquired` = ?, `partner` = ?");
                stmt.addUInt32(playerGuid);
                stmt.addUInt64(info.timestamp + serverStart);
                stmt.addUInt32(info.GetItemEntry());
                stmt.addUInt32(info.itemGuid);
                stmt.addBool(info.IsItemAquired());
                stmt.addUInt16(info.partner);
                stmt.Execute();
                }
                break;
            case PLAYER_LOG_KILL:
                {
                PlayerLogKilling info = *(PlayerLogKilling*)(&data[i]->at(id));
                static SqlStatementID killStmt;
                SqlStatement stmt = CharacterDatabase.CreateStatement(killStmt, "INSERT INTO `playerlog_killing` SET `guid` = ?, `time`= ?, `iskill` = ?, `entry` = ?, `victimGuid` = ?");
                stmt.addUInt32(playerGuid);
                stmt.addUInt64(info.timestamp + serverStart);
                stmt.addBool(info.IsKill());
                stmt.addUInt32(info.GetUnitEntry());
                stmt.addUInt32(info.unitGuid);
                stmt.Execute();
                }
                break;
            case PLAYER_LOG_POSITION:
                {
                PlayerLogPosition info = *(PlayerLogPosition*)(&data[i]->at(id));
                static SqlStatementID posStmt;
                SqlStatement stmt = CharacterDatabase.CreateStatement(posStmt, "INSERT INTO `playerlog_position` SET `guid` = ?, `time`= ?, `map` = ?, `posx` = ?, `posy` = ?, `posz` = ?");
                stmt.addUInt32(playerGuid);
                stmt.addUInt64(info.timestamp + serverStart);
                stmt.addUInt16(info.map);
                stmt.addFloat(info.x);
                stmt.addFloat(info.y);
                stmt.addFloat(info.z);
                stmt.Execute();
                }
                break;
            case PLAYER_LOG_PROGRESS:
                {
                PlayerLogProgress info = *(PlayerLogProgress*)(&data[i]->at(id));
                static SqlStatementID progStmt;
                SqlStatement stmt = CharacterDatabase.CreateStatement(progStmt, "INSERT INTO `playerlog_progress` SET `guid` = ?, `time` = ?, `type` = ?, `level` = ?, `data` = ?, `map` = ?, `posx` = ?, `posy` = ?, `posz` = ?");
                stmt.addUInt32(playerGuid);
                stmt.addUInt64(info.timestamp + serverStart);
                stmt.addUInt8(info.progressType);
                stmt.addUInt8(info.level);
                stmt.addUInt16(info.data);
                stmt.addUInt16(info.map);
                stmt.addFloat(info.x);
                stmt.addFloat(info.y);
                stmt.addFloat(info.z);
                stmt.Execute();
                }
            }
        }
        Stop(PlayerLogEntity(i));
        if (removeSaved)
        {
            data[i]->clear();
        }
    }
    if (written && !insideTransaction)
    {
        CharacterDatabase.CommitTransaction();
    }

    return written;
}

/**
 * @brief Starts the standard combat logging set.
 */
void PlayerLogger::StartCombatLogging()
{
    StartLogging(PLAYER_LOG_DAMAGE_GET);
    StartLogging(PLAYER_LOG_DAMAGE_DONE);
}

/**
 * @brief Starts logging for a specific entity type.
 *
 * @param entity The log entity type to activate.
 */
void PlayerLogger::StartLogging(PlayerLogEntity entity)
{
    if (data[entity] == NULL)
    {
        sLog.outError("PlayerLogger: StartLogging without init! Fixing, check your code.");
        Initialize(entity);
    }
    else
    {
        if (data[entity]->size() > 0)
            sLog.outDebug("PlayerLogger: dropped old data for type %u player GUID %u!", entity, playerGuid);
        data[entity]->clear();
    }

    SetLogActiveMask(entity, true);
}

/**
 * @brief Stops logging for a specific entity type.
 *
 * @param entity The log entity type to deactivate.
 * @return The number of buffered records for that entity.
 */
uint32 PlayerLogger::Stop(PlayerLogEntity entity)
{
    SetLogActiveMask(entity, false);
    sLog.outDebug("PlayerLogger: logging type %u stopped for player %u at %lu records.", entity, playerGuid, data[entity]->size());
    return data[entity]->size();
}

/**
 * @brief Truncates selected log buffers to a maximum record count.
 *
 * @param mask The set of log entity types to inspect.
 * @param maxRecords The maximum number of records to retain per selected log.
 */
void PlayerLogger::CheckAndTruncate(PlayerLogMask mask, uint32 maxRecords)
{
    for (uint8 i = 0; i < MAX_PLAYER_LOG_ENTITIES; ++i)
    {
        if ((mask & CalcLogMask(PlayerLogEntity(i))) == 0)
        {
            continue;
        }
        if (data[i]->size() > maxRecords)
        {
            switch (PlayerLogEntity(i))
            {
            case PLAYER_LOG_DAMAGE_GET:
            case PLAYER_LOG_DAMAGE_DONE:
                {
                std::vector<PlayerLogDamage>* v = (std::vector<PlayerLogDamage>*)data[i];
                std::vector<PlayerLogDamage>::iterator itr = v->begin();
                v->erase(itr, itr + v->size() - maxRecords);
                }
                break;
            case PLAYER_LOG_LOOTING:
                {
                std::vector<PlayerLogLooting>* v = (std::vector<PlayerLogLooting>*)data[i];
                std::vector<PlayerLogLooting>::iterator itr = v->begin();
                v->erase(itr, itr + v->size() - maxRecords);
                }
                break;
            case PLAYER_LOG_TRADE:
                {
                std::vector<PlayerLogTrading>* v = (std::vector<PlayerLogTrading>*)data[i];
                std::vector<PlayerLogTrading>::iterator itr = v->begin();
                v->erase(itr, itr + v->size() - maxRecords);
                }
                break;
            case PLAYER_LOG_KILL:
                {
                std::vector<PlayerLogKilling>* v = (std::vector<PlayerLogKilling>*)data[i];
                std::vector<PlayerLogKilling>::iterator itr = v->begin();
                v->erase(itr, itr + v->size() - maxRecords);
                }
                break;
            case PLAYER_LOG_POSITION:
                {
                std::vector<PlayerLogPosition>* v = (std::vector<PlayerLogPosition>*)data[i];
                std::vector<PlayerLogPosition>::iterator itr = v->begin();
                v->erase(itr, itr + v->size() - maxRecords);
                }
                break;
            case PLAYER_LOG_PROGRESS:
                {
                std::vector<PlayerLogProgress>* v = (std::vector<PlayerLogProgress>*)data[i];
                std::vector<PlayerLogProgress>::iterator itr = v->begin();
                v->erase(itr, itr + v->size() - maxRecords);
                }
                break;
            }
        }
    }
}

/**
 * @brief Logs a damage or heal event for the player.
 *
 * @param done true when the player dealt the damage; false when the player received it.
 * @param damage The damage amount.
 * @param heal The heal amount.
 * @param unitGuid The related unit GUID.
 * @param spell The related spell identifier.
 */
void PlayerLogger::LogDamage(bool done, uint16 damage, uint16 heal, ObjectGuid const & unitGuid, uint16 spell)
{
    if (!IsLoggingActive(done ? PLAYER_LOGMASK_DAMAGE_DONE : PLAYER_LOGMASK_DAMAGE_GET))
    {
        return;
    }
    PlayerLogDamage log = PlayerLogDamage(sWorld.GetUptime());
    log.dmgUnit = (unitGuid.GetCounter() == playerGuid) ? 0 : (unitGuid.IsPlayer() ? unitGuid.GetCounter() : unitGuid.GetEntry());
    log.SetCreature(unitGuid.IsCreatureOrPet());
    log.damage = damage > 0 ? int16(damage) : -int16(heal);
    log.spell = spell;
    ((std::vector<PlayerLogDamage>*)(data[done ? PLAYER_LOG_DAMAGE_DONE : PLAYER_LOG_DAMAGE_GET]))->push_back(log);
}

/**
 * @brief Records a looting event for the player.
 *
 * @param type The loot source type.
 * @param droppedBy The source object GUID, if available.
 * @param itemGuid The looted item GUID.
 * @param id The fallback source identifier.
 */
void PlayerLogger::LogLooting(LootSourceType type, ObjectGuid const & droppedBy, ObjectGuid const & itemGuid, uint32 id)
{
    if (!IsLoggingActive(PLAYER_LOGMASK_LOOTING))
    {
        return;
    }
    PlayerLogLooting log = PlayerLogLooting(sWorld.GetUptime());
    log.itemEntry = itemGuid.GetEntry();
    log.SetLootSourceType(type);
    log.itemGuid = itemGuid.GetCounter();
    log.droppedBy = droppedBy.IsEmpty() ? id : droppedBy.GetEntry();
    ((std::vector<PlayerLogLooting>*)(data[PLAYER_LOG_LOOTING]))->push_back(log);
}

/**
 * @brief Records a trade item transfer for the player.
 *
 * @param aquire true when the player receives the item; false when giving it away.
 * @param partner The trading partner GUID.
 * @param itemGuid The traded item GUID.
 */
void PlayerLogger::LogTrading(bool aquire, ObjectGuid const & partner, ObjectGuid const & itemGuid)
{
    if (!IsLoggingActive(PLAYER_LOGMASK_TRADE))
    {
        return;
    }
    PlayerLogTrading log = PlayerLogTrading(sWorld.GetUptime());
    log.itemEntry = itemGuid.GetEntry();
    log.SetItemAquired(aquire);
    log.itemGuid = itemGuid.GetCounter();
    log.partner = partner.GetCounter();
    ((std::vector<PlayerLogTrading>*)(data[PLAYER_LOG_TRADE]))->push_back(log);
}

/**
 * @brief Records a kill or death event for the player.
 *
 * @param killedEnemy true if the player killed the unit; false if killed by it.
 * @param unitGuid The related unit GUID.
 */
void PlayerLogger::LogKilling(bool killedEnemy, ObjectGuid const & unitGuid)
{
    if (!IsLoggingActive(PLAYER_LOGMASK_KILL))
    {
        return;
    }
    PlayerLogKilling log = PlayerLogKilling(sWorld.GetUptime());
    log.unitEntry = unitGuid.GetEntry();
    log.SetKill(killedEnemy);
    log.unitGuid = unitGuid.GetCounter();
    ((std::vector<PlayerLogKilling>*)(data[PLAYER_LOG_KILL]))->push_back(log);
}

/**
 * @brief Records the player's current position.
 */
void PlayerLogger::LogPosition()
{
    if (!IsLoggingActive(PLAYER_LOGMASK_POSITION))
    {
        return;
    }
    if (Player* pl = GetPlayer())
    {
        PlayerLogPosition log = PlayerLogPosition(sWorld.GetUptime());
        FillPosition(&log, pl);
        ((std::vector<PlayerLogPosition>*)(data[PLAYER_LOG_POSITION]))->push_back(log);
    }
}

/**
 * @brief Records a player progression event with position context.
 *
 * @param type The progress event type.
 * @param achieve The achieved level or stage value.
 * @param misc Additional event-specific data.
 */
void PlayerLogger::LogProgress(ProgressType type, uint8 achieve, uint16 misc)
{
    if (!IsLoggingActive(PLAYER_LOGMASK_PROGRESS))
    {
        return;
    }
    if (Player* pl = GetPlayer())
    {
        PlayerLogProgress log = PlayerLogProgress(sWorld.GetUptime());
        log.progressType = type;
        log.level = achieve;
        log.data = misc;
        FillPosition(&log, pl);
        ((std::vector<PlayerLogProgress>*)(data[PLAYER_LOG_PROGRESS]))->push_back(log);
    }
}

/**
 * @brief Enables or disables logging for a specific entity category.
 *
 * @param entity The logging category.
 * @param on true to enable logging; false to disable it.
 */
void PlayerLogger::SetLogActiveMask(PlayerLogEntity entity, bool on)
{
    if (on)
    {
        logActiveMask |= CalcLogMask(entity);
    }
    else
    {
        logActiveMask &= ~uint8(CalcLogMask(entity));
    }
}

/**
 * @brief Gets the current player or corpse-backed player instance for logging.
 *
 * @return The resolved player pointer, or null if unavailable.
 */
Player* PlayerLogger::GetPlayer() const
{
    Player* pl = sObjectAccessor.FindPlayer(ObjectGuid(HIGHGUID_PLAYER, playerGuid), true);
    if (!pl)
    {
        pl = sObjectAccessor.FindPlayer(ObjectGuid(HIGHGUID_CORPSE, playerGuid), true);
    }

    if (!pl)
    {
        sLog.outError("PlayerLogger: cannot get current player! Ignoring the record.");
    }

    return pl;
}

/**
 * @brief Copies a player's current map position into a log record.
 *
 * @param log The log record to populate.
 * @param me The player providing position data.
 */
void PlayerLogger::FillPosition(PlayerLogPosition* log, Player* me)
{
    log->map = uint16(me->GetMapId());
    log->x = me->GetPositionX();
    log->y = me->GetPositionY();
    log->z = me->GetPositionZ();
}
