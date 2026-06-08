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
 * @file VMapFactory.cpp
 * @brief VMap (Virtual Map) system factory and configuration
 *
 * This file implements the VMapFactory which provides:
 * - Singleton access to the VMapManager
 * - Line-of-Sight (LoS) spell filtering configuration
 * - Utility functions for string parsing
 *
 * VMaps are used for:
 * - Line-of-sight calculations
 * - Height/floor determination
 * - Model collision detection
 *
 * The system uses pre-processed map data files (.vmtree, .vmtile) for
 * efficient spatial queries.
 *
 * @see VMapFactory for the factory class
 * @see VMapManager2 for the implementation
 * @see IVMapManager for the interface
 */

#include "VMapFactory.h"
#include "VMapManager2.h"

using namespace G3D;

namespace VMAP
{

    /**
     * @brief Trim whitespace and quotes from string ends
     * @param str String to modify in-place
     *
     * Removes trailing and leading whitespace, carriage returns,
     * newlines, and quote characters from the string.
     */
    void VMapFactory::chompAndTrim(std::string& str)
    {
        while (str.length() > 0)
        {
            char lc = str[str.length() - 1];
            if (lc == '\r' || lc == '\n' || lc == ' ' || lc == '"' || lc == '\'')
            {
                str = str.substr(0, str.length() - 1);
            }
            else
            {
                break;
            }
        }
        while (str.length() > 0)
        {
            char lc = str[0];
            if (lc == ' ' || lc == '"' || lc == '\'')
            {
                str = str.substr(1, str.length() - 1);
            }
            else
            {
                break;
            }
        }
    }

    /**
     * @var gVMapManager
     * @brief Global singleton VMapManager instance
     */
    IVMapManager* gVMapManager = 0;

    /**
     * @var iIgnoreSpellIds
     * @brief Table of spell IDs to ignore for LoS checks
     */
    Table<unsigned int , bool>* iIgnoreSpellIds = 0;

    /**
     * @brief Parse next ID from comma-separated string
     * @param pString Source string containing comma-separated IDs
     * @param pStartPos Current position in string (updated on success)
     * @param pId Output parameter for parsed ID
     * @return true if ID was found, false at end of string
     */
    bool VMapFactory::getNextId(const std::string& pString, unsigned int& pStartPos, unsigned int& pId)
    {
        bool result = false;
        unsigned int i;
        for (i = pStartPos; i < pString.size(); ++i)
        {
            if (pString[i] == ',')
            {
                break;
            }
        }
        if (i > pStartPos)
        {
            std::string idString = pString.substr(pStartPos, i - pStartPos);
            pStartPos = i + 1;
            chompAndTrim(idString);
            pId = atoi(idString.c_str());
            result = true;
        }
        return(result);
    }

    /**
     * @brief Configure spells to skip LoS testing
     * @param pSpellIdString Comma-separated list of spell IDs
     *
     * Parses a comma-separated string of spell IDs and adds them to
     * the ignore list. These spells will bypass line-of-sight checks.
     *
     * @code
     * // Example config string: "133,475,8923"
     * VMapFactory::preventSpellsFromBeingTestedForLoS("133,475,8923");
     * @endcode
     */
    void VMapFactory::preventSpellsFromBeingTestedForLoS(const char* pSpellIdString)
    {
        if (!iIgnoreSpellIds)
        {
            iIgnoreSpellIds = new Table<unsigned int , bool>();
        }
        if (pSpellIdString != NULL)
        {
            unsigned int pos = 0;
            unsigned int id;
            std::string confString(pSpellIdString);
            chompAndTrim(confString);
            while (getNextId(confString, pos, id))
            {
                iIgnoreSpellIds->set(id, true);
            }
        }
    }

    /**
     * @brief Check if spell should test LoS
     * @param pSpellId Spell ID to check
     * @return true if spell should check LoS, false if ignored
     *
     * Returns true if the spell is NOT in the ignore list,
     * meaning it should perform line-of-sight checks.
     */
    bool VMapFactory::checkSpellForLoS(unsigned int pSpellId)
    {
        return(!iIgnoreSpellIds->containsKey(pSpellId));
    }

    /**
     * @brief Create or retrieve the VMapManager singleton
     * @return Pointer to the VMapManager instance
     *
     * Creates the VMapManager on first call. Subsequent calls return
     * the existing instance.
     */
    IVMapManager* VMapFactory::createOrGetVMapManager()
    {
        if (gVMapManager == 0)
        {
            gVMapManager = new VMapManager2();               // should be taken from config ... Please change if you like :-)
        }
        return gVMapManager;
    }

    /**
     * @brief Clean up all VMap resources
     *
     * Destroys the VMapManager and spell ignore table.
     * Should be called during server shutdown to free memory.
     */
    void VMapFactory::clear()
    {
        delete iIgnoreSpellIds;
        delete gVMapManager;

        iIgnoreSpellIds = NULL;
        gVMapManager = NULL;
    }
}
