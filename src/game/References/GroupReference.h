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

#ifndef MANGOS_H_GROUPREFERENCE
#define MANGOS_H_GROUPREFERENCE

#include "Utilities/LinkedReference/Reference.h"

class Group;
class Player;

/**
 * @brief Group reference class
 *
 * Manages the reference between a Group and a Player.
 */
class GroupReference : public Reference<Group, Player>
{
    protected:
        uint8 iSubGroup; ///< Sub-group ID

        /**
         * @brief Build link to target object
         */
        void targetObjectBuildLink() override;

        /**
         * @brief Destroy link to target object
         */
        void targetObjectDestroyLink() override;

        /**
         * @brief Destroy link from source object
         */
        void sourceObjectDestroyLink() override;

    public:
        /**
         * @brief Constructor
         */
        GroupReference() : Reference<Group, Player>(), iSubGroup(0) {}

        /**
         * @brief Destructor
         */
        ~GroupReference() { unlink(); }

        /**
         * @brief Get next reference
         * @return Next group reference
         */
        GroupReference* next() { return (GroupReference*)Reference<Group, Player>::next(); }

        /**
         * @brief Get next reference (const)
         * @return Next group reference (const)
         */
        GroupReference const* next() const { return (GroupReference const*)Reference<Group, Player>::next(); }

        /**
         * @brief Get sub-group ID
         * @return Sub-group ID
         */
        uint8 getSubGroup() const { return iSubGroup; }

        /**
         * @brief Set sub-group ID
         * @param pSubGroup Sub-group ID to set
         */
        void setSubGroup(uint8 pSubGroup) { iSubGroup = pSubGroup; }
};
#endif
