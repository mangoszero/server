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
 * @brief Union to hold different types of SQL statement fields.
 */
union SqlStmtField
{
    bool boolean; /**< Boolean field */
    uint8 ui8; /**< Unsigned 8-bit integer field */
    int8 i8; /**< Signed 8-bit integer field */
    uint16 ui16; /**< Unsigned 16-bit integer field */
    int16 i16; /**< Signed 16-bit integer field */
    uint32 ui32; /**< Unsigned 32-bit integer field */
    int32 i32; /**< Signed 32-bit integer field */
    uint64 ui64; /**< Unsigned 64-bit integer field */
    int64 i64; /**< Signed 64-bit integer field */
    float f; /**< Float field */
    double d; /**< Double field */
};

/**
 * @brief Enum to represent the type of SQL statement field.
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
 * @brief Class to hold data for a SQL statement field.
 */
class SqlStmtFieldData
{
    public:
        /**
         * @brief Default constructor.
         */
        SqlStmtFieldData() : m_type(FIELD_NONE) { m_binaryData.ui64 = 0; }
        /**
         * @brief Destructor.
         */
        ~SqlStmtFieldData() {}

        /**
         * @brief Constructor to initialize with a value.
         * @param param The value to initialize with.
         */
        template<typename T>
        SqlStmtFieldData(T param) { set(param); }

        /**
         * @brief Set the value of the field.
         * @param param1 The value to set.
         */
        template<typename T1>
        void set(T1 param1);

        /**
         * @brief Get the value as a boolean.
         * @return The boolean value.
         */
        bool toBool() const { MANGOS_ASSERT(m_type == FIELD_BOOL); return static_cast<bool>(m_binaryData.ui8); }
        /**
         * @brief Get the value as an unsigned 8-bit integer.
         * @return The unsigned 8-bit integer value.
         */
        uint8 toUint8() const { MANGOS_ASSERT(m_type == FIELD_UI8); return m_binaryData.ui8; }
        /**
         * @brief Get the value as a signed 8-bit integer.
         * @return The signed 8-bit integer value.
         */
        int8 toInt8() const { MANGOS_ASSERT(m_type == FIELD_I8); return m_binaryData.i8; }
        /**
         * @brief Get the value as an unsigned 16-bit integer.
         * @return The unsigned 16-bit integer value.
         */
        uint16 toUint16() const { MANGOS_ASSERT(m_type == FIELD_UI16); return m_binaryData.ui16; }
        /**
         * @brief Get the value as a signed 16-bit integer.
         * @return The signed 16-bit integer value.
         */
        int16 toInt16() const { MANGOS_ASSERT(m_type == FIELD_I16); return m_binaryData.i16; }
        /**
         * @brief Get the value as an unsigned 32-bit integer.
         * @return The unsigned 32-bit integer value.
         */
        uint32 toUint32() const { MANGOS_ASSERT(m_type == FIELD_UI32); return m_binaryData.ui32; }
        /**
         * @brief Get the value as a signed 32-bit integer.
         * @return The signed 32-bit integer value.
         */
        int32 toInt32() const { MANGOS_ASSERT(m_type == FIELD_I32); return m_binaryData.i32; }
        /**
         * @brief Get the value as an unsigned 64-bit integer.
         * @return The unsigned 64-bit integer value.
         */
        uint64 toUint64() const { MANGOS_ASSERT(m_type == FIELD_UI64); return m_binaryData.ui64; }
        /**
         * @brief Get the value as a signed 64-bit integer.
         * @return The signed 64-bit integer value.
         */
        int64 toInt64() const { MANGOS_ASSERT(m_type == FIELD_I64); return m_binaryData.i64; }
        /**
         * @brief Get the value as a float.
         * @return The float value.
         */
        float toFloat() const { MANGOS_ASSERT(m_type == FIELD_FLOAT); return m_binaryData.f; }
        /**
         * @brief Get the value as a double.
         * @return The double value.
         */
        double toDouble() const { MANGOS_ASSERT(m_type == FIELD_DOUBLE); return m_binaryData.d; }
        /**
         * @brief Get the value as a string.
         * @return The string value.
         */
        const char* toStr() const { MANGOS_ASSERT(m_type == FIELD_STRING); return m_szStringData.c_str(); }

        /**
         * @brief Get the type of the field.
         * @return The type of the field.
         */
        SqlStmtFieldType type() const { return m_type; }
        /**
         * @brief Get the underlying buffer of the field.
         * @return The buffer of the field.
         */
        void* buff() const { return m_type == FIELD_STRING ? (void*)m_szStringData.c_str() : (void*)&m_binaryData; }

        /**
         * @brief Get the size of the field.
         * @return The size of the field.
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
        SqlStmtFieldType m_type; /**< The type of the field */
        SqlStmtField m_binaryData; /**< The binary data of the field */
        std::string m_szStringData; /**< The string data of the field */
};

// template specialization
template<> inline void SqlStmtFieldData::set(bool val) { m_type = FIELD_BOOL; m_binaryData.ui8 = val; }
template<> inline void SqlStmtFieldData::set(uint8 val) { m_type = FIELD_UI8; m_binaryData.ui8 = val; }
template<> inline void SqlStmtFieldData::set(int8 val) { m_type = FIELD_I8; m_binaryData.i8 = val; }
template<> inline void SqlStmtFieldData::set(uint16 val) { m_type = FIELD_UI16; m_binaryData.ui16 = val; }
template<> inline void SqlStmtFieldData::set(int16 val) { m_type = FIELD_I16; m_binaryData.i16 = val; }
template<> inline void SqlStmtFieldData::set(uint32 val) { m_type = FIELD_UI32; m_binaryData.ui32 = val; }
template<> inline void SqlStmtFieldData::set(int32 val) { m_type = FIELD_I32; m_binaryData.i32 = val; }
template<> inline void SqlStmtFieldData::set(uint64 val) { m_type = FIELD_UI64; m_binaryData.ui64 = val; }
template<> inline void SqlStmtFieldData::set(int64 val) { m_type = FIELD_I64; m_binaryData.i64 = val; }
template<> inline void SqlStmtFieldData::set(float val) { m_type = FIELD_FLOAT; m_binaryData.f = val; }
template<> inline void SqlStmtFieldData::set(double val) { m_type = FIELD_DOUBLE; m_binaryData.d = val; }
template<> inline void SqlStmtFieldData::set(const char* val) { m_type = FIELD_STRING; m_szStringData = val; }

class SqlStatement;

/**
 * @brief Class to hold parameters for a SQL statement.
 */
class SqlStmtParameters
{
    public:
        typedef std::vector<SqlStmtFieldData> ParameterContainer;

        /**
         * @brief Constructor to reserve memory for parameters.
         * @param nParams The number of parameters to reserve memory for.
         */
        explicit SqlStmtParameters(uint32 nParams);

        /**
         * @brief Destructor.
         */
        ~SqlStmtParameters() {}

        /**
         * @brief Get the number of bound parameters.
         * @return The number of bound parameters.
         */
        uint32 boundParams() const { return m_params.size(); }

        /**
         * @brief Add a parameter.
         * @param data The parameter to add.
         */
        void addParam(const SqlStmtFieldData& data) { m_params.push_back(data); }

        /**
         * @brief Reset the parameters.
         * @param stmt The statement to reset the parameters for.
         */
        void reset(const SqlStatement& stmt);
        /**
         * @brief Swap the contents of the internal parameter container.
         * @param obj The object to swap with.
         */
        void swap(SqlStmtParameters& obj);
        /**
         * @brief Get the bound parameters.
         * @return The bound parameters.
         */
        const ParameterContainer& params() const { return m_params; }

    private:
        /**
         * @brief Assignment operator.
         * @param obj The object to assign from.
         * @return The assigned object.
         */
        SqlStmtParameters& operator=(const SqlStmtParameters& obj);

        ParameterContainer m_params; /**< The parameter container */
};

/**
 * @brief Class to encapsulate a SQL statement ID.
 */
class SqlStatementID
{
    public:
        /**
         * @brief Default constructor.
         */
        SqlStatementID() : m_nIndex(0), m_nArguments(0), m_bInitialized(false) {}

        /**
         * @brief Get the ID of the statement.
         * @return The ID of the statement.
         */
        int ID() const { return m_nIndex; }
        /**
         * @brief Get the number of arguments for the statement.
         * @return The number of arguments.
         */
        uint32 arguments() const { return m_nArguments; }
        /**
         * @brief Check if the statement is initialized.
         * @return True if the statement is initialized, false otherwise.
         */
        bool initialized() const { return m_bInitialized; }

    private:
        friend class Database;
        /**
         * @brief Initialize the statement ID.
         * @param nID The ID of the statement.
         * @param nArgs The number of arguments for the statement.
         */
        void init(int nID, uint32 nArgs) { m_nIndex = nID; m_nArguments = nArgs; m_bInitialized = true; }

        int m_nIndex; /**< The ID of the statement */
        uint32 m_nArguments; /**< The number of arguments for the statement */
        bool m_bInitialized; /**< Whether the statement is initialized */
};

/**
 * @brief Class to represent a SQL statement.
 */
class SqlStatement
{
    public:
        /**
         * @brief Destructor.
         */
        ~SqlStatement() { delete m_pParams; }

        /**
         * @brief Copy constructor.
         * @param index The statement to copy from.
         */
        SqlStatement(const SqlStatement& index) : m_index(index.m_index), m_pDB(index.m_pDB), m_pParams(NULL)
        {
            if (index.m_pParams)
            {
                m_pParams = new SqlStmtParameters(*(index.m_pParams));
            }
        }

        /**
         * @brief Assignment operator.
         * @param index The statement to assign from.
         * @return The assigned statement.
         */
        SqlStatement& operator=(const SqlStatement& index);

        /**
         * @brief Get the ID of the statement.
         * @return The ID of the statement.
         */
        int ID() const { return m_index.ID(); }
        /**
         * @brief Get the number of arguments for the statement.
         * @return The number of arguments.
         */
        uint32 arguments() const { return m_index.arguments(); }

        /**
         * @brief Execute the statement.
         * @return True if the execution was successful, false otherwise.
         */
        bool Execute();
        /**
         * @brief Directly execute the statement.
         * @return True if the execution was successful, false otherwise.
         */
        bool DirectExecute();

        // templates to simplify 1-4 parameter bindings
        template<typename ParamType1>
        /**
         * @brief Execute the statement with one parameter.
         * @param param1 The parameter to execute with.
         * @return True if the execution was successful, false otherwise.
         */
        bool PExecute(ParamType1 param1)
        {
            arg(param1);
            return Execute();
        }

        template<typename ParamType1, typename ParamType2>
        /**
         * @brief Execute the statement with two parameters.
         * @param param1 The first parameter to execute with.
         * @param param2 The second parameter to execute with.
         * @return True if the execution was successful, false otherwise.
         */
        bool PExecute(ParamType1 param1, ParamType2 param2)
        {
            arg(param1);
            arg(param2);
            return Execute();
        }

        template<typename ParamType1, typename ParamType2, typename ParamType3>
        /**
         * @brief Execute the statement with three parameters.
         * @param param1 The first parameter to execute with.
         * @param param2 The second parameter to execute with.
         * @param param3 The third parameter to execute with.
         * @return True if the execution was successful, false otherwise.
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
         * @brief Execute the statement with four parameters.
         * @param param1 The first parameter to execute with.
         * @param param2 The second parameter to execute with.
         * @param param3 The third parameter to execute with.
         * @param param4 The fourth parameter to execute with.
         * @return True if the execution was successful, false otherwise.
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
         * @brief Add a boolean parameter.
         * @param var The boolean parameter to add.
         */
        void addBool(bool var) { arg(var); }
        /**
         * @brief Add an unsigned 8-bit integer parameter.
         * @param var The unsigned 8-bit integer parameter to add.
         */
        void addUInt8(uint8 var) { arg(var); }
        /**
         * @brief Add a signed 8-bit integer parameter.
         * @param var The signed 8-bit integer parameter to add.
         */
        void addInt8(int8 var) { arg(var); }
        /**
         * @brief Add an unsigned 16-bit integer parameter.
         * @param var The unsigned 16-bit integer parameter to add.
         */
        void addUInt16(uint16 var) { arg(var); }
        /**
         * @brief Add a signed 16-bit integer parameter.
         * @param var The signed 16-bit integer parameter to add.
         */
        void addInt16(int16 var) { arg(var); }
        /**
         * @brief Add an unsigned 32-bit integer parameter.
         * @param var The unsigned 32-bit integer parameter to add.
         */
        void addUInt32(uint32 var) { arg(var); }
        /**
         * @brief Add a signed 32-bit integer parameter.
         * @param var The signed 32-bit integer parameter to add.
         */
        void addInt32(int32 var) { arg(var); }
        /**
         * @brief Add an unsigned 64-bit integer parameter.
         * @param var The unsigned 64-bit integer parameter to add.
         */
        void addUInt64(uint64 var) { arg(var); }
        /**
         * @brief Add a signed 64-bit integer parameter.
         * @param var The signed 64-bit integer parameter to add.
         */
        void addInt64(int64 var) { arg(var); }
        /**
         * @brief Add a float parameter.
         * @param var The float parameter to add.
         */
        void addFloat(float var) { arg(var); }
        /**
         * @brief Add a double parameter.
         * @param var The double parameter to add.
         */
        void addDouble(double var) { arg(var); }
        /**
         * @brief Add a string parameter.
         * @param var The string parameter to add.
         */
        void addString(const char* var) { arg(var); }
        /**
         * @brief Add a string parameter from an ostringstream.
         * @param ss The ostringstream containing the string parameter to add.
         */
        void addString(std::ostringstream& ss) { arg(ss.str().c_str()); ss.str(std::string()); }

        /**
         * @brief Add a string parameter from a string
         * @param ss The string containing the string parameter to add.
         */
         void addString(const std::string& var) { arg(var.c_str()); } // Add this line

    protected:
        // don't allow anyone except Database class to create static SqlStatement objects
        friend class Database;
        /**
         * @brief Constructor to create a SqlStatement object.
         * @param index The statement ID.
         * @param db The database object.
         */
        SqlStatement(const SqlStatementID& index, Database& db) : m_index(index), m_pDB(&db), m_pParams(NULL) {}

    private:

        /**
         * @brief Get the parameters for the statement.
         * @return The parameters for the statement.
         */
        SqlStmtParameters* get()
        {
            if (!m_pParams)
            {
                m_pParams = new SqlStmtParameters(arguments());
            }

            return m_pParams;
        }

        /**
         * @brief Detach the parameters from the statement.
         * @return The detached parameters.
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
         * @brief Bind a parameter to the statement.
         * @param val The parameter to bind.
         */
        void arg(ParamType val)
        {
            SqlStmtParameters* p = get();
            p->addParam(SqlStmtFieldData(val));
        }

        SqlStatementID m_index; /**< The statement ID */
        Database* m_pDB; /**< The database object */
        SqlStmtParameters* m_pParams; /**< The parameters for the statement */
};

/**
 * @brief Base class for prepared SQL statements.
 */
class SqlPreparedStatement
{
    public:
        /**
         * @brief Virtual destructor.
         */
        virtual ~SqlPreparedStatement() {}

        /**
         * @brief Check if the statement is prepared.
         * @return True if the statement is prepared, false otherwise.
         */
        bool isPrepared() const { return m_bPrepared; }
        /**
         * @brief Check if the statement is a query.
         * @return True if the statement is a query, false otherwise.
         */
        bool isQuery() const { return m_bIsQuery; }

        /**
         * @brief Get the number of parameters for the statement.
         * @return The number of parameters.
         */
        uint32 params() const { return m_nParams; }
        /**
         * @brief Get the number of columns for the statement.
         * @return The number of columns.
         */
        uint32 columns() const { return isQuery() ? m_nColumns : 0; }

        /**
         * @brief Initialize internal structures of the prepared statement.
         * @return True if the initialization was successful, false otherwise.
         */
        virtual bool prepare() = 0;
        /**
         * @brief Bind parameters for the prepared statement from the parameter placeholder.
         * @param holder The parameter holder.
         */
        virtual void bind(const SqlStmtParameters& holder) = 0;

        /**
         * @brief Execute the statement without a result set.
         * @return True if the execution was successful, false otherwise.
         */
        virtual bool execute() = 0;

    protected:
        /**
         * @brief Constructor to create a SqlPreparedStatement object.
         * @param fmt The format string for the statement.
         * @param conn The SQL connection.
         */
        SqlPreparedStatement(const std::string& fmt, SqlConnection& conn) :
            m_nParams(0), m_nColumns(0), m_bIsQuery(false),
            m_bPrepared(false), m_szFmt(fmt), m_pConn(conn)
        {}

        uint32 m_nParams; /**< The number of parameters for the statement */
        uint32 m_nColumns; /**< The number of columns for the statement */
        bool m_bIsQuery; /**< Whether the statement is a query */
        bool m_bPrepared; /**< Whether the statement is prepared */
        std::string m_szFmt; /**< The format string for the statement */
        SqlConnection& m_pConn; /**< The SQL connection */
};

/**
 * @brief Prepared statements via plain SQL string requests.
 */
class SqlPlainPreparedStatement : public SqlPreparedStatement
{
    public:
        /**
         * @brief Constructor to create a SqlPlainPreparedStatement object.
         * @param fmt The format string for the statement.
         * @param conn The SQL connection.
         */
        SqlPlainPreparedStatement(const std::string& fmt, SqlConnection& conn);
        /**
         * @brief Destructor.
         */
        ~SqlPlainPreparedStatement() {}

        /**
         * @brief This statement is always prepared.
         * @return True.
         */
        bool prepare() override { return true; }

        /**
         * @brief Replace all '?' symbols with substrings with proper format.
         * @param holder The parameter holder.
         */
        void bind(const SqlStmtParameters& holder) override;

        /**
         * @brief Execute the statement.
         * @return True if the execution was successful, false otherwise.
         */
        bool execute() override;

    protected:
        /**
         * @brief Convert data to string format.
         * @param data The data to convert.
         * @param fmt The output format string.
         */
        void DataToString(const SqlStmtFieldData& data, std::ostringstream& fmt);

        std::string m_szPlainRequest; /**< The plain SQL request string */
};

#endif
