/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2019  MaNGOS project <https://getmangos.eu>
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

#ifndef DBCSTORE_H
#define DBCSTORE_H

#include "DBCFileLoader.h"

template<class T>
/**
 * @brief
 *
 */
class DBCStorage
{
        /**
         * @brief
         *
         */
        typedef std::list<char*> StringPoolList;
    public:
        /**
         * @brief
         *
         * @param f
         */
        explicit DBCStorage(const char* f) : nCount(0), fieldCount(0), fmt(f), indexTable(NULL), m_dataTable(NULL) { }
        /**
         * @brief
         *
         */
        ~DBCStorage() { Clear(); }

        /**
        * @brief
        *
        * @return uint32
        */
        //uint32  GetNumRows() const { return nCount; }
        uint32  GetNumRows() const { return loaded ? data.size() : nCount; }
        /**
         * @brief
         *
         * @return const char
         */
        char const* GetFormat() const { return fmt; }
        /**
         * @brief
         *
         * @return uint32
         */
        uint32 GetFieldCount() const { return fieldCount; }

        /**
        * @brief
        *
        * @param id
        * @return const T
        */
        //T const* LookupEntry(uint32 id) const { return (id >= nCount) ? NULL : indexTable[id]; }
        T const* LookupEntry(uint32 id) const
        {
            if (loaded)
            {
                typename std::map<uint32, T const*>::const_iterator it = data.find(id);
                if (it != data.end())
                    return it->second;
            }
            return (id >= nCount) ? NULL : indexTable[id];
        }
        /**
         * @brief
         *
         * @param fn
         * @return bool
         */
        bool Load(char const* fn)
        {
            DBCFileLoader dbc;
            // Check if load was sucessful, only then continue
            if (!dbc.Load(fn, fmt))
                { return false; }

            fieldCount = dbc.GetCols();

            // load raw non-string data
            m_dataTable = (T*)dbc.AutoProduceData(fmt, nCount, (char**&)indexTable);

            // load strings from dbc data
            m_stringPoolList.push_back(dbc.AutoProduceStrings(fmt, (char*)m_dataTable));

            // error in dbc file at loading if NULL
            return indexTable != NULL;
        }

        void SetEntry(uint32 id, T* t) // Cryptic they say..
        {
            if (!loaded)
            {
                for (uint32 i = 0; i < nCount; ++i)
                {
                    T const* node = LookupEntry(i);
                    if (!node)
                        continue;
                    data[i] = node;
                }
                loaded = true;
            }
            data[id] = t;
        }

        /**
         * @brief
         *
         * @param fn
         * @return bool
         */
        bool LoadStringsFrom(char const* fn)
        {
            // DBC must be already loaded using Load
            if (!indexTable)
                { return false; }

            DBCFileLoader dbc;
            // Check if load was successful, only then continue
            if (!dbc.Load(fn, fmt))
                { return false; }

            // load strings from another locale dbc data
            m_stringPoolList.push_back(dbc.AutoProduceStrings(fmt, (char*)m_dataTable));

            return true;
        }

        /**
         * @brief
         *
         */
        void Clear()
        {
            if (loaded)
            {
                data.clear();
                loaded = false;
            }

            if (!indexTable)
                { return; }

            delete[]((char*)indexTable);
            indexTable = NULL;
            delete[]((char*)m_dataTable);
            m_dataTable = NULL;

            while (!m_stringPoolList.empty())
            {
                delete[] m_stringPoolList.front();
                m_stringPoolList.pop_front();
            }
            nCount = 0;
        }

        /**
         * @brief
         *
         * @param id
         */
        void EraseEntry(uint32 id) { assert(id < nCount && "Entry to be erased must be in bounds!") ; indexTable[id] = NULL; }
        /**
         * @brief
         *
         * @param entry
         * @param id
         */
        void InsertEntry(T* entry, uint32 id) { assert(id < nCount && "Entry to be inserted must be in bounds!"); indexTable[id] = entry; }

    private:
        uint32 nCount; /**< TODO */
        uint32 fieldCount; /**< TODO */
        char const* fmt; /**< TODO */
        T** indexTable; /**< TODO */
        T* m_dataTable; /**< TODO */
        std::map<uint32, T const*> data;
        bool loaded;
        StringPoolList m_stringPoolList; /**< TODO */
};

#endif
