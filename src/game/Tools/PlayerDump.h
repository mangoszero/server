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

#ifndef MANGOS_H_PLAYER_DUMP
#define MANGOS_H_PLAYER_DUMP

#include <set>

/**
 * @brief Dump table type enumeration
 */
enum DumpTableType
{
    DTT_CHARACTER,    ///< Character data (guid, name) -> characters

    DTT_CHAR_TABLE,   ///< Character table data (character_action, character_aura, character_homebind, character_queststatus, character_reputation, character_spell, character_spell_cooldown, character_ticket, character_tutorial)
                      ///< character_queststatus, character_reputation,
                      ///< character_spell, character_spell_cooldown, character_ticket,
                      ///< character_tutorial

    DTT_INVENTORY,    ///< Inventory data (item guids collection) -> character_inventory

    DTT_MAIL,         ///< Mail data (mail ids collection) -> mail, item_text
                      ///<    -> item_text

    DTT_MAIL_ITEM,    ///< Mail item data (mail ids) -> mail_items, item guids collection
                      ///<    -> item guids collection

    DTT_ITEM,         ///< Item data (item guids) -> item_instance, item_text
                      ///<    -> item_text

    DTT_ITEM_GIFT,    ///< Item gift data (item guids) -> character_gifts

    DTT_ITEM_LOOT,    ///< Item loot data (item guids) -> item_loot

    DTT_PET,          ///< Pet data (pet guids collection) -> character_pet
    DTT_PET_TABLE,    ///< Pet table data (pet guids) -> pet_aura, pet_spell, pet_spell_cooldown
};

/**
 * @brief Dump return enumeration
 */
enum DumpReturn
{
    DUMP_SUCCESS,            ///< Success
    DUMP_FILE_OPEN_ERROR,    ///< File open error
    DUMP_TOO_MANY_CHARS,     ///< Too many characters
    DUMP_UNEXPECTED_END,     ///< Unexpected end of file
    DUMP_FILE_BROKEN,        ///< File broken
    DUMP_DB_VERSION_MISMATCH ///< Database version mismatch
};

/**
 * @brief Player dump base class
 */
class PlayerDump
{
    protected:
        /**
         * @brief Constructor
         */
        PlayerDump() {}
};

/**
 * @brief Player dump writer class
 */
class PlayerDumpWriter : public PlayerDump
{
    public:
        /**
         * @brief Constructor
         */
        PlayerDumpWriter() {}

        /**
         * @brief Get dump string
         * @param guid Player GUID
         * @return Dump string
         */
        std::string GetDump(uint32 guid);

        /**
         * @brief Write dump to file
         * @param file File path
         * @param guid Player GUID
         * @return Dump return code
         */
        DumpReturn WriteDump(const std::string& file, uint32 guid);

    private:
        typedef std::set<uint32> GUIDs;

        /**
         * @brief Dump table content
         * @param dump Dump string output
         * @param guid Player GUID
         * @param tableFrom Source table
         * @param tableTo Destination table
         * @param type Dump table type
         */
        void DumpTableContent(std::string& dump, uint32 guid, char const* tableFrom, char const* tableTo, DumpTableType type);

        /**
         * @brief Generate WHERE string for GUIDs
         * @param field Field name
         * @param guids GUIDs set
         * @param itr Iterator
         * @return WHERE string
         */
        std::string GenerateWhereStr(char const* field, GUIDs const& guids, GUIDs::const_iterator& itr);

        /**
         * @brief Generate WHERE string for single GUID
         * @param field Field name
         * @param guid GUID
         * @return WHERE string
         */
        std::string GenerateWhereStr(char const* field, uint32 guid);

        GUIDs pets; ///< Pet GUIDs
        GUIDs mails; ///< Mail GUIDs
        GUIDs items; ///< Item GUIDs
};

/**
 * @brief Player dump reader class
 */
class PlayerDumpReader : public PlayerDump
{
    public:
        /**
         * @brief Constructor
         */
        PlayerDumpReader() {}

        /**
         * @brief Load dump from file
         * @param file File path
         * @param account Account ID
         * @param name Character name
         * @param guid Player GUID
         * @return Dump return code
         */
        DumpReturn LoadDump(const std::string& file, uint32 account, std::string name, uint32 guid);
};

#endif
