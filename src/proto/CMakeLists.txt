# MaNGOS is a full featured server for World of Warcraft, supporting
# the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
#
# Copyright (C) 2005-2025 MaNGOS <https://www.getmangos.eu>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

# =============================================================================
# proto -- the 1.12.x world protocol
#
# Framing, the header cipher, the login proof, and nothing else. It talks to the
# world exclusively through IWorldGateway.
#
# The link list below is the boundary. `proto` does NOT link `game`, and it does
# NOT link the database: if someone reaches for sWorld, sObjectMgr or
# LoginDatabase from in here, the build fails rather than quietly re-tangling the
# two layers. That is the whole point of the library existing -- a naming
# convention would not have survived contact.
# =============================================================================

set(SRC_GRP_PROTO
    ClientConnection.cpp
    ClientConnection.h
    IClientLink.h
    IWorldGateway.h
    Listener.cpp
    Listener.h
    Opcodes.h
    WorldPacket.h
    PacketCodec.cpp
    PacketCodec.h
)
source_group("proto" FILES ${SRC_GRP_PROTO})

add_library(proto STATIC ${SRC_GRP_PROTO})

target_include_directories(proto
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(proto
    PUBLIC
        shared
    PRIVATE
        mangos_openssl_strict
)
