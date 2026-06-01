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
 * @file GroupReference.cpp
 * @brief Group membership reference link management
 *
 * This file implements the GroupReference class which manages the
 * bidirectional link between a player (source) and their group (target).
 *
 * Each player in a group has a GroupReference that maintains the connection
 * to the Group object. This reference pattern allows:
 * - Player to know which group they belong to
 * - Group to track all its members
 * - Automatic cleanup when player or group is destroyed
 *
 * The reference is created when a player joins a group and destroyed
 * when they leave or either object is deleted.
 *
 * @see GroupReference for the reference class
 * @see GroupRefManager for the manager
 * @see Group for the group container
 */

#include "Group.h"
#include "GroupReference.h"

/**
 * @brief Called when link is established to target (group)
 *
 * Called from link(). Registers this player reference with the group
 * so the group can track its members.
 */
void GroupReference::targetObjectBuildLink()
{
    getTarget()->LinkMember(this);
}

/**
 * @brief Called when target (group) is being destroyed
 *
 * Called from unlink(). Removes this player reference from the group
 * before the group is destroyed.
 */
void GroupReference::targetObjectDestroyLink()
{
    getTarget()->DelinkMember(this);
}

/**
 * @brief Called when source (player) is being destroyed
 *
 * Called from invalidate(). Removes this player from the group
 * when the player object is being destroyed.
 */
void GroupReference::sourceObjectDestroyLink()
{
    getTarget()->DelinkMember(this);
}
