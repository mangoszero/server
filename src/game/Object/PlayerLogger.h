/**
* MaNGOS is a full featured server for World of Warcraft, supporting
* the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
*
* Copyright (C) 2005-2019  MaNGOS project <http://getmangos.eu>
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

#ifndef MANGOS_H_PLAYERLOGGER
#define MANGOS_H_PLAYERLOGGER

#include "Platform/Define.h"
#include <vector>
class Player;
class ObjectGuid;

enum PlayerLogEntity
{
    PLAYER_LOG_DAMAGE_GET   = 0,
    PLAYER_LOG_DAMAGE_DONE  = 1,
    PLAYER_LOG_LOOTING      = 2,
    PLAYER_LOG_TRADE        = 3,
    PLAYER_LOG_KILL         = 4,
    PLAYER_LOG_POSITION     = 5,
    PLAYER_LOG_PROGRESS     = 6,
};
#define MAX_PLAYER_LOG_ENTITIES 7

enum PlayerLogMask
{
    PLAYER_LOGMASK_DAMAGE_GET   = 1,
    PLAYER_LOGMASK_DAMAGE_DONE  = 2,
    PLAYER_LOGMASK_LOOTING      = 4,
    PLAYER_LOGMASK_TRADE        = 8,
    PLAYER_LOGMASK_KILL         = 0x10,
    PLAYER_LOGMASK_POSITION     = 0x20,
    PLAYER_LOGMASK_PROGRESS     = 0x40,
};
#define PLAYER_LOGMASK_ANYTHING PlayerLogMask((1 << MAX_PLAYER_LOG_ENTITIES)-1)

enum ProgressType
{
    PROGRESS_LEVEL      = 0,
    PROGRESS_REPUTATION = 1,
    PROGRESS_BOSS_KILL  = 3,
    PROGRESS_PET_LEVEL  = 2,
};

struct PlayerLogBase
{
    uint32 timestamp;

    PlayerLogBase(uint32 _time) : timestamp(_time) {}
};
typedef std::vector<PlayerLogBase> PlayerLogBaseType;

struct PlayerLogDamage : public PlayerLogBase       // 10 bytes
{
    uint16 dmgUnit;     // guid for player, entry with highest bit set for mob incl. pet/guardian
    int16 damage;       // if negative then it's heal
    uint16 spell;       // 0 for melee autoattack

    void SetCreature(bool on) { if (on) dmgUnit |= 0x8000; else dmgUnit &= 0x7FFF; }
    bool IsPlayer() const { return (dmgUnit & 0x8000) == 0; }
    bool IsAutoAttack() const { return spell == 0; }
    bool GetHeal() const { return -damage; }
    uint16 GetId() const { return dmgUnit & 0x7FFF; }

    PlayerLogDamage(uint32 _time) : PlayerLogBase(_time) {}
};

enum LootSourceType
{
    LOOTSOURCE_CREATURE     = 0,
    LOOTSOURCE_GAMEOBJECT   = 1,
    LOOTSOURCE_SPELL        = 2,
    LOOTSOURCE_VENDOR       = 3,
    LOOTSOURCE_LETTER       = 4,
};

struct PlayerLogLooting : public PlayerLogBase      // 16 bytes
{
    uint32 droppedBy;   // entry of object, depends on sourceType
    uint32 itemGuid;    // item GUIDlow
    uint32 itemEntry;   // item entry, packet with LootSourceType

    LootSourceType GetLootSourceType() const { return LootSourceType((itemEntry >> 24) & 0xFF); }
    void SetLootSourceType(LootSourceType ltype) { itemEntry &= 0x00FFFFFF; itemEntry |= ltype << 24; }
    uint32 GetItemEntry() const { return itemEntry & 0x00FFFFFF; }

    PlayerLogLooting(uint32 _time) : PlayerLogBase(_time) {}
};

struct PlayerLogTrading : public PlayerLogBase      // 14 bytes
{
    uint32 itemGuid;    // item GUIDlow
    uint32 itemEntry;   // item entry, with highest bit: =1 item lost, =0 item aquired
    uint16 partner;     // GUID of the player - trade partner; 0 if no partner (item sold/destroyed)

    bool IsItemAquired() const { return (itemEntry & 0x80000000) == 0; }
    void SetItemAquired(bool aquired) { if (aquired) itemEntry &= 0x7FFFFFFF; else itemEntry |= 0x80000000; }
    uint32 GetItemEntry() const { return itemEntry & 0x7FFFFFFF; }

    PlayerLogTrading(uint32 _time) : PlayerLogBase(_time) {}
};

struct PlayerLogKilling : public PlayerLogBase      // 12 bytes
{
    uint32 unitGuid;    // GUID of unit
    uint32 unitEntry;   // entry of the unit (highest bit: 1 unit is killer, 0 unit is victim)

    bool IsKill() const { return (unitEntry & 0x80000000) == 0; }
    void SetKill(bool on) { if (on) unitEntry &= 0x7FFFFFFF; else unitEntry |= 0x80000000; }
    uint32 GetUnitEntry() const { return unitEntry & 0x7FFFFFFF; }

    PlayerLogKilling(uint32 _time) : PlayerLogBase(_time) {}
};

struct PlayerLogPosition : public PlayerLogBase     // 18 bytes
{
    float x, y, z;
    uint16 map;

    PlayerLogPosition(uint32 _time) : PlayerLogBase(_time) {}
};

struct PlayerLogProgress : public PlayerLogPosition // 18+4=22 bytes
{
    uint8 progressType; // enum ProgressType
    uint8 level;        // level achieved
    uint16 data;        // misc data, depends on ProgressType (like faction ID for reputation)

    PlayerLogProgress(uint32 _time) : PlayerLogPosition(_time) {}
};

class PlayerLogger
{
public:
    PlayerLogger(ObjectGuid const & guid);
    ~PlayerLogger();

    static inline PlayerLogMask CalcLogMask(PlayerLogEntity entity) { return PlayerLogMask(1 << entity); }

    // active logs check
    bool IsLoggingActive(PlayerLogMask mask) const { return (mask & logActiveMask) != 0; }
    bool IsLoggingActive(PlayerLogEntity entity) const { return IsLoggingActive(CalcLogMask(entity)); }

    // check active loggers and init missing ones
    void Initialize(PlayerLogEntity, uint32 maxLength = 0);

    // remove entries of type PlayerLogEntity
    void Clean(PlayerLogMask);

    // save to DB entries
    bool SaveToDB(PlayerLogMask, bool removeSaved = true, bool insideTransaction = false);

    // start logging for PLAYER_LOG_DAMAGE
    void StartCombatLogging();

    // start logging for strictly timed logs
    void StartLogging(PlayerLogEntity);

    // stop logging - returns number of entries logged currently
    uint32 Stop(PlayerLogEntity);

    // check and limit the total size of the log dropping older entries
    void CheckAndTruncate(PlayerLogMask, uint32 maxRecords);

    // logging itself
    void LogDamage(bool done, uint16 damage, uint16 heal, ObjectGuid const & unitGuid, uint16 spell);
    void LogLooting(LootSourceType type, ObjectGuid const & droppedBy, ObjectGuid const & itemGuid, uint32 id);
    void LogTrading(bool aquire, ObjectGuid const & partner, ObjectGuid const & itemGuid);
    void LogKilling(bool killedEnemy, ObjectGuid const & unitGuid);
    void LogPosition();
    void LogProgress(ProgressType type, uint8 achieve, uint16 misc = 0);

private:
    inline void SetLogActiveMask(PlayerLogEntity entity, bool on);
    Player* GetPlayer() const;
    void FillPosition(PlayerLogPosition* log, Player* me);

    uint32 playerGuid;

    std::vector<PlayerLogBase>* data[MAX_PLAYER_LOG_ENTITIES];
    uint8 logActiveMask;
};

#endif