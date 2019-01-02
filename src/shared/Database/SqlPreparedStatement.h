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

#ifndef SQLPREPAREDSTATEMENTS_H
#define SQLPREPAREDSTATEMENTS_H

#include "Common/Common.h"
#include "Utilities/Errors.h"
#include <ace/TSS_T.h>
#include <vector>
#include <stdexcept>

class Database;
class SqlConnection;
class QueryResult;

/**
 * @brief
 *
 */
union SqlStmtField
{
    bool boolean; /**< TODO */
    uint8 ui8; /**< TODO */
    int8 i8; /**< TODO */
    uint16 ui16; /**< TODO */
    int16 i16; /**< TODO */
    uint32 ui32; /**< TODO */
    int32 i32; /**< TODO */
    uint64 ui64; /**< TODO */
    int64 i64; /**< TODO */
    float f; /**< TODO */
    double d; /**< TODO */
};

/**
 * @brief
 *
 */
enum SqlStmtFieldType
{
    FIELD_BOOL,
    FIELD_UI8,
    FIELD_UI16,
    FIELD_UI32,
    FIELD_UI64,
    FIELD_I8,
    FIELD_I16,
    FIELD_I32,
    FIELD_I64,
    FIELD_FLOAT,
    FIELD_DOUBLE,
    FIELD_STRING,
    FIELD_NONE
};

/**
 * @brief templates might be the best choice here
 *
 */
class SqlStmtFieldData
{
    public:
        /**
         * @brief
         *
         */
        SqlStmtFieldData() : m_type(FIELD_NONE) { m_binaryData.ui64 = 0; }
        /**
         * @brief
         *
         */
        ~SqlStmtFieldData() {}

        template<typename T>
        /**
         * @brief
         *
         * @param param
         */
        SqlStmtFieldData(T param) { set(param); }

        template<typename T1>
        /**
         * @brief
         *
         * @param param1
         */
        void set(T1 param1);

        /**
         * @brief getter
         *
         * @return bool
         */
        bool toBool() const { MANGOS_ASSERT(m_type == FIELD_BOOL); return static_cast<bool>(m_binaryData.ui8); }
        /**
         * @brief getter
         *
         * @return uint8
         */
        uint8 toUint8() const { MANGOS_ASSERT(m_type == FIELD_UI8); return m_binaryData.ui8; }
        /**
         * @brief getter
         *
         * @return int8
         */
        int8 toInt8() const { MANGOS_ASSERT(m_type == FIELD_I8); return m_binaryData.i8; }
        /**
         * @brief getter
         *
         * @return uint16
         */
        uint16 toUint16() const { MANGOS_ASSERT(m_type == FIELD_UI16); return m_binaryData.ui16; }
        /**
         * @brief getter
         *
         * @return int16
         */
        int16 toInt16() const { MANGOS_ASSERT(m_type == FIELD_I16); return m_binaryData.i16; }
        /**
         * @brief getter
         *
         * @return uint32
         */
        uint32 toUint32() const { MANGOS_ASSERT(m_type == FIELD_UI32); return m_binaryData.ui32; }
        /**
         * @brief getter
         *
         * @return int32
         */
        int32 toInt32() const { MANGOS_ASSERT(m_type == FIELD_I32); return m_binaryData.i32; }
        /**
         * @brief getter
         *
         * @return uint64
         */
        uint64 toUint64() const { MANGOS_ASSERT(m_type == FIELD_UI64); return m_binaryData.ui64; }
        /**
         * @brief getter
         *
         * @return int64
         */
        int64 toInt64() const { MANGOS_ASSERT(m_type == FIELD_I64); return m_binaryData.i64; }
        /**
         * @brief getter
         *
         * @return float
         */
        float toFloat() const { MANGOS_ASSERT(m_type == FIELD_FLOAT); return m_binaryData.f; }
        /**
         * @brief getter
         *
         * @return double
         */
        double toDouble() const { MANGOS_ASSERT(m_type == FIELD_DOUBLE); return m_binaryData.d; }
        /**
         * @brief getter
         *
         * @return const char
         */
        const char* toStr() const { MANGOS_ASSERT(m_type == FIELD_STRING); return m_szStringData.c_str(); }

        /**
         * @brief get type of data
         *
         * @return SqlStmtFieldType
         */
        SqlStmtFieldType type() const { return m_type; }
        /**
         * @brief get underlying buffer type
         *
         */
        void* buff() const { return m_type == FIELD_STRING ? (void*)m_szStringData.c_str() : (void*)&m_binaryData; }

        /**
         * @brief get size of data
         *
         * @return size_t
         */
        size_t size() const
        {
            switch (m_type)
            {
                case FIELD_NONE:    return 0;
                case FIELD_BOOL:    // return sizeof(bool);
                case FIELD_UI8:     return sizeof(uint8);
                case FIELD_UI16:    return sizeof(uint16);
                case FIELD_UI32:    return sizeof(uint32);
                case FIELD_UI64:    return sizeof(uint64);
                case FIELD_I8:      return sizeof(int8);
                case FIELD_I16:     return sizeof(int16);
                case FIELD_I32:     return sizeof(int32);
                case FIELD_I64:     return sizeof(int64);
                case FIELD_FLOAT:   return sizeof(float);
                case FIELD_DOUBLE:  return sizeof(double);
                case FIELD_STRING:  return m_szStringData.length();

                default:
                    throw std::runtime_error("unrecognized type of SqlStmtFieldType obtained");
            }
        }

    private:
        SqlStmtFieldType m_type; /**< TODO */
        SqlStmtField m_binaryData; /**< TODO */
        std::string m_szStringData; /**< TODO */
};

// template specialization
/**
 * @brief
 *
 * @param val
 */
template<> inline void SqlStmtFieldData::set(bool val) { m_type = FIELD_BOOL; m_binaryData.ui8 = val; }
/**
 * @brief
 *
 * @param val
 */
template<> inline void SqlStmtFieldData::set(uint8 val) { m_type = FIELD_UI8; m_binaryData.ui8 = val; }
/**
 * @brief
 *
 * @param val
 */
template<> inline void SqlStmtFieldData::set(int8 val) { m_type = FIELD_I8; m_binaryData.i8 = val; }
/**
 * @brief
 *
 * @param val
 */
template<> inline void SqlStmtFieldData::set(uint16 val) { m_type = FIELD_UI16; m_binaryData.ui16 = val; }
/**
 * @brief
 *
 * @param val
 */
template<> inline void SqlStmtFieldData::set(int16 val) { m_type = FIELD_I16; m_binaryData.i16 = val; }
/**
 * @brief
 *
 * @param val
 */
template<> inline void SqlStmtFieldData::set(uint32 val) { m_type = FIELD_UI32; m_binaryData.ui32 = val; }
/**
 * @brief
 *
 * @param val
 */
template<> inline void SqlStmtFieldData::set(int32 val) { m_type = FIELD_I32; m_binaryData.i32 = val; }
/**
 * @brief
 *
 * @param val
 */
template<> inline void SqlStmtFieldData::set(uint64 val) { m_type = FIELD_UI64; m_binaryData.ui64 = val; }
/**
 * @brief
 *
 * @param val
 */
template<> inline void SqlStmtFieldData::set(int64 val) { m_type = FIELD_I64; m_binaryData.i64 = val; }
/**
 * @brief
 *
 * @param val
 */
template<> inline void SqlStmtFieldData::set(float val) { m_type = FIELD_FLOAT; m_binaryData.f = val; }
/**
 * @brief
 *
 * @param val
 */
template<> inline void SqlStmtFieldData::set(double val) { m_type = FIELD_DOUBLE; m_binaryData.d = val; }
/**
 * @brief
 *
 * @param val
 */
template<> inline void SqlStmtFieldData::set(const char* val) { m_type = FIELD_STRING; m_szStringData = val; }

class SqlStatement;
// prepared statement executor
/**
 * @brief
 *
 */
class SqlStmtParameters
{
    public:
        /**
         * @brief
         *
         */
        typedef std::vector<SqlStmtFieldData> ParameterContainer;

        /**
         * @brief reserve memory to contain all input parameters of stmt
         *
         * @param nParams
         */
        explicit SqlStmtParameters(uint32 nParams);

        /**
         * @brief
         *
         */
        ~SqlStmtParameters() {}

        /**
         * @brief get amount of bound parameters
         *
         * @return uint32
         */
        uint32 boundParams() const { return m_params.size(); }

        /**
         * @brief add parameter
         *
         * @param data
         */
        void addParam(const SqlStmtFieldData& data) { m_params.push_back(data); }

        /**
         * @brief empty SQL statement parameters.
         *
         * In case nParams > 1 - reserve memory for parameters should help to
         * reuse the same object with batched SQL requests
         *
         * @param stmt
         */
        void reset(const SqlStatement& stmt);
        /**
         * @brief swaps contents of internal param container
         *
         * @param obj
         */
        void swap(SqlStmtParameters& obj);
        /**
         * @brief get bound parameters
         *
         * @return const ParameterContainer
         */
        const ParameterContainer& params() const { return m_params; }

    private:
        /**
         * @brief
         *
         * @param obj
         * @return SqlStmtParameters &operator
         */
        SqlStmtParameters& operator=(const SqlStmtParameters& obj);

        ParameterContainer m_params; /**< statement parameter holder */
};

/**
 * @brief statement ID encapsulation logic
 *
 */
class SqlStatementID
{
    public:
        /**
         * @brief
         *
         */
        SqlStatementID() : m_bInitialized(false) {}

        /**
         * @brief
         *
         * @return int
         */
        int ID() const { return m_nIndex; }
        /**
         * @brief
         *
         * @return uint32
         */
        uint32 arguments() const { return m_nArguments; }
        /**
         * @brief
         *
         * @return bool
         */
        bool initialized() const { return m_bInitialized; }

    private:
        friend class Database;
        /**
         * @brief
         *
         * @param nID
         * @param nArgs
         */
        void init(int nID, uint32 nArgs) { m_nIndex = nID; m_nArguments = nArgs; m_bInitialized = true; }

        int m_nIndex; /**< TODO */
        uint32 m_nArguments; /**< TODO */
        bool m_bInitialized; /**< TODO */
};

/**
 * @brief statement index
 *
 */
class SqlStatement
{
    public:
        /**
         * @brief
         *
         */
        ~SqlStatement() { delete m_pParams; }

        /**
         * @brief
         *
         * @param index
         */
        SqlStatement(const SqlStatement& index) : m_index(index.m_index), m_pDB(index.m_pDB), m_pParams(NULL)
        {
            if (index.m_pParams)
                { m_pParams = new SqlStmtParameters(*(index.m_pParams)); }
        }

        /**
         * @brief
         *
         * @param index
         * @return SqlStatement &operator
         */
        SqlStatement& operator=(const SqlStatement& index);

        /**
         * @brief
         *
         * @return int
         */
        int ID() const { return m_index.ID(); }
        /**
         * @brief
         *
         * @return uint32
         */
        uint32 arguments() const { return m_index.arguments(); }

        /**
         * @brief
         *
         * @return bool
         */
        bool Execute();
        /**
         * @brief
         *
         * @return bool
         */
        bool DirectExecute();

        // templates to simplify 1-4 parameter bindings
        template<typename ParamType1>
        /**
         * @brief
         *
         * @param param1
         * @return bool
         */
        bool PExecute(ParamType1 param1)
        {
            arg(param1);
            return Execute();
        }

        template<typename ParamType1, typename ParamType2>
        /**
         * @brief
         *
         * @param param1
         * @param param2
         * @return bool
         */
        bool PExecute(ParamType1 param1, ParamType2 param2)
        {
            arg(param1);
            arg(param2);
            return Execute();
        }

        template<typename ParamType1, typename ParamType2, typename ParamType3>
        /**
         * @brief
         *
         * @param param1
         * @param param2
         * @param param3
         * @return bool
         */
        bool PExecute(ParamType1 param1, ParamType2 param2, ParamType3 param3)
        {
            arg(param1);
            arg(param2);
            arg(param3);
            return Execute();
        }

        template<typename ParamType1, typename ParamType2, typename ParamType3, typename ParamType4>
        /**
         * @brief
         *
         * @param param1
         * @param param2
         * @param param3
         * @param param4
         * @return bool
         */
        bool PExecute(ParamType1 param1, ParamType2 param2, ParamType3 param3, ParamType4 param4)
        {
            arg(param1);
            arg(param2);
            arg(param3);
            arg(param4);
            return Execute();
        }

        // bind parameters with specified type
        /**
         * @brief
         *
         * @param var
         */
        void addBool(bool var) { arg(var); }
        /**
         * @brief
         *
         * @param var
         */
        void addUInt8(uint8 var) { arg(var); }
        /**
         * @brief
         *
         * @param var
         */
        void addInt8(int8 var) { arg(var); }
        /**
         * @brief
         *
         * @param var
         */
        void addUInt16(uint16 var) { arg(var); }
        /**
         * @brief
         *
         * @param var
         */
        void addInt16(int16 var) { arg(var); }
        /**
         * @brief
         *
         * @param var
         */
        void addUInt32(uint32 var) { arg(var); }
        /**
         * @brief
         *
         * @param var
         */
        void addInt32(int32 var) { arg(var); }
        /**
         * @brief
         *
         * @param var
         */
        void addUInt64(uint64 var) { arg(var); }
        /**
         * @brief
         *
         * @param var
         */
        void addInt64(int64 var) { arg(var); }
        /**
         * @brief
         *
         * @param var
         */
        void addFloat(float var) { arg(var); }
        /**
         * @brief
         *
         * @param var
         */
        void addDouble(double var) { arg(var); }
        /**
         * @brief
         *
         * @param var
         */
        void addString(const char* var) { arg(var); }
        /**
         * @brief
         *
         * @param var
         */
        void addString(const std::string& var) { arg(var.c_str()); }
        /**
         * @brief
         *
         * @param ss
         */
        void addString(std::ostringstream& ss) { arg(ss.str().c_str()); ss.str(std::string()); }

    protected:
        // don't allow anyone except Database class to create static SqlStatement objects
        friend class Database;
        /**
         * @brief
         *
         * @param index
         * @param db
         */
        SqlStatement(const SqlStatementID& index, Database& db) : m_index(index), m_pDB(&db), m_pParams(NULL) {}

    private:

        /**
         * @brief
         *
         * @return SqlStmtParameters
         */
        SqlStmtParameters* get()
        {
            if (!m_pParams)
                { m_pParams = new SqlStmtParameters(arguments()); }

            return m_pParams;
        }

        /**
         * @brief
         *
         * @return SqlStmtParameters
         */
        SqlStmtParameters* detach()
        {
            SqlStmtParameters* p = m_pParams ? m_pParams : new SqlStmtParameters(0);
            m_pParams = NULL;
            return p;
        }

        // helper function
        // use appropriate add* functions to bind specific data type
        template<typename ParamType>
        /**
         * @brief
         *
         * @param val
         */
        void arg(ParamType val)
        {
            SqlStmtParameters* p = get();
            p->addParam(SqlStmtFieldData(val));
        }

        SqlStatementID m_index; /**< TODO */
        Database* m_pDB; /**< TODO */
        SqlStmtParameters* m_pParams; /**< TODO */
};

/**
 * @brief base prepared statement class
 *
 */
class SqlPreparedStatement
{
    public:
        /**
         * @brief
         *
         */
        virtual ~SqlPreparedStatement() {}

        /**
         * @brief
         *
         * @return bool
         */
        bool isPrepared() const { return m_bPrepared; }
        /**
         * @brief
         *
         * @return bool
         */
        bool isQuery() const { return m_bIsQuery; }

        /**
         * @brief
         *
         * @return uint32
         */
        uint32 params() const { return m_nParams; }
        /**
         * @brief
         *
         * @return uint32
         */
        uint32 columns() const { return isQuery() ? m_nColumns : 0; }

        /**
         * @brief initialize internal structures of prepared statement
         *
         * upon success m_bPrepared should be true
         *
         * @return bool
         */
        virtual bool prepare() = 0;
        /**
         * @brief bind parameters for prepared statement from parameter placeholder
         *
         * @param holder
         */
        virtual void bind(const SqlStmtParameters& holder) = 0;

        /**
         * @brief execute statement w/o result set
         *
         * @return bool
         */
        virtual bool execute() = 0;

    protected:
        /**
         * @brief
         *
         * @param fmt
         * @param conn
         */
        SqlPreparedStatement(const std::string& fmt, SqlConnection& conn) :
            m_nParams(0), m_nColumns(0), m_bIsQuery(false),
            m_bPrepared(false), m_szFmt(fmt), m_pConn(conn)
        {}

        uint32 m_nParams; /**< TODO */
        uint32 m_nColumns; /**< TODO */
        bool m_bIsQuery; /**< TODO */
        bool m_bPrepared; /**< TODO */
        std::string m_szFmt; /**< TODO */
        SqlConnection& m_pConn; /**< TODO */
};

/**
 * @brief prepared statements via plain SQL string requests
 *
 */
class SqlPlainPreparedStatement : public SqlPreparedStatement
{
    public:
        /**
         * @brief
         *
         * @param fmt
         * @param conn
         */
        SqlPlainPreparedStatement(const std::string& fmt, SqlConnection& conn);
        /**
         * @brief
         *
         */
        ~SqlPlainPreparedStatement() {}

        /**
         * @brief this statement is always prepared
         *
         * @return bool
         */
        virtual bool prepare() override { return true; }

        /**
         * @brief we should replace all '?' symbols with substrings with proper format
         *
         * @param holder
         */
        virtual void bind(const SqlStmtParameters& holder) override;

        /**
         * @brief
         *
         * @return bool
         */
        virtual bool execute() override;

    protected:
        /**
         * @brief
         *
         * @param data
         * @param fmt
         */
        void DataToString(const SqlStmtFieldData& data, std::ostringstream& fmt);

        std::string m_szPlainRequest; /**< TODO */
};

#endif
