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

#ifndef SQLSTORAGE_H
#define SQLSTORAGE_H

#include "Common/Common.h"
#include "Database/DatabaseEnv.h"
#include "DataStores/DBCFileLoader.h"

/**
 * @brief
 *
 */
class SQLStorageBase
{
        template<class DerivedLoader, class StorageClass> friend class SQLStorageLoaderBase;

    public:
        /**
         * @brief
         *
         * @return const char
         */
        char const* GetTableName() const { return m_tableName; }
        /**
         * @brief
         *
         * @return const char
         */
        char const* EntryFieldName() const { return m_entry_field; }

        /**
         * @brief
         *
         * @param idx
         * @return FieldFormat
         */
        FieldFormat GetDstFormat(uint32 idx) const { return (FieldFormat)m_dst_format[idx]; }
        /**
         * @brief
         *
         * @return const char
         */
        const char* GetDstFormat() const { return m_dst_format; }
        /**
         * @brief
         *
         * @param idx
         * @return FieldFormat
         */
        FieldFormat GetSrcFormat(uint32 idx) const { return (FieldFormat)m_src_format[idx]; }
        /**
         * @brief
         *
         * @return const char
         */
        const char* GetSrcFormat() const { return m_src_format; }

        /**
         * @brief
         *
         * @return uint32
         */
        uint32 GetMaxEntry() const { return m_maxEntry; }
        /**
         * @brief
         *
         * @return uint32
         */
        uint32 GetRecordCount() const { return m_recordCount; }

        template<typename T>
        /**
         * @brief
         *
         */
        class SQLSIterator
        {
                friend class SQLStorageBase;

            public:
                /**
                 * @brief
                 *
                 * @return const T
                 */
                T const* getValue() const { return reinterpret_cast<T const*>(pointer); }

                /**
                 * @brief
                 *
                 */
                void operator ++() { pointer += recordSize; }
                /**
                 * @brief
                 *
                 * @return const T *operator
                 */
                T const* operator *() const { return getValue(); }
                /**
                 * @brief
                 *
                 * @return const T *operator ->
                 */
                T const* operator ->() const { return getValue(); }
                /**
                 * @brief
                 *
                 * @param r
                 * @return bool operator
                 */
                bool operator <(const SQLSIterator& r) const { return pointer < r.pointer; }
                /**
                 * @brief
                 *
                 * @param r
                 */
                void operator =(const SQLSIterator& r) { pointer = r.pointer; recordSize = r.recordSize; }

            private:
                /**
                 * @brief
                 *
                 * @param ptr
                 * @param _recordSize
                 */
                SQLSIterator(char* ptr, uint32 _recordSize) : pointer(ptr), recordSize(_recordSize) {}
                char* pointer; /**< TODO */
                uint32 recordSize; /**< TODO */
        };

        template<typename T>
        /**
         * @brief
         *
         * @return SQLSIterator<T>
         */
        SQLSIterator<T> getDataBegin() const { return SQLSIterator<T>(m_data, m_recordSize); }
        template<typename T>
        /**
         * @brief
         *
         * @return SQLSIterator<T>
         */
        SQLSIterator<T> getDataEnd() const { return SQLSIterator<T>(m_data + m_recordCount * m_recordSize, m_recordSize); }

    protected:
        /**
         * @brief
         *
         */
        SQLStorageBase();
        /**
         * @brief
         *
         */
        virtual ~SQLStorageBase() { Free(); }

        /**
         * @brief
         *
         * @param tableName
         * @param entry_field
         * @param src_format
         * @param dst_format
         */
        void Initialize(const char* tableName, const char* entry_field, const char* src_format, const char* dst_format);

        /**
         * @brief
         *
         * @return uint32
         */
        uint32 GetDstFieldCount() const { return m_dstFieldCount; }
        /**
         * @brief
         *
         * @return uint32
         */
        uint32 GetSrcFieldCount() const { return m_srcFieldCount; }
        /**
         * @brief
         *
         * @return uint32
         */
        uint32 GetRecordSize() const { return m_recordSize; }

        /**
         * @brief
         *
         * @param maxRecordId
         * @param recordCount
         * @param recordSize
         */
        virtual void prepareToLoad(uint32 maxRecordId, uint32 recordCount, uint32 recordSize);
        /**
         * @brief
         *
         * @param recordId
         * @param record
         */
        virtual void JustCreatedRecord(uint32 recordId, char* record) = 0;
        /**
         * @brief
         *
         */
        virtual void Free();

    private:
        /**
         * @brief
         *
         * @param recordId
         * @return char
         */
        char* createRecord(uint32 recordId);

        // Information about the table
        const char* m_tableName; /**< TODO */
        const char* m_entry_field; /**< TODO */
        const char* m_src_format; /**< TODO */
        const char* m_dst_format; /**< TODO */

        // Information about the records
        uint32 m_dstFieldCount; /**< TODO */
        uint32 m_srcFieldCount; /**< TODO */
        uint32 m_recordCount; /**< TODO */
        uint32 m_maxEntry; /**< TODO */
        uint32 m_recordSize; /**< TODO */

        // Data Storage
        char* m_data; /**< TODO */
};

/**
 * @brief
 *
 */
class SQLStorage : public SQLStorageBase
{
        template<class DerivedLoader, class StorageClass> friend class SQLStorageLoaderBase;

    public:
        /**
         * @brief
         *
         * @param fmt
         * @param _entry_field
         * @param sqlname
         */
        SQLStorage(const char* fmt, const char* _entry_field, const char* sqlname);

        /**
         * @brief
         *
         * @param src_fmt
         * @param dst_fmt
         * @param _entry_field
         * @param sqlname
         */
        SQLStorage(const char* src_fmt, const char* dst_fmt, const char* _entry_field, const char* sqlname);

        /**
         * @brief
         *
         */
        ~SQLStorage() { Free(); }

        template<class T>
        /**
         * @brief
         *
         * @param id
         * @return const T
         */
        T const* LookupEntry(uint32 id) const
        {
            if (id >= GetMaxEntry())
                { return NULL; }
            return reinterpret_cast<T const*>(m_Index[id]);
        }

        /**
         * @brief
         *
         * @param error_at_empty
         */
        void Load(bool error_at_empty = true);

        /**
         * @brief
         *
         * @param id
         */
        void EraseEntry(uint32 id);

    protected:
        /**
         * @brief
         *
         * @param maxRecordId
         * @param recordCount
         * @param recordSize
         */
        void prepareToLoad(uint32 maxRecordId, uint32 recordCount, uint32 recordSize) override;
        /**
         * @brief
         *
         * @param recordId
         * @param record
         */
        void JustCreatedRecord(uint32 recordId, char* record) override
        {
            m_Index[recordId] = record;
        }

        /**
         * @brief
         *
         */
        void Free() override;

    private:
        char** m_Index; /**< Lookup access */
};

/**
 * @brief
 *
 */
class SQLHashStorage : public SQLStorageBase
{
        template<class DerivedLoader, class StorageClass> friend class SQLStorageLoaderBase;

    public:
        /**
         * @brief
         *
         * @param fmt
         * @param _entry_field
         * @param sqlname
         */
        SQLHashStorage(const char* fmt, const char* _entry_field, const char* sqlname);
        /**
         * @brief
         *
         * @param src_fmt
         * @param dst_fmt
         * @param _entry_field
         * @param sqlname
         */
        SQLHashStorage(const char* src_fmt, const char* dst_fmt, const char* _entry_field, const char* sqlname);

        /**
         * @brief
         *
         */
        ~SQLHashStorage() { Free(); }

        template<class T>
        /**
         * @brief
         *
         * @param id
         * @return const T
         */
        T const* LookupEntry(uint32 id) const
        {
            RecordMap::const_iterator find = m_indexMap.find(id);
            if (find != m_indexMap.end())
                { return reinterpret_cast<T const*>(find->second); }
            return NULL;
        }

        /**
         * @brief
         *
         */
        void Load();

        /**
         * @brief
         *
         * @param id
         */
        void EraseEntry(uint32 id);

    protected:
        /**
         * @brief
         *
         * @param maxRecordId
         * @param recordCount
         * @param recordSize
         */
        void prepareToLoad(uint32 maxRecordId, uint32 recordCount, uint32 recordSize) override;
        /**
         * @brief
         *
         * @param recordId
         * @param record
         */
        void JustCreatedRecord(uint32 recordId, char* record) override
        {
            m_indexMap[recordId] = record;
        }

        /**
         * @brief
         *
         */
        void Free() override;

    private:
        /**
         * @brief
         *
         */
        typedef UNORDERED_MAP < uint32 /*recordId*/, char* /*record*/ > RecordMap;
        RecordMap m_indexMap; /**< TODO */
};

/**
 * @brief
 *
 */
class SQLMultiStorage : public SQLStorageBase
{
        template<class DerivedLoader, class StorageClass> friend class SQLStorageLoaderBase;
        template<typename T> friend class SQLMultiSIterator;
        template<typename T> friend class SQLMSIteratorBounds;

    private:
        /**
         * @brief
         *
         */
        typedef std::multimap < uint32 /*recordId*/, char* /*record*/ > RecordMultiMap;

    public:
        /**
         * @brief
         *
         * @param fmt
         * @param _entry_field
         * @param sqlname
         */
        SQLMultiStorage(const char* fmt, const char* _entry_field, const char* sqlname);
        /**
         * @brief
         *
         * @param src_fmt
         * @param dst_fmt
         * @param _entry_field
         * @param sqlname
         */
        SQLMultiStorage(const char* src_fmt, const char* dst_fmt, const char* _entry_field, const char* sqlname);

        /**
         * @brief
         *
         */
        ~SQLMultiStorage() { Free(); }

        // forward declaration
        template<typename T> class SQLMSIteratorBounds;

        template<typename T>
        /**
         * @brief
         *
         */
        class SQLMultiSIterator
        {
                friend class SQLMultiStorage;
                friend class SQLMSIteratorBounds<T>;

            public:
                /**
                 * @brief
                 *
                 * @return const T
                 */
                T const* getValue() const { return reinterpret_cast<T const*>(citerator->second); }
                /**
                 * @brief
                 *
                 * @return uint32
                 */
                uint32 getKey() const { return citerator->first; }

                /**
                 * @brief
                 *
                 */
                void operator ++() { ++citerator; }
                /**
                 * @brief
                 *
                 * @return const T *operator
                 */
                T const* operator *() const { return getValue(); }
                /**
                 * @brief
                 *
                 * @return const T *operator ->
                 */
                T const* operator ->() const { return getValue(); }
                /**
                 * @brief
                 *
                 * @param r
                 * @return bool operator
                 */
                bool operator !=(const SQLMultiSIterator& r) const { return citerator != r.citerator; }
                /**
                 * @brief
                 *
                 * @param r
                 * @return bool operator
                 */
                bool operator ==(const SQLMultiSIterator& r) const { return citerator == r.citerator; }

            private:
                /**
                 * @brief
                 *
                 * @param _itr
                 */
                SQLMultiSIterator(RecordMultiMap::const_iterator _itr) : citerator(_itr) {}
                RecordMultiMap::const_iterator citerator; /**< TODO */
        };

        template<typename T>
        /**
         * @brief
         *
         */
        class SQLMSIteratorBounds
        {
                friend class SQLMultiStorage;

            public:
                const SQLMultiSIterator<T> first; /**< TODO */
                const SQLMultiSIterator<T> second; /**< TODO */

            private:
                /**
                 * @brief
                 *
                 * @param std::pair<RecordMultiMap::const_iterator
                 * @param pair
                 */
                SQLMSIteratorBounds(std::pair<RecordMultiMap::const_iterator, RecordMultiMap::const_iterator> pair) : first(pair.first), second(pair.second) {}
        };

        template<typename T>
        /**
         * @brief
         *
         * @param key
         * @return SQLMSIteratorBounds<T>
         */
        SQLMSIteratorBounds<T> getBounds(uint32 key) const { return SQLMSIteratorBounds<T>(m_indexMultiMap.equal_range(key)); }

        /**
         * @brief
         *
         */
        void Load();

        /**
         * @brief
         *
         * @param id
         */
        void EraseEntry(uint32 id);

    protected:
        /**
         * @brief
         *
         * @param maxRecordId
         * @param recordCount
         * @param recordSize
         */
        void prepareToLoad(uint32 maxRecordId, uint32 recordCount, uint32 recordSize) override;
        /**
         * @brief
         *
         * @param recordId
         * @param record
         */
        void JustCreatedRecord(uint32 recordId, char* record) override
        {
            m_indexMultiMap.insert(RecordMultiMap::value_type(recordId, record));
        }

        /**
         * @brief
         *
         */
        void Free() override;

    private:
        RecordMultiMap m_indexMultiMap; /**< TODO */
};

template <class DerivedLoader, class StorageClass>
/**
 * @brief
 *
 */
class SQLStorageLoaderBase
{
    public:
        /**
         * @brief
         *
         * @param storage
         * @param error_at_empty
         */
        void Load(StorageClass& storage, bool error_at_empty = true);

        template<class S, class D>
        /**
         * @brief
         *
         * @param field_pos
         * @param src
         * @param dst
         */
        void convert(uint32 field_pos, S src, D& dst);
        template<class S>
        /**
         * @brief
         *
         * @param field_pos
         * @param src
         * @param dst
         */
        void convert_to_str(uint32 field_pos, S src, char*& dst);
        template<class D>
        /**
         * @brief
         *
         * @param field_pos
         * @param src
         * @param dst
         */
        void convert_from_str(uint32 field_pos, char const* src, D& dst);
        /**
         * @brief
         *
         * @param field_pos
         * @param src
         * @param dst
         */
        void convert_str_to_str(uint32 field_pos, char const* src, char*& dst);
        template<class S, class D>
        /**
         * @brief
         *
         * @param field_pos
         * @param src
         * @param dst
         */
        void default_fill(uint32 field_pos, S src, D& dst);
        /**
         * @brief
         *
         * @param field_pos
         * @param src
         * @param dst
         */
        void default_fill_to_str(uint32 field_pos, char const* src, char*& dst);

        template<class D>
        /**
         * @brief trap, no body
         *
         * @param field_pos
         * @param src
         * @param dst
         */
        void convert_from_str(uint32 field_pos, char* src, D& dst);
        /**
         * @brief
         *
         * @param field_pos
         * @param src
         * @param dst
         */
        void convert_str_to_str(uint32 field_pos, char* src, char*& dst);

    private:
        template<class V>
        /**
         * @brief
         *
         * @param value
         * @param store
         * @param record
         * @param field_pos
         * @param offset
         */
        void storeValue(V value, StorageClass& store, char* record, uint32 field_pos, uint32& offset);
        /**
         * @brief
         *
         * @param value
         * @param store
         * @param record
         * @param field_pos
         * @param offset
         */
        void storeValue(char const* value, StorageClass& store, char* record, uint32 field_pos, uint32& offset);

        /**
         * @brief trap, no body
         *
         * @param value
         * @param store
         * @param record
         * @param field_pos
         * @param offset
         */
        void storeValue(char* value, StorageClass& store, char* record, uint32 field_pos, uint32& offset);
};

/**
 * @brief
 *
 */
class SQLStorageLoader : public SQLStorageLoaderBase<SQLStorageLoader, SQLStorage>
{
};

/**
 * @brief
 *
 */
class SQLHashStorageLoader : public SQLStorageLoaderBase<SQLHashStorageLoader, SQLHashStorage>
{
};

/**
 * @brief
 *
 */
class SQLMultiStorageLoader : public SQLStorageLoaderBase<SQLMultiStorageLoader, SQLMultiStorage>
{
};

#include "SQLStorageImpl.h"

#endif
