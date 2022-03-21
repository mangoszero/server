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

#ifndef MANGOS_H_SOAPTHREAD
#define MANGOS_H_SOAPTHREAD

#include "Common.h"

#include <mutex>
#include <future>
#include <string>

void process_message(struct soap* soap_message);
void SoapThread(const std::string& host, uint16 port);

class SOAPCommand
{
public:
    SOAPCommand() : m_success(false) { }
    ~SOAPCommand() { }

    void appendToPrintBuffer(std::string msg)
    {
        m_printBuffer += msg;
    }

    void setCommandSuccess(bool val)
    {
        m_success = val;
        finishedPromise.set_value();
    }

    bool hasCommandSucceeded() const
    {
        return m_success;
    }

    static void print(void* callbackArg, const char* msg)
    {
        ((SOAPCommand*)callbackArg)->appendToPrintBuffer(msg);
    }

    static void commandFinished(void* callbackArg, bool success);

    bool m_success;
    std::string m_printBuffer;
    std::promise<void> finishedPromise;
};

#endif
