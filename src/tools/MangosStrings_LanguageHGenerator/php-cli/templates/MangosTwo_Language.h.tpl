/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2022 MaNGOS <https://getmangos.eu>
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

#ifndef MANGOS_H_MANGOS_LANGUAGE
#define MANGOS_H_MANGOS_LANGUAGE

enum MangosStrings
{
{CONTENT}
        // FREE IDS                           1701-9999
        // Use for not-in-official-sources patches
        //                                    10000-10999

        // Use for custom patches             11000-11999

        // NOT RESERVED IDS                   12000-1999999999
        // `db_script_string` table index     2000000000-2000999999 (MIN_DB_SCRIPT_STRING_ID-MAX_DB_SCRIPT_STRING_ID)
        // For other tables maybe             2001000000-2147483647 (max index)
};
#endif
