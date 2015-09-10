/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2015  MaNGOS project <http://getmangos.eu>
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

#ifndef VMAPEXPORT_H
#define VMAPEXPORT_H

#include <string>
#include <set>

/**
 * @brief
 *
 */
typedef std::set<std::string> StringSet;

/**
 * @brief
 *
 */
enum ModelFlags
{
    MOD_M2 = 1,
    MOD_WORLDSPAWN = 1 << 1,
    MOD_HAS_BOUND = 1 << 2
};

extern const char* szWorkDirWmo; /**< TODO */
extern const char* szRawVMAPMagic; /**< vmap magic string for extracted raw vmap data */

/**
 * @brief Test if the specified file exists in the building directory
 *
 * @param file
 * @return bool
 */
bool FileExists(const char* file);

/**
 * @brief Get "uniform" name for a path (a uniform name has the format <md5hash>-<filename>.<ext>)
 *
 * @param path
 * @return string
 */
std::string GetUniformName(std::string& path);

/**
 * @brief Get extension for a file
 *
 * @param file
 * @return extension, if found, or std::string::npos if not
 */
std::string GetExtension(std::string& file);

#endif
