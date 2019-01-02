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

#ifndef MANGOS_H_BYTEBUFFER
#define MANGOS_H_BYTEBUFFER

#include "Common/Common.h"
#include "Utilities/ByteConverter.h"
#include "Utilities/Errors.h"

/**
 * @brief
 *
 */
class ByteBufferException
{
    public:
    /**
     * @brief
     *
     * @param _add
     * @param _pos
     * @param _esize
     * @param _size
     */
        ByteBufferException(bool _add, size_t _pos, size_t _esize, size_t _size)
            : add(_add), pos(_pos), esize(_esize), size(_size)
        {
            PrintPosError();
        }

        /**
         * @brief
         *
         */
        void PrintPosError() const;
    private:
        bool add; /**< TODO */
        size_t pos; /**< TODO */
        size_t esize; /**< TODO */
        size_t size; /**< TODO */
};

template<class T>
/**
 * @brief
 *
 */
struct Unused
{
/**
 * @brief
 *
 */
    Unused() {}
};

/**
 * @brief
 *
 */
class ByteBuffer
{
    public:
        const static size_t DEFAULT_SIZE = 0x1000; /**< TODO */

        /**
         * @brief constructor
         *
         */
        ByteBuffer(): _rpos(0), _wpos(0)
        {
            _storage.reserve(DEFAULT_SIZE);
        }

        /**
         * @brief constructor
         *
         * @param res
         */
        ByteBuffer(size_t res): _rpos(0), _wpos(0)
        {
            _storage.reserve(res);
        }

        /**
         * @brief copy constructor
         *
         * @param buf
         */
        ByteBuffer(const ByteBuffer& buf): _rpos(buf._rpos), _wpos(buf._wpos), _storage(buf._storage) { }

        /**
         * @brief
         *
         */
        void clear()
        {
            _storage.clear();
            _rpos = _wpos = 0;
        }

        /**
         * @brief
         *
         * @param pos
         * @param value
         */
        template <typename T> void put(size_t pos, T value)
        {
            EndianConvert(value);
            put(pos, (uint8*)&value, sizeof(value));
        }

        /**
         * @brief
         *
         * @param value
         * @return ByteBuffer &operator
         */
        ByteBuffer& operator<<(uint8 value)
        {
            append<uint8>(value);
            return *this;
        }

        /**
         * @brief
         *
         * @param value
         * @return ByteBuffer &operator
         */
        ByteBuffer& operator<<(uint16 value)
        {
            append<uint16>(value);
            return *this;
        }

        /**
         * @brief
         *
         * @param value
         * @return ByteBuffer &operator
         */
        ByteBuffer& operator<<(uint32 value)
        {
            append<uint32>(value);
            return *this;
        }

        /**
         * @brief
         *
         * @param value
         * @return ByteBuffer &operator
         */
        ByteBuffer& operator<<(uint64 value)
        {
            append<uint64>(value);
            return *this;
        }

        /**
         * @brief signed as in 2e complement
         *
         * @param value
         * @return ByteBuffer &operator
         */
        ByteBuffer& operator<<(int8 value)
        {
            append<int8>(value);
            return *this;
        }

        /**
         * @brief
         *
         * @param value
         * @return ByteBuffer &operator
         */
        ByteBuffer& operator<<(int16 value)
        {
            append<int16>(value);
            return *this;
        }

        /**
         * @brief
         *
         * @param value
         * @return ByteBuffer &operator
         */
        ByteBuffer& operator<<(int32 value)
        {
            append<int32>(value);
            return *this;
        }

        /**
         * @brief
         *
         * @param value
         * @return ByteBuffer &operator
         */
        ByteBuffer& operator<<(int64 value)
        {
            append<int64>(value);
            return *this;
        }

        /**
         * @brief floating points
         *
         * @param value
         * @return ByteBuffer &operator
         */
        ByteBuffer& operator<<(float value)
        {
            append<float>(value);
            return *this;
        }

        /**
         * @brief
         *
         * @param value
         * @return ByteBuffer &operator
         */
        ByteBuffer& operator<<(double value)
        {
            append<double>(value);
            return *this;
        }

        /**
         * @brief
         *
         * @param value
         * @return ByteBuffer &operator
         */
        ByteBuffer& operator<<(const std::string& value)
        {
            append((uint8 const*)value.c_str(), value.length());
            append((uint8)0);
            return *this;
        }

        /**
         * @brief
         *
         * @param str
         * @return ByteBuffer &operator
         */
        ByteBuffer& operator<<(const char* str)
        {
            append((uint8 const*)str, str ? strlen(str) : 0);
            append((uint8)0);
            return *this;
        }

        /**
         * @brief
         *
         * @param value
         * @return ByteBuffer &operator >>
         */
        ByteBuffer& operator>>(bool& value)
        {
            value = read<char>() > 0 ? true : false;
            return *this;
        }

        /**
         * @brief
         *
         * @param value
         * @return ByteBuffer &operator >>
         */
        ByteBuffer& operator>>(uint8& value)
        {
            value = read<uint8>();
            return *this;
        }

        /**
         * @brief
         *
         * @param value
         * @return ByteBuffer &operator >>
         */
        ByteBuffer& operator>>(uint16& value)
        {
            value = read<uint16>();
            return *this;
        }

        /**
         * @brief
         *
         * @param value
         * @return ByteBuffer &operator >>
         */
        ByteBuffer& operator>>(uint32& value)
        {
            value = read<uint32>();
            return *this;
        }

        /**
         * @brief
         *
         * @param value
         * @return ByteBuffer &operator >>
         */
        ByteBuffer& operator>>(uint64& value)
        {
            value = read<uint64>();
            return *this;
        }

        /**
         * @brief signed as in 2e complement
         *
         * @param value
         * @return ByteBuffer &operator >>
         */
        ByteBuffer& operator>>(int8& value)
        {
            value = read<int8>();
            return *this;
        }

        /**
         * @brief
         *
         * @param value
         * @return ByteBuffer &operator >>
         */
        ByteBuffer& operator>>(int16& value)
        {
            value = read<int16>();
            return *this;
        }

        /**
         * @brief
         *
         * @param value
         * @return ByteBuffer &operator >>
         */
        ByteBuffer& operator>>(int32& value)
        {
            value = read<int32>();
            return *this;
        }

        /**
         * @brief
         *
         * @param value
         * @return ByteBuffer &operator >>
         */
        ByteBuffer& operator>>(int64& value)
        {
            value = read<int64>();
            return *this;
        }

        /**
         * @brief
         *
         * @param value
         * @return ByteBuffer &operator >>
         */
        ByteBuffer& operator>>(float& value)
        {
            value = read<float>();
            return *this;
        }

        /**
         * @brief
         *
         * @param value
         * @return ByteBuffer &operator >>
         */
        ByteBuffer& operator>>(double& value)
        {
            value = read<double>();
            return *this;
        }

        /**
         * @brief
         *
         * @param value
         * @return ByteBuffer &operator >>
         */
        ByteBuffer& operator>>(std::string& value)
        {
            value.clear();
            while (rpos() < size())                         // prevent crash at wrong string format in packet
            {
                char c = read<char>();
                if (c == 0)
                    { break; }
                value += c;
            }
            return *this;
        }

        template<class T>
        /**
         * @brief
         *
         * @param
         * @return ByteBuffer &operator >>
         */
        ByteBuffer& operator>>(Unused<T> const&)
        {
            read_skip<T>();
            return *this;
        }


        /**
         * @brief
         *
         * @param pos
         * @return uint8 operator
         */
        uint8 operator[](size_t pos) const
        {
            return read<uint8>(pos);
        }

        /**
         * @brief
         *
         * @return size_t
         */
        size_t rpos() const { return _rpos; }

        /**
         * @brief
         *
         * @param rpos_
         * @return size_t
         */
        size_t rpos(size_t rpos_)
        {
            _rpos = rpos_;
            return _rpos;
        }

        /**
         * @brief
         *
         * @return size_t
         */
        size_t wpos() const { return _wpos; }

        /**
         * @brief
         *
         * @param wpos_
         * @return size_t
         */
        size_t wpos(size_t wpos_)
        {
            _wpos = wpos_;
            return _wpos;
        }

        template<typename T>
        /**
         * @brief
         *
         */
        void read_skip() { read_skip(sizeof(T)); }

        /**
         * @brief
         *
         * @param skip
         */
        void read_skip(size_t skip)
        {
            if (_rpos + skip > size())
                { throw ByteBufferException(false, _rpos, skip, size()); }
            _rpos += skip;
        }

        /**
         * @brief
         *
         * @return T
         */
        template <typename T> T read()
        {
            T r = read<T>(_rpos);
            _rpos += sizeof(T);
            return r;
        }

        /**
         * @brief
         *
         * @param pos
         * @return T
         */
        template <typename T> T read(size_t pos) const
        {
            if (pos + sizeof(T) > size())
                { throw ByteBufferException(false, pos, sizeof(T), size()); }
            T val = *((T const*)&_storage[pos]);
            EndianConvert(val);
            return val;
        }

        /**
         * @brief
         *
         * @param dest
         * @param len
         */
        void read(uint8* dest, size_t len)
        {
            if (_rpos  + len > size())
                { throw ByteBufferException(false, _rpos, len, size()); }
            memcpy(dest, &_storage[_rpos], len);
            _rpos += len;
        }

        /**
         * @brief
         *
         * @return uint64
         */
        uint64 readPackGUID()
        {
            uint64 guid = 0;
            uint8 guidmark = 0;
            (*this) >> guidmark;

            for (int i = 0; i < 8; ++i)
            {
                if (guidmark & (uint8(1) << i))
                {
                    uint8 bit;
                    (*this) >> bit;
                    guid |= (uint64(bit) << (i * 8));
                }
            }

            return guid;
        }

        /**
         * @brief
         *
         * @return const uint8
         */
        const uint8* contents() const { return &_storage[0]; }

        /**
         * @brief
         *
         * @return size_t
         */
        size_t size() const { return _storage.size(); }
        /**
         * @brief
         *
         * @return bool
         */
        bool empty() const { return _storage.empty(); }

        /**
         * @brief
         *
         * @param newsize
         */
        void resize(size_t newsize)
        {
            _storage.resize(newsize);
            _rpos = 0;
            _wpos = size();
        }

        /**
         * @brief
         *
         * @param ressize
         */
        void reserve(size_t ressize)
        {
            if (ressize > size())
                { _storage.reserve(ressize); }
        }

        /**
         * @brief
         *
         * @param str
         */
        void append(const std::string& str)
        {
            append((uint8 const*)str.c_str(), str.size() + 1);
        }

        /**
         * @brief
         *
         * @param src
         * @param cnt
         */
        void append(const char* src, size_t cnt)
        {
            return append((const uint8*)src, cnt);
        }

        /**
         * @brief
         *
         * @param src
         * @param cnt
         */
        template<class T> void append(const T* src, size_t cnt)
        {
            return append((const uint8*)src, cnt * sizeof(T));
        }

        /**
         * @brief
         *
         * @param src
         * @param cnt
         */
        void append(const uint8* src, size_t cnt)
        {
            if (!cnt)
                { return; }

            MANGOS_ASSERT(size() < 10000000);

            if (_storage.size() < _wpos + cnt)
                { _storage.resize(_wpos + cnt); }
            memcpy(&_storage[_wpos], src, cnt);
            _wpos += cnt;
        }

        /**
         * @brief
         *
         * @param buffer
         */
        void append(const ByteBuffer& buffer)
        {
            if (buffer.wpos())
                { append(buffer.contents(), buffer.wpos()); }
        }

        /**
         * @brief can be used in SMSG_MONSTER_MOVE opcode
         *
         * @param x
         * @param y
         * @param z
         */
        void appendPackXYZ(float x, float y, float z)
        {
            uint32 packed = 0;
            packed |= ((int)(x / 0.25f) & 0x7FF);
            packed |= ((int)(y / 0.25f) & 0x7FF) << 11;
            packed |= ((int)(z / 0.25f) & 0x3FF) << 22;
            *this << packed;
        }

        /**
         * @brief
         *
         * @param guid
         */
        void appendPackGUID(uint64 guid)
        {
            uint8 packGUID[8 + 1];
            packGUID[0] = 0;
            size_t size = 1;
            for (uint8 i = 0; guid != 0; ++i)
            {
                if (guid & 0xFF)
                {
                    packGUID[0] |= uint8(1 << i);
                    packGUID[size] =  uint8(guid & 0xFF);
                    ++size;
                }

                guid >>= 8;
            }

            append(packGUID, size);
        }

        /**
         * @brief
         *
         * @param pos
         * @param src
         * @param cnt
         */
        void put(size_t pos, const uint8* src, size_t cnt)
        {
            if (pos + cnt > size())
                { throw ByteBufferException(true, pos, cnt, size()); }
            memcpy(&_storage[pos], src, cnt);
        }

        /**
         * @brief
         *
         */
        void print_storage() const;
        /**
         * @brief
         *
         */
        void textlike() const;
        /**
         * @brief
         *
         */
        void hexlike() const;

    private:
        /**
         * @brief limited for internal use because can "append" any unexpected type (like pointer and etc) with hard detection problem
         *
         * @param value
         */
        template <typename T> void append(T value)
        {
            EndianConvert(value);
            append((uint8*)&value, sizeof(value));
        }

    protected:
        size_t _rpos, _wpos; /**< TODO */
        std::vector<uint8> _storage; /**< TODO */
};

template <typename T>
/**
 * @brief
 *
 * @param b
 * @param v
 * @return ByteBuffer &operator
 */
inline ByteBuffer& operator<<(ByteBuffer& b, std::vector<T> const& v)
{
    b << (uint32)v.size();
    for (typename std::vector<T>::iterator i = v.begin(); i != v.end(); ++i)
    {
        b << *i;
    }
    return b;
}

template <typename T>
/**
 * @brief
 *
 * @param b
 * @param v
 * @return ByteBuffer &operator >>
 */
inline ByteBuffer& operator>>(ByteBuffer& b, std::vector<T>& v)
{
    uint32 vsize;
    b >> vsize;
    v.clear();
    while (vsize--)
    {
        T t;
        b >> t;
        v.push_back(t);
    }
    return b;
}

template <typename T>
/**
 * @brief
 *
 * @param b
 * @param v
 * @return ByteBuffer &operator
 */
inline ByteBuffer& operator<<(ByteBuffer& b, std::list<T> const& v)
{
    b << (uint32)v.size();
    for (typename std::list<T>::iterator i = v.begin(); i != v.end(); ++i)
    {
        b << *i;
    }
    return b;
}

template <typename T>
/**
 * @brief
 *
 * @param b
 * @param v
 * @return ByteBuffer &operator >>
 */
inline ByteBuffer& operator>>(ByteBuffer& b, std::list<T>& v)
{
    uint32 vsize;
    b >> vsize;
    v.clear();
    while (vsize--)
    {
        T t;
        b >> t;
        v.push_back(t);
    }
    return b;
}

template <typename K, typename V>
/**
 * @brief
 *
 * @param b
 * @param std::map<K
 * @param m
 * @return ByteBuffer &operator
 */
inline ByteBuffer& operator<<(ByteBuffer& b, std::map<K, V>& m)
{
    b << (uint32)m.size();
    for (typename std::map<K, V>::iterator i = m.begin(); i != m.end(); ++i)
    {
        b << i->first << i->second;
    }
    return b;
}

template <typename K, typename V>
/**
 * @brief
 *
 * @param b
 * @param std::map<K
 * @param m
 * @return ByteBuffer &operator >>
 */
inline ByteBuffer& operator>>(ByteBuffer& b, std::map<K, V>& m)
{
    uint32 msize;
    b >> msize;
    m.clear();
    while (msize--)
    {
        K k;
        V v;
        b >> k >> v;
        m.insert(make_pair(k, v));
    }
    return b;
}

template<>
/**
 * @brief
 *
 */
inline void ByteBuffer::read_skip<char*>()
{
    std::string temp;
    *this >> temp;
}

template<>
/**
 * @brief
 *
 */
inline void ByteBuffer::read_skip<char const*>()
{
    read_skip<char*>();
}

template<>
/**
 * @brief
 *
 */
inline void ByteBuffer::read_skip<std::string>()
{
    read_skip<char*>();
}
#endif
