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
#include "DisableMgr.h"
#include "CinematicFlyover.h"
#include <cmath>
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */
#ifdef ENABLE_PLAYERBOTS
#include "playerbot.h"
#endif /* ENABLE_PLAYERBOTS */

/**
 * @brief Sends the player's initial action bar state to the client.
 */
void Player::SendInitialActionButtons() const
{
    DETAIL_LOG("Initializing Action Buttons for '%u'", GetGUIDLow());

    /* Initiate packet with size 4 bytes per action button */
    WorldPacket data(SMSG_ACTION_BUTTONS, (MAX_ACTION_BUTTONS * 4));

    /* For each possible action button the player could have */
    for (uint8 button = 0; button < MAX_ACTION_BUTTONS; ++button)
    {
        /* Try and get each action button the player could have */
        ActionButtonList::const_iterator itr = m_actionButtons.find(button);

        /* If the button is valid and not deleted */
        if (itr != m_actionButtons.end() && itr->second.uState != ACTIONBUTTON_DELETED)
        {
            /* Send the data */
            data << uint32(itr->second.packedData);
        }
        else
        {
            /* Nothing to send, so just send 0 */
            data << uint32(0);
        }
    }

    GetSession()->SendPacket(&data);
    DETAIL_LOG("Action Buttons for '%u' Initialized", GetGUIDLow());
}

/**
 * @brief Validates action bar data before it is stored or loaded.
 *
 * @param button The action bar button index.
 * @param action The action identifier assigned to the button.
 * @param type The action button type.
 * @param player The player being validated for, or null for template data.
 * @return True if the action button data is valid; otherwise, false.
 */
bool Player::IsActionButtonDataValid(uint8 button, uint32 action, uint8 type, Player* player)
{
    if (button >= MAX_ACTION_BUTTONS)
    {
        if (player)
        {
            sLog.outError("Action %u not added into button %u for player %s: button must be < %u", action, button, player->GetName(), MAX_ACTION_BUTTONS);
        }
        else
        {
            sLog.outError("Table `playercreateinfo_action` have action %u into button %u : button must be < %u", action, button, MAX_ACTION_BUTTONS);
        }
        return false;
    }

    if (action >= MAX_ACTION_BUTTON_ACTION_VALUE)
    {
        if (player)
        {
            sLog.outError("Action %u not added into button %u for player %s: action must be < %u", action, button, player->GetName(), MAX_ACTION_BUTTON_ACTION_VALUE);
        }
        else
        {
            sLog.outError("Table `playercreateinfo_action` have action %u into button %u : action must be < %u", action, button, MAX_ACTION_BUTTON_ACTION_VALUE);
        }
        return false;
    }

    switch (type)
    {
        case ACTION_BUTTON_SPELL:
        {
            SpellEntry const* spellProto = sSpellStore.LookupEntry(action);
            if (!spellProto)
            {
                if (player)
                {
                    sLog.outError("Spell action %u not added into button %u for player %s: spell not exist", action, button, player->GetName());
                }
                else
                {
                    sLog.outError("Table `playercreateinfo_action` have spell action %u into button %u: spell not exist", action, button);
                }
                return false;
            }

            if (player)
            {
                if (!player->HasSpell(spellProto->Id))
                {
                    sLog.outError("Spell action %u not added into button %u for player %s: player don't known this spell", action, button, player->GetName());
                    return false;
                }
                else if (IsPassiveSpell(spellProto))
                {
                    sLog.outError("Spell action %u not added into button %u for player %s: spell is passive", action, button, player->GetName());
                    return false;
                }
            }
            break;
        }
        case ACTION_BUTTON_ITEM:
        {
            if (!ObjectMgr::GetItemPrototype(action))
            {
                if (player)
                {
                    sLog.outError("Item action %u not added into button %u for player %s: item not exist", action, button, player->GetName());
                }
                else
                {
                    sLog.outError("Table `playercreateinfo_action` have item action %u into button %u: item not exist", action, button);
                }
                return false;
            }
            break;
        }
        default:
            break;                                          // other cases not checked at this moment
    }

    return true;
}

/**
 * @brief Adds or updates an action bar button entry.
 *
 * @param button The action bar button index.
 * @param action The action identifier to bind.
 * @param type The action button type.
 * @return The updated action button, or null if validation fails.
 */
ActionButton* Player::addActionButton(uint8 button, uint32 action, uint8 type)
{
    if (!IsActionButtonDataValid(button, action, type, this))
    {
        return NULL;
    }

    // it create new button (NEW state) if need or return existing
    ActionButton& ab = m_actionButtons[button];

    // set data and update to CHANGED if not NEW
    ab.SetActionAndType(action, ActionButtonType(type));

    DETAIL_LOG("Player '%u' Added Action '%u' (type %u) to Button '%u'", GetGUIDLow(), action, uint32(type), button);
    return &ab;
}

/**
 * @brief Removes an action bar button entry.
 *
 * @param button The action bar button index to remove.
 */
void Player::removeActionButton(uint8 button)
{
    ActionButtonList::iterator buttonItr = m_actionButtons.find(button);
    if (buttonItr == m_actionButtons.end())
    {
        return;
    }

    if (buttonItr->second.uState == ACTIONBUTTON_NEW)
    {
        m_actionButtons.erase(buttonItr); // new and not saved
    }
    else
    {
        buttonItr->second.uState = ACTIONBUTTON_DELETED; // saved, will deleted at next save
    }

    DETAIL_LOG("Action Button '%u' Removed from Player '%u'", button, GetGUIDLow());
}
