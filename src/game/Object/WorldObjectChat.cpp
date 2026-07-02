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

/**
 * @file Object.cpp
 * @brief Base implementation for all game objects
 *
 * This file implements the Object class, which is the base class for all
 * entities in the game world. It provides:
 * - Update field management (synchronized with clients)
 * - Object GUID handling
 * - Update data building for network transmission
 * - Object visibility and spawning
 * - Type identification
 *
 * The Object class uses an array of uint32 values (update fields) that
 * mirror the client's object state. Changes to these values are sent to
 * players who can see the object.
 */



#include "Object.h"
#include "SharedDefines.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "Log.h"
#include "World.h"
#include "Creature.h"
#include "Player.h"
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "UpdateData.h"
#include "UpdateMask.h"
#include "Util.h"
#include "MapManager.h"
#include "Transports.h"
#include "TargetedMovementGenerator.h"
#include "WaypointMovementGenerator.h"
#include "VMapFactory.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "ObjectPosSelector.h"
#include "TemporarySummon.h"
#include "movement/packet_builder.h"
#include "CreatureLinkingMgr.h"
#include "Chat.h"
#include "GameTime.h"
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */
#ifdef ENABLE_ELUNA
#include "ElunaConfig.h"
#endif /* ENABLE_ELUNA */
#ifdef ENABLE_ELUNA
#include "ElunaEventMgr.h"
#endif /* ENABLE_ELUNA */

/**
 * @brief Send monster say message
 * @param text Text to say
 * @param language Language (unused)
 * @param target Target unit
 *
 * Sends a monster say message to nearby players.
 */
void WorldObject::MonsterSay(const char* text, uint32 /*language*/, Unit const* target) const
{
    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_MONSTER_SAY, text, LANG_UNIVERSAL, CHAT_TAG_NONE, GetObjectGuid(), GetName(),
        target ? target->GetObjectGuid() : ObjectGuid(), target ? target->GetName() : "");
    SendMessageToSetInRange(&data, sWorld.getConfig(CONFIG_FLOAT_LISTEN_RANGE_SAY), true);
}

/**
 * @brief Send monster yell message
 * @param text Text to yell
 * @param language Language (unused)
 * @param target Target unit
 *
 * Sends a monster yell message to nearby players.
 */
void WorldObject::MonsterYell(const char* text, uint32 /*language*/, Unit const* target) const
{
    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_MONSTER_YELL, text, LANG_UNIVERSAL, CHAT_TAG_NONE, GetObjectGuid(), GetName(),
        target ? target->GetObjectGuid() : ObjectGuid(), target ? target->GetName() : "");
    SendMessageToSetInRange(&data, sWorld.getConfig(CONFIG_FLOAT_LISTEN_RANGE_YELL), true);
}

/**
 * @brief Send monster text emote message
 * @param text Text to emote
 * @param target Target unit
 * @param IsBossEmote If true, use boss emote range
 *
 * Sends a monster text emote message to nearby players.
 */
void WorldObject::MonsterTextEmote(const char* text, Unit const* target, bool IsBossEmote) const
{
    WorldPacket data;
    ChatHandler::BuildChatPacket(data, IsBossEmote ? CHAT_MSG_RAID_BOSS_EMOTE : CHAT_MSG_MONSTER_EMOTE, text, LANG_UNIVERSAL, CHAT_TAG_NONE, GetObjectGuid(), GetName(),
        target ? target->GetObjectGuid() : ObjectGuid(), target ? target->GetName() : "");
    SendMessageToSetInRange(&data, sWorld.getConfig(IsBossEmote ? CONFIG_FLOAT_LISTEN_RANGE_YELL : CONFIG_FLOAT_LISTEN_RANGE_TEXTEMOTE), true);
}

/**
 * @brief Send monster whisper message
 * @param text Text to whisper
 * @param target Target unit
 * @param IsBossWhisper If true, use boss whisper
 *
 * Sends a monster whisper message to the target player.
 */
void WorldObject::MonsterWhisper(const char* text, Unit const* target, bool IsBossWhisper) const
{
    if (!target || target->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }

    WorldPacket data;
    ChatHandler::BuildChatPacket(data, IsBossWhisper ? CHAT_MSG_RAID_BOSS_WHISPER : CHAT_MSG_MONSTER_WHISPER, text, LANG_UNIVERSAL, CHAT_TAG_NONE, GetObjectGuid(), GetName(),
        target->GetObjectGuid(), target->GetName());
    ((Player*)target)->GetSession()->SendPacket(&data);
}

namespace MaNGOS
{

    /**
     * @brief Monster chat builder functor
     *
     * Builds localized chat packets for monster messages.
     */
    class MonsterChatBuilder
    {
        public:
            /**
             * @brief Constructor
             * @param obj Source object
             * @param msgtype Chat message type
             * @param textData Localized text data
             * @param language Language
             * @param target Target unit
             */
            MonsterChatBuilder(WorldObject const& obj, ChatMsg msgtype, MangosStringLocale const* textData, Language language, Unit const* target)
                : i_object(obj), i_msgtype(msgtype), i_textData(textData), i_language(language), i_target(target) {}

            /**
             * @brief Build chat packet
             * @param data Packet to build
             * @param loc_idx Locale index
             */
            void operator()(WorldPacket& data, int32 loc_idx)
            {
                char const* text = NULL;
                if ((int32)i_textData->Content.size() > loc_idx + 1 && !i_textData->Content[loc_idx + 1].empty())
                {
                    text = i_textData->Content[loc_idx + 1].c_str();
                }
                else
                {
                    text = i_textData->Content[0].c_str();
                }

                ChatHandler::BuildChatPacket(data, i_msgtype, text, i_language, CHAT_TAG_NONE, i_object.GetObjectGuid(), i_object.GetNameForLocaleIdx(loc_idx),
                    i_target ? i_target->GetObjectGuid() : ObjectGuid(), i_target ? i_target->GetNameForLocaleIdx(loc_idx) : "");
            }

        private:
            WorldObject const& i_object; ///< Source object
            ChatMsg i_msgtype; ///< Chat message type
            MangosStringLocale const* i_textData; ///< Localized text data
            Language i_language; ///< Language
            Unit const* i_target; ///< Target unit
    };
}                                                           // namespace MaNGOS

/**
 * @brief Sends localized monster chat packets to players around a source object.
 *
 * @param source The source world object.
 * @param textData The localized text definition.
 * @param msgtype The chat packet type.
 * @param language The chat language.
 * @param target Optional target unit.
 * @param range Broadcast range.
 */
void _DoLocalizedTextAround(WorldObject const* source, MangosStringLocale const* textData, ChatMsg msgtype, Language language, Unit const* target, float range)
{
    MaNGOS::MonsterChatBuilder say_build(*source, msgtype, textData, language, target);
    MaNGOS::LocalizedPacketDo<MaNGOS::MonsterChatBuilder> say_do(say_build);
    MaNGOS::CameraDistWorker<MaNGOS::LocalizedPacketDo<MaNGOS::MonsterChatBuilder> > say_worker(source, range, say_do);
    Cell::VisitWorldObjects(source, say_worker, range);
}

/**
 * @brief Sends a localized monster text defined by a Mangos string entry.
 *
 * @param textData The localized text definition.
 * @param target Optional target unit.
 */
void WorldObject::MonsterText(MangosStringLocale const* textData, Unit const* target) const
{
    MANGOS_ASSERT(textData);

    switch (textData->Type)
    {
        case CHAT_TYPE_SAY:
            _DoLocalizedTextAround(this, textData, CHAT_MSG_MONSTER_SAY, textData->LanguageId, target, sWorld.getConfig(CONFIG_FLOAT_LISTEN_RANGE_SAY));
            break;
        case CHAT_TYPE_YELL:
            _DoLocalizedTextAround(this, textData, CHAT_MSG_MONSTER_YELL, textData->LanguageId, target, sWorld.getConfig(CONFIG_FLOAT_LISTEN_RANGE_YELL));
            break;
        case CHAT_TYPE_TEXT_EMOTE:
            _DoLocalizedTextAround(this, textData, CHAT_MSG_MONSTER_EMOTE, LANG_UNIVERSAL, target, sWorld.getConfig(CONFIG_FLOAT_LISTEN_RANGE_TEXTEMOTE));
            break;
        case CHAT_TYPE_BOSS_EMOTE:
            _DoLocalizedTextAround(this, textData, CHAT_MSG_RAID_BOSS_EMOTE, LANG_UNIVERSAL, target, sWorld.getConfig(CONFIG_FLOAT_LISTEN_RANGE_YELL));
            break;
        case CHAT_TYPE_WHISPER:
        {
            if (!target || target->GetTypeId() != TYPEID_PLAYER)
            {
                return;
            }
            MaNGOS::MonsterChatBuilder say_build(*this, CHAT_MSG_MONSTER_WHISPER, textData, LANG_UNIVERSAL, target);
            MaNGOS::LocalizedPacketDo<MaNGOS::MonsterChatBuilder> say_do(say_build);
            say_do((Player*)target);
            break;
        }
        case CHAT_TYPE_BOSS_WHISPER:
        {
            if (!target || target->GetTypeId() != TYPEID_PLAYER)
            {
                return;
            }
            MaNGOS::MonsterChatBuilder say_build(*this, CHAT_MSG_RAID_BOSS_WHISPER, textData, LANG_UNIVERSAL, target);
            MaNGOS::LocalizedPacketDo<MaNGOS::MonsterChatBuilder> say_do(say_build);
            say_do((Player*)target);
            break;
        }
        case CHAT_TYPE_ZONE_YELL:
        {
            MaNGOS::MonsterChatBuilder say_build(*this, CHAT_MSG_MONSTER_YELL, textData, textData->LanguageId, target);
            MaNGOS::LocalizedPacketDo<MaNGOS::MonsterChatBuilder> say_do(say_build);
            uint32 zoneid = GetZoneId();
            Map::PlayerList const& pList = GetMap()->GetPlayers();
            for (Map::PlayerList::const_iterator itr = pList.begin(); itr != pList.end(); ++itr)
            {
                if (itr->getSource()->GetZoneId() == zoneid)
                {
                    say_do(itr->getSource());
                }
            }
            break;
        }
    }
}

/**
 * @brief Broadcasts a packet to all players in the object's visibility set.
 *
 * @param data The packet to send.
 * @param bToSelf Unused self-delivery flag.
 */
void WorldObject::SendMessageToSet(WorldPacket* data, bool /*bToSelf*/) const
{
    // if object is in world, map for it already created!
    if (IsInWorld())
    {
        GetMap()->MessageBroadcast(this, data);
    }
}

/**
 * @brief Broadcasts a packet to players within a specified range.
 *
 * @param data The packet to send.
 * @param dist The broadcast distance.
 * @param bToSelf Unused self-delivery flag.
 */
void WorldObject::SendMessageToSetInRange(WorldPacket* data, float dist, bool /*bToSelf*/) const
{
    // if object is in world, map for it already created!
    if (IsInWorld())
    {
        GetMap()->MessageDistBroadcast(this, data, dist);
    }
}

/**
 * @brief Broadcasts a packet to visible players except one receiver.
 *
 * @param data The packet to send.
 * @param skipped_receiver The player to exclude.
 */
void WorldObject::SendMessageToSetExcept(WorldPacket* data, Player const* skipped_receiver) const
{
    // if object is in world, map for it already created!
    if (IsInWorld())
    {
        MaNGOS::MessageDelivererExcept notifier(data, skipped_receiver);
        Cell::VisitWorldObjects(this, notifier, GetMap()->GetBroadcastRadius());
    }
}

/**
 * @brief Sends a gameobject despawn animation packet.
 *
 * @param guid The GUID of the object being animated.
 */
void WorldObject::SendObjectDeSpawnAnim(ObjectGuid guid)
{
    WorldPacket data(SMSG_GAMEOBJECT_DESPAWN_ANIM, 8);
    data << ObjectGuid(guid);
    SendMessageToSet(&data, true);
}
