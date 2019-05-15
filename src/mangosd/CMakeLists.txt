# MaNGOS is a full featured server for World of Warcraft, supporting
# the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
#
# Copyright (C) 2005-2019  MaNGOS project <https://getmangos.eu>
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


#Main Files
set(SRC_GRP_MAIN
  AFThread.cpp
  AFThread.h
  CliThread.cpp
  CliThread.h
  RAThread.cpp
  RAThread.h
  WorldThread.cpp
  WorldThread.h
  mangosd.cpp
)
source_group("Main" FILES ${SRC_GRP_MAIN})

set(SRC_GRP_SOAP
  SOAP/SoapThread.cpp
  SOAP/SoapThread.h
)
source_group("SOAP" FILES ${SRC_GRP_SOAP})

if(WIN32)
    set(PRODUCT_NAME "mangosd.exe")
    set(PRODUCT_ICON ${CMAKE_CURRENT_SOURCE_DIR}/mangosd.ico)
    set(PRODUCT_DESCRIPTION "${MANGOS_PKG} World Server")
    configure_file(${CMAKE_SOURCE_DIR}/cmake/win/VersionInfo.h.in ${CMAKE_CURRENT_BINARY_DIR}/VersionInfo.h)
endif()


add_executable(mangosd
    ${SRC_GRP_MAIN}
    $<$<BOOL:${SOAP}>:${SRC_GRP_SOAP}>
    $<$<BOOL:${WIN32}>:
        ${CMAKE_CURRENT_BINARY_DIR}/VersionInfo.h
        ${CMAKE_SOURCE_DIR}/cmake/win/VersionInfo.rc
    >
)

target_include_directories(mangosd
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(mangosd
    PUBLIC
        game
        $<$<BOOL:${SOAP}>:gsoap>
        Threads::Threads
        DL::DL
)

install(
    TARGETS mangosd
    DESTINATION ${BIN_DIR}
)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/mangosd.conf.dist.in ${CMAKE_CURRENT_BINARY_DIR}/mangosd.conf.dist)
install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/mangosd.conf.dist
    DESTINATION ${CONF_INSTALL_DIR}
)

if(WIN32 AND MSVC)
    install(
        FILES $<TARGET_PDB_FILE:mangosd>
        DESTINATION ${BIN_DIR}
        OPTIONAL
        )
endif()
