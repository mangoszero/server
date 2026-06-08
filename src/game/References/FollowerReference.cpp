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
 * @file FollowerReference.cpp
 * @brief Follower reference link management for unit following
 *
 * This file implements the FollowerReference class which manages the
 * bidirectional link between a following unit (source) and its target.
 *
 * When a unit follows another (e.g., pets, escort NPCs, charmed units),
 * a FollowerReference is created to maintain this relationship and handle
 * proper cleanup when either unit is destroyed.
 *
 * The reference ensures:
 * - Target knows who is following it
 * - Follower stops when target is destroyed
 * - Proper cleanup of movement generators
 *
 * @see FollowerReference for the reference class
 * @see FollowerRefManager for the manager
 */

#include "Unit.h"
#include "TargetedMovementGenerator.h"
#include "FollowerReference.h"

/**
 * @brief Called when link is established to target
 *
 * Registers this reference with the target unit, allowing the target
 * to know who is following it.
 */
void FollowerReference::targetObjectBuildLink()
{
    getTarget()->AddFollower(this);
}

/**
 * @brief Called when target is being destroyed
 *
 * Removes this reference from the target's follower list before
 * the target is destroyed.
 */
void FollowerReference::targetObjectDestroyLink()
{
    getTarget()->RemoveFollower(this);
}

/**
 * @brief Called when source (follower) is being destroyed
 *
 * Stops the following behavior on the source unit when the
 * follower is being destroyed.
 */
void FollowerReference::sourceObjectDestroyLink()
{
    getSource()->stopFollowing();
}
