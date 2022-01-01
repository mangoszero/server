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

#include "SoapThread.h"

#include "AccountMgr.h"
#include "Log.h"
#include "World.h"

#include "soapStub.h"

void SoapThread(const std::string& host, uint16 port)
{
    struct soap soap;
    soap_init(&soap);
    soap_set_imode(&soap, SOAP_C_UTFSTRING);
    soap_set_omode(&soap, SOAP_C_UTFSTRING);

    // check every 3 seconds if world ended
    soap.accept_timeout = 3;
    soap.recv_timeout = 5;
    soap.send_timeout = 5;

    if (!soap_valid_socket(soap_bind(&soap, host.c_str(), port, 100)))
    {
        sLog.outError("SoapThread: couldn't bind to %s:%d", host.c_str(), port);
        exit(-1);
    }

    sLog.outString("SoapThread: Bound to http://%s:%d", host.c_str(), port);

    while (!World::IsStopped())
    {
        if (!soap_valid_socket(soap_accept(&soap)))
        {
            continue; // ran into an accept timeout
        }

        sLog.outString("Accepted connection from IP %d.%d.%d.%d", (int)(soap.ip >> 24) & 0xFF, (int)(soap.ip >> 16) & 0xFF, (int)(soap.ip >> 8) & 0xFF, (int)soap.ip & 0xFF);
        struct soap* thread_soap = soap_copy(&soap); // make a safe copy
        process_message(thread_soap);
    }

    soap_destroy(&soap);
    soap_end(&soap);
    soap_done(&soap);
}

void process_message(struct soap* soap_message)
{
    soap_serve(soap_message);
    soap_destroy(soap_message); // dealloc C++ data
    soap_end(soap_message);     // dealloc data and clean up
    soap_free(soap_message);    // detach soap struct and free up memory
}

int ns1__executeCommand(soap* soap, char* command, char** result)
{
    // security check
    if (!soap->userid || !soap->passwd)
    {
        sLog.outString("SoapThread: Client didn't provide login information");
        return 401;
    }

    uint32 accountId = sAccountMgr.GetId(soap->userid);
    if (!accountId)
    {
        sLog.outString("SoapThread: Client used invalid username %s", soap->userid);
        return 401;
    }

    if (!sAccountMgr.CheckPassword(accountId, soap->passwd))
    {
        sLog.outString("SoapThread: Client sent an invalid password for account %s", soap->passwd);
        return 401;
    }

    /* ToDo: Add realm check */
    if (sAccountMgr.GetSecurity(accountId) < SEC_ADMINISTRATOR)
    {
        sLog.outString("SoapThread: %s's account security level is to low", soap->userid);
        return 403;
    }

    if (!command || !*command)
    {
        return soap_sender_fault(soap, "Command can not be empty", "The supplied command was an empty string");
    }

    sLog.outString("SoapThread: Recieved command %s", command);
    SOAPCommand connection;

    // commands are executed in the world thread and have to wait till they are completed.
    {
        CliCommandHolder* cmd = new CliCommandHolder(accountId, SEC_CONSOLE, &connection, command, &SOAPCommand::print, &SOAPCommand::commandFinished);
        sWorld.QueueCliCommand(cmd);
    }

    // wait until the command has finished executing
    connection.finishedPromise.get_future().wait();

    // the command has finished executing already
    char* printBuffer = soap_strdup(soap, connection.m_printBuffer.c_str());
    if (connection.hasCommandSucceeded())
    {
        *result = printBuffer;
        return SOAP_OK;
    }
    else
    {
        return soap_sender_fault(soap, printBuffer, printBuffer);
    }
}

void SOAPCommand::commandFinished(void* soapconnection, bool success)
{
    SOAPCommand* con = (SOAPCommand*)soapconnection;
    con->setCommandSuccess(success);
}

////////////////////////////////////////////////////////////////////////////////
//
//  Namespace Definition Table
//
////////////////////////////////////////////////////////////////////////////////

struct Namespace namespaces[] =
{
    { "SOAP-ENV", "http://schemas.xmlsoap.org/soap/envelope/" }, // must be first
    { "SOAP-ENC", "http://schemas.xmlsoap.org/soap/encoding/" }, // must be second
    { "xsi", "http://www.w3.org/1999/XMLSchema-instance", "http://www.w3.org/*/XMLSchema-instance" },
    { "xsd", "http://www.w3.org/1999/XMLSchema",          "http://www.w3.org/*/XMLSchema" },
    { "ns1", "urn:MaNGOS" },     // "ns1" namespace prefix
    { NULL, NULL }
};
