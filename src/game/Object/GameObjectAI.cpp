/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of the CMaNGOS Project. See AUTHORS file for Copyright information
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
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "GameObjectAI.h"
#include "GameObject.h"

/**
 * @brief Creates a scripted AI wrapper for a game object.
 *
 * @param go The game object controlled by this AI.
 */
GameObjectAI::GameObjectAI(GameObject* go) : m_go(go)
{
}

/**
 * @brief Destroys the game object AI instance.
 */
GameObjectAI::~GameObjectAI()
{
}
