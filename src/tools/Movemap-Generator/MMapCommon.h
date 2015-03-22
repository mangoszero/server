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

#ifndef MANGOS_H_MMAP_COMMON
#define MANGOS_H_MMAP_COMMON

// stop warning spam from ACE includes
#ifdef WIN32
#  pragma warning( disable : 4996 )
#endif

#include <string>
#include <vector>

#include "Platform/Define.h"

#ifndef WIN32
#include <stddef.h>
#include <dirent.h>
#endif

using namespace std;

namespace MMAP
{
    /**
     * @brief
     *
     * @param filter
     * @param str
     * @return bool
     */
    inline bool matchWildcardFilter(const char* filter, const char* str)
    {
        if (!filter || !str)
            { return false; }

        // end on null character
        while (*filter && *str)
        {
            if (*filter == '*')
            {
                if (*++filter == '\0')  // wildcard at end of filter means all remaing chars match
                    { return true; }

                while (true)
                {
                    if (*filter == *str)
                        { break; }
                    if (*str == '\0')
                        { return false; }   // reached end of string without matching next filter character
                    str++;
                }
            }
            else if (*filter != *str)
                { return false; }           // mismatch

            filter++;
            str++;
        }

        return ((*filter == '\0' || (*filter == '*' && *++filter == '\0')) && *str == '\0');
    }

    /**
     * @brief
     *
     */
    enum ListFilesResult
    {
        LISTFILE_DIRECTORY_NOT_FOUND = 0,
        LISTFILE_OK = 1
    };

    /**
     * @brief
     *
     * @param fileList
     * @param dirpath
     * @param filter
     * @param includeSubDirs
     * @return ListFilesResult
     */
    inline ListFilesResult getDirContents(vector<string>& fileList, string dirpath = ".", string filter = "*", bool includeSubDirs = false)
    {
#ifdef WIN32
        HANDLE hFind;
        WIN32_FIND_DATA findFileInfo;
        string directory;

        directory = dirpath + "/" + filter;

        hFind = FindFirstFile(directory.c_str(), &findFileInfo);

        if (hFind == INVALID_HANDLE_VALUE)
            { return LISTFILE_DIRECTORY_NOT_FOUND; }

        do
        {
            if (includeSubDirs || (findFileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
                { fileList.push_back(string(findFileInfo.cFileName)); }
        }
        while (FindNextFile(hFind, &findFileInfo));

        FindClose(hFind);

#else
        const char* p = dirpath.c_str();
        DIR* dirp = opendir(p);
        struct dirent* dp;

        while (dirp)
        {
            errno = 0;
            if ((dp = readdir(dirp)) != NULL)
            {
                if (matchWildcardFilter(filter.c_str(), dp->d_name))
                    { fileList.push_back(string(dp->d_name)); }
            }
            else
                { break; }
        }

        if (dirp)
            { closedir(dirp); }
        else
            { return LISTFILE_DIRECTORY_NOT_FOUND; }
#endif

        return LISTFILE_OK;
    }
}

#endif
