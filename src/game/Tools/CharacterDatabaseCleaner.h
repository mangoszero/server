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

#ifndef CHARACTERDATABASECLEANER_H
#define CHARACTERDATABASECLEANER_H

namespace CharacterDatabaseCleaner
{

    /**
     * @brief Cleaning flags enumeration
     */
    enum CleaningFlags
    {
//      Reserved for next version = 0x1,
        CLEANING_FLAG_SKILLS = 0x2,      ///< Clean skills
        CLEANING_FLAG_SPELLS = 0x4,      ///< Clean spells
//      Reserved for next version = 0x8
    };

    /**
     * @brief Clean the character database
     */
    void CleanDatabase();

    /**
     * @brief Check unique values
     * @param column Column name
     * @param table Table name
     * @param check Check function
     */
    void CheckUnique(const char* column, const char* table, bool (*check)(uint32));

    /**
     * @brief Check skill validity
     * @param skill Skill ID
     * @return True if valid
     */
    bool SkillCheck(uint32 skill);

    /**
     * @brief Check spell validity
     * @param spell_id Spell ID
     * @return True if valid
     */
    bool SpellCheck(uint32 spell_id);

    /**
     * @brief Clean character skills
     */
    void CleanCharacterSkills();

    /**
     * @brief Clean character spells
     */
    void CleanCharacterSpell();
}

#endif
