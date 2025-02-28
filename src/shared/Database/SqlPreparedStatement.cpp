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

#include "DatabaseEnv.h"

/**
 * @brief Constructor to reserve memory for parameters.
 * @param nParams The number of parameters to reserve memory for.
 */
SqlStmtParameters::SqlStmtParameters(uint32 nParams)
{
    // reserve memory if needed
    if (nParams > 0)
    {
        m_params.reserve(nParams);
    }
}

/**
 * @brief Reset the parameters.
 * @param stmt The statement to reset the parameters for.
 */
void SqlStmtParameters::reset(const SqlStatement& stmt)
{
    m_params.clear();
    // reserve memory if needed
    if (stmt.arguments() > 0)
    {
        m_params.reserve(stmt.arguments());
    }
}

//////////////////////////////////////////////////////////////////////////

/**
 * @brief Assignment operator.
 * @param index The statement to assign from.
 * @return The assigned statement.
 */
SqlStatement& SqlStatement::operator=(const SqlStatement& index)
{
    if (this != &index)
    {
        m_index = index.m_index;
        m_pDB = index.m_pDB;

        delete m_pParams;
        m_pParams = NULL;

        if (index.m_pParams)
        {
            m_pParams = new SqlStmtParameters(*(index.m_pParams));
        }
    }

    return *this;
}

/**
 * @brief Execute the statement.
 * @return True if the execution was successful, false otherwise.
 */
bool SqlStatement::Execute()
{
    SqlStmtParameters* args = detach();
    // verify amount of bound parameters
    if (args->boundParams() != arguments())
    {
        sLog.outError("SQL ERROR: wrong amount of parameters (%i instead of %i)", args->boundParams(), arguments());
        sLog.outError("SQL ERROR: statement: %s", m_pDB->GetStmtString(ID()).c_str());
        MANGOS_ASSERT(false);
        return false;
    }

    return m_pDB->ExecuteStmt(m_index, args);
}

/**
 * @brief Directly execute the statement.
 * @return True if the execution was successful, false otherwise.
 */
bool SqlStatement::DirectExecute()
{
    SqlStmtParameters* args = detach();
    // verify amount of bound parameters
    if (args->boundParams() != arguments())
    {
        sLog.outError("SQL ERROR: wrong amount of parameters (%i instead of %i)", args->boundParams(), arguments());
        sLog.outError("SQL ERROR: statement: %s", m_pDB->GetStmtString(ID()).c_str());
        MANGOS_ASSERT(false);
        return false;
    }

    return m_pDB->DirectExecuteStmt(m_index, args);
}

//////////////////////////////////////////////////////////////////////////

/**
 * @brief Constructor to create a SqlPlainPreparedStatement object.
 * @param fmt The format string for the statement.
 * @param conn The SQL connection.
 */
SqlPlainPreparedStatement::SqlPlainPreparedStatement(const std::string& fmt, SqlConnection& conn) : SqlPreparedStatement(fmt, conn)
{
    m_bPrepared = true;
    m_nParams = std::count(m_szFmt.begin(), m_szFmt.end(), '?');
    m_bIsQuery = strnicmp(m_szFmt.c_str(), "select", 6) == 0;
}

/**
 * @brief Replace all '?' symbols with substrings with proper format.
 * @param holder The parameter holder.
 */
void SqlPlainPreparedStatement::bind(const SqlStmtParameters& holder)
{
    // verify if we bound all needed input parameters
    if (m_nParams != holder.boundParams())
    {
        MANGOS_ASSERT(false);
        return;
    }

    // reset resulting plain SQL request
    m_szPlainRequest = m_szFmt;
    size_t nLastPos = 0;

    SqlStmtParameters::ParameterContainer const& _args = holder.params();

    SqlStmtParameters::ParameterContainer::const_iterator iter_last = _args.end();
    for (SqlStmtParameters::ParameterContainer::const_iterator iter = _args.begin(); iter != iter_last; ++iter)
    {
        // bind parameter
        const SqlStmtFieldData& data = (*iter);

        std::ostringstream fmt;
        DataToString(data, fmt);

        nLastPos = m_szPlainRequest.find('?', nLastPos);
        if (nLastPos != std::string::npos)
        {
            std::string tmp = fmt.str();
            m_szPlainRequest.replace(nLastPos, 1, tmp);
            nLastPos += tmp.length();
        }
    }
}

/**
 * @brief Execute the statement.
 * @return True if the execution was successful, false otherwise.
 */
bool SqlPlainPreparedStatement::execute()
{
    if (m_szPlainRequest.empty())
    {
        return false;
    }

    return m_pConn.Execute(m_szPlainRequest.c_str());
}

/**
 * @brief Convert data to string format.
 * @param data The data to convert.
 * @param fmt The output format string.
 */
void SqlPlainPreparedStatement::DataToString(const SqlStmtFieldData& data, std::ostringstream& fmt)
{
    switch (data.type())
    {
        case FIELD_BOOL:    fmt << "'" << uint32(data.toBool()) << "'";     break;
        case FIELD_UI8:     fmt << "'" << uint32(data.toUint8()) << "'";    break;
        case FIELD_UI16:    fmt << "'" << uint32(data.toUint16()) << "'";   break;
        case FIELD_UI32:    fmt << "'" << data.toUint32() << "'";           break;
        case FIELD_UI64:    fmt << "'" << data.toUint64() << "'";           break;
        case FIELD_I8:      fmt << "'" << int32(data.toInt8()) << "'";      break;
        case FIELD_I16:     fmt << "'" << int32(data.toInt16()) << "'";     break;
        case FIELD_I32:     fmt << "'" << data.toInt32() << "'";            break;
        case FIELD_I64:     fmt << "'" << data.toInt64() << "'";            break;
        case FIELD_FLOAT:   fmt << "'" << data.toFloat() << "'";            break;
        case FIELD_DOUBLE:  fmt << "'" << data.toDouble() << "'";           break;
        case FIELD_STRING:
        {
            std::string tmp = data.toStr();
            m_pConn.DB().escape_string(tmp);
            fmt << "'" << tmp << "'";
            break;
        }
        case FIELD_NONE:                                                    break;
    }
}

/**
 * @brief Set the value of the field.
 * @param param1 The value to set.
 */
template<typename T1>
void SqlStmtFieldData::set(T1 param1)
{
    // Implementation for setting the value based on the type of param1
    if constexpr (std::is_same_v<T1, bool>)
    {
        m_type = FIELD_BOOL;
        m_binaryData.ui8 = param1;
    }
    else if constexpr (std::is_same_v<T1, uint8>)
    {
        m_type = FIELD_UI8;
        m_binaryData.ui8 = param1;
    }
    else if constexpr (std::is_same_v<T1, int8>)
    {
        m_type = FIELD_I8;
        m_binaryData.i8 = param1;
    }
    else if constexpr (std::is_same_v<T1, uint16>)
    {
        m_type = FIELD_UI16;
        m_binaryData.ui16 = param1;
    }
    else if constexpr (std::is_same_v<T1, int16>)
    {
        m_type = FIELD_I16;
        m_binaryData.i16 = param1;
    }
    else if constexpr (std::is_same_v<T1, uint32>)
    {
        m_type = FIELD_UI32;
        m_binaryData.ui32 = param1;
    }
    else if constexpr (std::is_same_v<T1, int32>)
    {
        m_type = FIELD_I32;
        m_binaryData.i32 = param1;
    }
    else if constexpr (std::is_same_v<T1, uint64>)
    {
        m_type = FIELD_UI64;
        m_binaryData.ui64 = param1;
    }
    else if constexpr (std::is_same_v<T1, int64>)
    {
        m_type = FIELD_I64;
        m_binaryData.i64 = param1;
    }
    else if constexpr (std::is_same_v<T1, float>)
    {
        m_type = FIELD_FLOAT;
        m_binaryData.f = param1;
    }
    else if constexpr (std::is_same_v<T1, double>)
    {
        m_type = FIELD_DOUBLE;
        m_binaryData.d = param1;
    }
    else if constexpr (std::is_same_v<T1, const char*>)
    {
        m_type = FIELD_STRING;
        m_szStringData = param1;
    }
    else
    {
        throw std::runtime_error("Unsupported type for SqlStmtFieldData::set");
    }
}

/**
 * @brief Swap the contents of the internal parameter container.
 * @param obj The object to swap with.
 */
void SqlStmtParameters::swap(SqlStmtParameters& obj)
{
    std::swap(m_params, obj.m_params);
}

/**
 * @brief Assignment operator.
 * @param obj The object to assign from.
 * @return The assigned object.
 */
SqlStmtParameters& SqlStmtParameters::operator=(const SqlStmtParameters& obj)
{
    if (this != &obj)
    {
        m_params = obj.m_params;
    }
    return *this;
}
