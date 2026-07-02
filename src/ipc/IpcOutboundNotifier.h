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
 */

#ifndef AH_IPC_OUTBOUND_NOTIFIER_H
#define AH_IPC_OUTBOUND_NOTIFIER_H

#include "IpcLink.h"
#include "IpcMessage.h"

#include <ace/Event_Handler.h>
#include <ace/Reactor.h>

/**
 * @brief Reactor-thread drain hook for a side's outbound queue.
 *
 * Registered with the reactor; the caller thread wakes it via
 * ACE_Reactor::notify(this). ACE then invokes handle_exception() ON THE
 * REACTOR THREAD, where it is safe to touch the live handler. handle_exception
 * drains the link's outbound queue and forwards each frame to the live handler.
 *
 * @tparam LinkT     IpcServerLink or IpcClientLink.
 * @tparam HandlerT  IpcServerHandler or IpcClientHandler (LinkT::handler type).
 */
template<class LinkT, class HandlerT>
class IpcOutboundNotifier : public ACE_Event_Handler
{
    public:
        explicit IpcOutboundNotifier(LinkT* link)
            : m_link(link)
        {
            m_link->AddRef();
        }

        ~IpcOutboundNotifier() override
        {
            if (m_link)
            {
                m_link->Release();
                m_link = nullptr;
            }
        }

        /**
         * @brief Runs on the reactor thread (via ACE_Reactor::notify).
         *
         * Drains the outbound queue and forwards each frame to the live
         * handler. Because this executes on the reactor thread, the handler
         * pointer cannot be freed underneath us (handle_close() also runs on
         * this thread and is serialised against us).
         */
        int handle_exception(ACE_HANDLE /*fd*/ = ACE_INVALID_HANDLE) override
        {
            if (!m_link)
            {
                return 0;
            }

            HandlerT* h = m_link->handler; // reactor-thread-only read - safe.

            IpcMessage msg;
            while (m_link->outbound.pop(msg))
            {
                if (h)
                {
                    // Best-effort: a send failure here means the handler is
                    // closing; the frame is dropped (mirrors a dropped queue).
                    h->SendFrame(msg);
                }
                // If no live handler, frames are silently discarded - the
                // caller checks Connected() before sending in normal use.
            }

            return 0;
        }

    private:
        LinkT* m_link;
};

#endif // AH_IPC_OUTBOUND_NOTIFIER_H
