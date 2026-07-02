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



#include "ObjectMgr.h"
#include "Database/DatabaseEnv.h"
#include "Policies/Singleton.h"
#include "Log.h"
#include "ProgressBar.h"
#include "SQLStorages.h"
#include "DBCStores.h"
#include "AuctionHouseBot/AhBotSystemOwner.h"
#include "LivingWorldAnchorPolicy.h"
#include "MotionGenerators/MotionMaster.h"
#include "MapManager.h"
#include "ObjectGuid.h"
#include "ScriptMgr.h"
#include "SpellMgr.h"
#include "World.h"
#include "Group.h"
#include "Transports.h"
#include "Language.h"
#include "PoolManager.h"
#include "GameEventMgr.h"
#include "Chat.h"
#include "MapPersistentStateMgr.h"
#include "SpellAuras.h"
#include "Util.h"
#include "GossipDef.h"
#include "Mail.h"
#include "Formulas.h"
#include "InstanceData.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "DisableMgr.h"
#include "ItemEnchantmentMgr.h"

struct SQLGameObjectLoader : public SQLStorageLoaderBase<SQLGameObjectLoader, SQLHashStorage>
{
    template<class D>
        void convert_from_str(uint32 /*field_pos*/, char const* src, D& dst)
    {
        dst = D(sScriptMgr.GetScriptId(src));
    }
};

/**
 * @brief Validates a referenced gameobject lock id.
 *
 * @param goInfo The gameobject template being checked.
 * @param dataN The referenced lock id.
 * @param N The source data index.
 */
inline void CheckGOLockId(GameObjectInfo const* goInfo, uint32 dataN, uint32 N)
{
    if (sLockStore.LookupEntry(dataN))
    {
        return;
    }

    sLog.outErrorDb("Gameobject (Entry: %u GoType: %u) have data%d=%u but lock (Id: %u) not found.",
        goInfo->id, goInfo->type, N, dataN, dataN);
}

/**
 * @brief Validates that a linked trap entry exists and is a trap gameobject.
 *
 * @param goInfo The gameobject template being checked.
 * @param dataN The referenced trap entry.
 * @param N The source data index.
 */
inline void CheckGOLinkedTrapId(GameObjectInfo const* goInfo, uint32 dataN, uint32 N)
{
    if (GameObjectInfo const* trapInfo = sGOStorage.LookupEntry<GameObjectInfo>(dataN))
    {
        if (trapInfo->type != GAMEOBJECT_TYPE_TRAP)
        {
            sLog.outErrorDb("Gameobject (Entry: %u GoType: %u) have data%d=%u but GO (Entry %u) have not GAMEOBJECT_TYPE_TRAP (%u) type.",
                goInfo->id, goInfo->type, N, dataN, dataN, GAMEOBJECT_TYPE_TRAP);
        }
    }
    else        // too many error reports about nonexistent trap templates
    {
        ERROR_DB_STRICT_LOG("Gameobject (Entry: %u GoType: %u) have data%d=%u but trap GO (Entry %u) not exist in `gameobject_template`.",
            goInfo->id, goInfo->type, N, dataN, dataN);
    }
}

/**
 * @brief Validates a referenced spell id in gameobject data.
 *
 * @param goInfo The gameobject template being checked.
 * @param dataN The referenced spell id.
 * @param N The source data index.
 */
inline void CheckGOSpellId(GameObjectInfo const* goInfo, uint32 dataN, uint32 N)
{
    if (sSpellStore.LookupEntry(dataN))
    {
        return;
    }

    sLog.outErrorDb("Gameobject (Entry: %u GoType: %u) have data%d=%u but Spell (Entry %u) not exist.",
        goInfo->id, goInfo->type, N, dataN, dataN);
}

/**
 * @brief Validates and clamps chair height data for a gameobject.
 *
 * @param goInfo The gameobject template being checked.
 * @param dataN The chair height value.
 * @param N The source data index.
 */
inline void CheckAndFixGOChairHeightId(GameObjectInfo const* goInfo, uint32 const& dataN, uint32 N)
{
    if (dataN <= (UNIT_STAND_STATE_SIT_HIGH_CHAIR - UNIT_STAND_STATE_SIT_LOW_CHAIR))
    {
        return;
    }

    sLog.outErrorDb("Gameobject (Entry: %u GoType: %u) have data%d=%u but correct chair height in range 0..%i.",
        goInfo->id, goInfo->type, N, dataN, UNIT_STAND_STATE_SIT_HIGH_CHAIR - UNIT_STAND_STATE_SIT_LOW_CHAIR);

    // prevent client and server unexpected work
    const_cast<uint32&>(dataN) = 0;
}

/**
 * @brief Validates a boolean no-damage-immune flag in gameobject data.
 *
 * @param goInfo The gameobject template being checked.
 * @param dataN The field value.
 * @param N The source data index.
 */
inline void CheckGONoDamageImmuneId(GameObjectInfo const* goInfo, uint32 dataN, uint32 N)
{
    // 0/1 correct values
    if (dataN <= 1)
    {
        return;
    }

    sLog.outErrorDb("Gameobject (Entry: %u GoType: %u) have data%d=%u but expected boolean (0/1) noDamageImmune field value.",
        goInfo->id, goInfo->type, N, dataN);
}

/**
 * @brief Validates a boolean consumable flag in gameobject data.
 *
 * @param goInfo The gameobject template being checked.
 * @param dataN The field value.
 * @param N The source data index.
 */
inline void CheckGOConsumable(GameObjectInfo const* goInfo, uint32 dataN, uint32 N)
{
    // 0/1 correct values
    if (dataN <= 1)
    {
        return;
    }

    sLog.outErrorDb("Gameobject (Entry: %u GoType: %u) have data%d=%u but expected boolean (0/1) consumable field value.",
        goInfo->id, goInfo->type, N, dataN);
}

/**
 * @brief Validates and fixes minimum capture time data for a gameobject.
 *
 * @param goInfo The gameobject template being checked.
 * @param dataN The minimum capture time value.
 * @param N The source data index.
 */
inline void CheckAndFixGOCaptureMinTime(GameObjectInfo const* goInfo, uint32 const& dataN, uint32 N)
{
    if (dataN > 0)
    {
        return;
    }

    sLog.outErrorDb("Gameobject (Entry: %u GoType: %u) has data%d=%u but minTime field value must be > 0.",
        goInfo->id, goInfo->type, N, dataN);

    // prevent division through 0 exception
    const_cast<uint32&>(dataN) = 1;
}

/**
 * @brief Loads gameobject templates and validates type-specific fields.
 */
void ObjectMgr::LoadGameobjectInfo()
{
    SQLGameObjectLoader loader;
    loader.Load(sGOStorage);

    // some checks
    for (SQLStorageBase::SQLSIterator<GameObjectInfo> itr = sGOStorage.getDataBegin<GameObjectInfo>(); itr < sGOStorage.getDataEnd<GameObjectInfo>(); ++itr)
    {
        GameObjectInfo const* goInfo = itr.getValue();

        if (goInfo->size <= 0.0f)                           // prevent use too small scales
        {
            ERROR_DB_STRICT_LOG("Gameobject (Entry: %u GoType: %u) have too small size=%f",
                goInfo->id, goInfo->type, goInfo->size);
            const_cast<GameObjectInfo*>(goInfo)->size =  DEFAULT_OBJECT_SCALE;
        }

        // some GO types have unused go template, check goInfo->displayId at GO spawn data loading or ignore

        switch (goInfo->type)
        {
            case GAMEOBJECT_TYPE_DOOR:                      // 0
            {
                if (goInfo->door.lockId)
                {
                    CheckGOLockId(goInfo, goInfo->door.lockId, 1);
                }
                CheckGONoDamageImmuneId(goInfo, goInfo->door.noDamageImmune, 3);
                break;
            }
            case GAMEOBJECT_TYPE_BUTTON:                    // 1
            {
                if (goInfo->button.lockId)
                {
                    CheckGOLockId(goInfo, goInfo->button.lockId, 1);
                }
                if (goInfo->button.linkedTrapId)            // linked trap
                {
                    CheckGOLinkedTrapId(goInfo, goInfo->button.linkedTrapId, 3);
                }
                CheckGONoDamageImmuneId(goInfo, goInfo->button.noDamageImmune, 4);
                break;
            }
            case GAMEOBJECT_TYPE_QUESTGIVER:                // 2
            {
                if (goInfo->questgiver.lockId)
                {
                    CheckGOLockId(goInfo, goInfo->questgiver.lockId, 0);
                }
                CheckGONoDamageImmuneId(goInfo, goInfo->questgiver.noDamageImmune, 5);
                break;
            }
            case GAMEOBJECT_TYPE_CHEST:                     // 3
            {
                if (goInfo->chest.lockId)
                {
                    CheckGOLockId(goInfo, goInfo->chest.lockId, 0);
                }

                CheckGOConsumable(goInfo, goInfo->chest.consumable, 3);

                if (goInfo->chest.linkedTrapId)             // linked trap
                {
                    CheckGOLinkedTrapId(goInfo, goInfo->chest.linkedTrapId, 7);
                }
                break;
            }
            case GAMEOBJECT_TYPE_TRAP:                      // 6
            {
                if (goInfo->trap.lockId)
                {
                    CheckGOLockId(goInfo, goInfo->trap.lockId, 0);
                }
                /** disable check for while, too many nonexistent spells
                 *  if (goInfo->trap.spellId)                   // spell
                 *  {
                 *      CheckGOSpellId(goInfo,goInfo->trap.spellId,3);
                 *  }
                 */
                break;
            }
            case GAMEOBJECT_TYPE_CHAIR:                     // 7
                CheckAndFixGOChairHeightId(goInfo, goInfo->chair.height, 1);
                break;
            case GAMEOBJECT_TYPE_SPELL_FOCUS:               // 8
            {
                if (goInfo->spellFocus.focusId)
                {
                    if (!sSpellFocusObjectStore.LookupEntry(goInfo->spellFocus.focusId))
                    {
                        sLog.outErrorDb("Gameobject (Entry: %u GoType: %u) have data0=%u but SpellFocus (Id: %u) not exist.",
                            goInfo->id, goInfo->type, goInfo->spellFocus.focusId, goInfo->spellFocus.focusId);
                    }
                }

                if (goInfo->spellFocus.linkedTrapId)        // linked trap
                {
                    CheckGOLinkedTrapId(goInfo, goInfo->spellFocus.linkedTrapId, 2);
                }
                break;
            }
            case GAMEOBJECT_TYPE_GOOBER:                    // 10
            {
                if (goInfo->goober.lockId)
                {
                    CheckGOLockId(goInfo, goInfo->goober.lockId, 0);
                }

                CheckGOConsumable(goInfo, goInfo->goober.consumable, 3);

                if (goInfo->goober.pageId)                  // pageId
                {
                    if (!sPageTextStore.LookupEntry<PageText>(goInfo->goober.pageId))
                    {
                        sLog.outErrorDb("Gameobject (Entry: %u GoType: %u) have data7=%u but PageText (Entry %u) not exist.",
                            goInfo->id, goInfo->type, goInfo->goober.pageId, goInfo->goober.pageId);
                    }
                }

                /** disable check for while, too many nonexistent spells
                 *  if (goInfo->goober.spellId)                 // spell
                 *  {
                 *      CheckGOSpellId(goInfo,goInfo->goober.spellId,10);
                 *  }
                 */
                CheckGONoDamageImmuneId(goInfo, goInfo->goober.noDamageImmune, 11);
                if (goInfo->goober.linkedTrapId)            // linked trap
                {
                    CheckGOLinkedTrapId(goInfo, goInfo->goober.linkedTrapId, 12);
                }
                break;
            }
            case GAMEOBJECT_TYPE_AREADAMAGE:                // 12
            {
                if (goInfo->areadamage.lockId)
                {
                    CheckGOLockId(goInfo, goInfo->areadamage.lockId, 0);
                }
                break;
            }
            case GAMEOBJECT_TYPE_CAMERA:                    // 13
            {
                if (goInfo->camera.lockId)
                {
                    CheckGOLockId(goInfo, goInfo->camera.lockId, 0);
                }
                break;
            }
            case GAMEOBJECT_TYPE_MO_TRANSPORT:              // 15
            {
                if (goInfo->moTransport.taxiPathId)
                {
                    if (goInfo->moTransport.taxiPathId >= sTaxiPathNodesByPath.size() || sTaxiPathNodesByPath[goInfo->moTransport.taxiPathId].empty())
                    {
                        sLog.outErrorDb("Gameobject (Entry: %u GoType: %u) have data0=%u but TaxiPath (Id: %u) not exist.",
                            goInfo->id, goInfo->type, goInfo->moTransport.taxiPathId, goInfo->moTransport.taxiPathId);
                    }
                }
                break;
            }
            case GAMEOBJECT_TYPE_SUMMONING_RITUAL:          // 18
            {
                /** disable check for while, too many nonexistent spells
                 *  // always must have spell
                 *  CheckGOSpellId(goInfo,goInfo->summoningRitual.spellId,1);
                 */
                break;
            }
            case GAMEOBJECT_TYPE_SPELLCASTER:               // 22
            {
                // always must have spell
                CheckGOSpellId(goInfo, goInfo->spellcaster.spellId, 0);
                break;
            }
            case GAMEOBJECT_TYPE_FLAGSTAND:                 // 24
            {
                if (goInfo->flagstand.lockId)
                {
                    CheckGOLockId(goInfo, goInfo->flagstand.lockId, 0);
                }
                CheckGONoDamageImmuneId(goInfo, goInfo->flagstand.noDamageImmune, 5);
                break;
            }
            case GAMEOBJECT_TYPE_FISHINGHOLE:               // 25
            {
                if (goInfo->fishinghole.lockId)
                {
                    CheckGOLockId(goInfo, goInfo->fishinghole.lockId, 4);
                }
                break;
            }
            case GAMEOBJECT_TYPE_FLAGDROP:                  // 26
            {
                if (goInfo->flagdrop.lockId)
                {
                    CheckGOLockId(goInfo, goInfo->flagdrop.lockId, 0);
                }
                CheckGONoDamageImmuneId(goInfo, goInfo->flagdrop.noDamageImmune, 3);
                break;
            }
            case GAMEOBJECT_TYPE_CAPTURE_POINT:             // 29
            {
                CheckAndFixGOCaptureMinTime(goInfo, goInfo->capturePoint.minTime, 16);
                break;
            }
        }
    }

    sLog.outString(">> Loaded %u game object templates", sGOStorage.GetRecordCount());
    sLog.outString();
}
