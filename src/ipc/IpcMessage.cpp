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

#include "IpcMessage.h"

void IpcMessage::Encode(ByteBuffer& w) const
{
    w << IPC_PROTOCOL_VERSION << uint16(op) << uint32(body.size());
    w.append(body.contents(), body.size());
}

bool IpcMessage::Decode(ByteBuffer& w, IpcMessage& out, std::string& err)
{
    if (w.size() - w.rpos() < 8)
    {
        err = "short header";
        return false;
    }

    uint16 ver;
    uint16 op;
    uint32 len;
    size_t start = w.rpos();

    w >> ver >> op >> len;

    if (ver != IPC_PROTOCOL_VERSION)
    {
        w.rpos(start);
        err = "version mismatch";
        return false;
    }

    if (len > IPC_MAX_FRAME)
    {
        w.rpos(start);
        err = "oversize frame";
        return false;
    }

    if (w.size() - w.rpos() < len)
    {
        w.rpos(start);
        err = "incomplete";
        return false;
    }

    out.op = IpcOpcode(op);
    out.body.clear();

    if (len)
    {
        out.body.append(w.contents() + w.rpos(), len);
        w.rpos(w.rpos() + len);
    }

    return true;
}
