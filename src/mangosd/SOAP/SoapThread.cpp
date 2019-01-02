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

#include <ace/OS.h>
#include <ace/Message_Block.h>

#include "SoapThread.h"
#include "soapH.h"
#include "soapStub.h"

#include "World.h"
#include "Log.h"
#include "AccountMgr.h"


/// WARNING! This code needs serious reviewing


struct SOAPCommand
{
    public:
        void appendToPrintBuffer(const char* msg)
          { m_printBuffer += msg; }

        void setCommandSuccess(bool val)
         { m_success = val; }

        bool hasCommandSucceeded()
          { return m_success; }

        static void print(void* callbackArg, const char* msg)
        {
            ((SOAPCommand*)callbackArg)->appendToPrintBuffer(msg);
        }

        static void commandFinished(void* callbackArg, bool success);

        bool m_success;
        std::string m_printBuffer;
};


class SoapPool : public ACE_Task<ACE_MT_SYNCH>
{
    public:
        virtual int svc(void) override
        {
            while (1)
            {
                ACE_Message_Block* mb = 0;
                if (this->getq(mb) == -1)
                {
                    break;
                }

                // Process the message.
                process_message(mb);
            }
            return 0;
        }

    private:
        void process_message(ACE_Message_Block* mb)
        {
            struct soap* soap;
            ACE_OS::memcpy(&soap, mb->rd_ptr(), sizeof(struct soap*));
            mb->release();

            soap_serve(soap);

            soap_destroy(soap); // dealloc C++ data
            soap_end(soap); // dealloc data and clean up
            soap_done(soap); // detach soap struct
            free(soap);
        }
};



SoapThread::~SoapThread()
{
  if(pool_)
      delete pool_;
}

int SoapThread::open(void* unused)
{
    // create pool
    pool_ = new SoapPool;
    pool_->activate(THR_NEW_LWP | THR_JOINABLE, SOAP_THREADS);

    int m;
    soap_init(&soap_);
    soap_set_imode(&soap_, SOAP_C_UTFSTRING);
    soap_set_omode(&soap_, SOAP_C_UTFSTRING);
    m = soap_bind(&soap_, host_, port_, 100);

    if (m < 0)
    {
        sLog.outError("SoapThread: couldn't bind to %s:%d", host_, port_);
        return -1;
    }

    // check every 3 seconds if world ended
    soap_.accept_timeout = 3;
    soap_.recv_timeout = 5;
    soap_.send_timeout = 5;

    activate();
    return 0;
}


int SoapThread::svc()
{
    int s;
    sLog.outString("SOAP Thread started (listening on %s:%d)", host_, port_);
    while (!World::IsStopped())
    {
        s = soap_accept(&soap_);

        if (s < 0)
        {
            // ran into an accept timeout
            continue;
        }

        struct soap* thread_soap = soap_copy(&soap_);// make a safe copy

        ACE_Message_Block* mb = new ACE_Message_Block(sizeof(struct soap*));
        ACE_OS::memcpy(mb->wr_ptr(), &thread_soap, sizeof(struct soap*));
        pool_->putq(mb);
    }

    pool_->msg_queue()->deactivate();
    pool_->wait();

    soap_done(&soap_);
    sLog.outString("SOAP Thread stopped");
    return 0;
}


/*
Code used for generating stubs:

int ns1__executeCommand(char* command, char** result);
*/

int ns1__executeCommand(soap* soap, char* command, char** result)
{
    // security check
    if (!soap->userid || !soap->passwd)
    {
        DEBUG_LOG("MaNGOSsoap: Client didn't provide login information");
        return 401;
    }

    uint32 accountId = sAccountMgr.GetId(soap->userid);
    if (!accountId)
    {
        DEBUG_LOG("MaNGOSsoap: Client used invalid username '%s'", soap->userid);
        return 401;
    }

    if (!sAccountMgr.CheckPassword(accountId, soap->passwd))
    {
        DEBUG_LOG("MaNGOSsoap: invalid password for account '%s'", soap->userid);
        return 401;
    }

    if (sAccountMgr.GetSecurity(accountId) < SEC_ADMINISTRATOR)
    {
        DEBUG_LOG("MaNGOSsoap: %s's gmlevel is too low", soap->userid);
        return 403;
    }

    if (!command || !*command)
        { return soap_sender_fault(soap, "Command mustn't be empty", "The supplied command was an empty string"); }

    DEBUG_LOG("MaNGOSsoap: got command '%s'", command);
    SOAPCommand connection;

    // commands are executed in the world thread. We have to wait for them to be completed
    {
        // CliCommandHolder will be deleted from world, accessing after queueing is NOT save
        CliCommandHolder* cmd = new CliCommandHolder(accountId, SEC_CONSOLE, &connection, command, &SOAPCommand::print, &SOAPCommand::commandFinished);
        sWorld.QueueCliCommand(cmd);
    }

    ACE_OS::sleep(1);

    char* printBuffer = soap_strdup(soap, connection.m_printBuffer.c_str());
    if (connection.hasCommandSucceeded())
    {
        *result = printBuffer;
        return SOAP_OK;
    }
    else
        { return soap_sender_fault(soap, printBuffer, printBuffer); }
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
