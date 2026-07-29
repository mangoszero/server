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

#ifndef MANGOS_H_SOAPSERVICE
#define MANGOS_H_SOAPSERVICE

#include "SoapThread.h"

#include "../Service.h"

#include "Platform/Define.h"

#include <string>
#include <thread>

/**
 * @brief The gSOAP listener, as a Master service.
 *
 * SoapThread() polls with a one-second accept timeout and re-checks
 * World::IsStopped() between polls, so there is nothing to signal here: the
 * world stopping is the stop request, and Join() only has to wait out at most
 * one poll interval.
 */
class SoapService : public IService
{
    public:

        SoapService(const std::string& host, uint16 port)
            : m_host(host), m_port(port) {}

        const char* Name() const override { return "SOAP"; }

        void Start() override
        {
            m_thread = std::thread(SoapThread, m_host, m_port);
        }

        void Join() override
        {
            if (m_thread.joinable())
            {
                m_thread.join();
            }
        }

    private:

        std::string m_host;
        uint16      m_port;
        std::thread m_thread;
};

#endif
