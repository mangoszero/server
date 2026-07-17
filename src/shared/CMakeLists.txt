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

include(TestBigEndian)

set(SRC_GRP_AUTH
  Auth/ARC4.cpp
  Auth/ARC4.h
  Auth/AuthCrypt.cpp
  Auth/AuthCrypt.h
  Auth/BigNumber.cpp
  Auth/BigNumber.h
  Auth/HMACSHA1.cpp
  Auth/HMACSHA1.h
  Auth/OpenSSLProvider.cpp
  Auth/OpenSSLProvider.h
  Auth/Sha1.cpp
  Auth/Sha1.h
  Auth/WardenKeyGeneration.h
  Auth/md5.c
  Auth/md5.h
)
source_group("Auth" FILES ${SRC_GRP_AUTH})

set(SRC_GRP_COMMON
  Common/Common.cpp
  Common/Common.h
  Common/ServerDefines.h
  Common/GitRevision.cpp
  Common/GitRevision.h
)
source_group("Common" FILES ${SRC_GRP_COMMON})

set(SRC_GRP_CONFIG
  Config/Config.cpp
  Config/Config.h
)
source_group("Config" FILES ${SRC_GRP_CONFIG})

set(SRC_GRP_DATASTORE
  DataStores/DBCFileLoader.cpp
  DataStores/DBCFileLoader.h
  DataStores/DBCStore.h
)
source_group("DataStores" FILES ${SRC_GRP_DATASTORE})

set(SRC_GRP_DATABASE
  Database/Database.cpp
  Database/Database.h
  Database/DatabaseEnv.h
  Database/DatabaseImpl.h
  Database/DatabaseMysql.cpp
  Database/DatabaseMysql.h
  Database/Field.cpp
  Database/Field.h
  Database/QueryResult.h
  Database/QueryResultMysql.cpp
  Database/QueryResultMysql.h
  Database/SQLStorage.cpp
  Database/SQLStorage.h
  Database/SQLStorageImpl.h
  Database/SqlDelayThread.cpp
  Database/SqlDelayThread.h
  Database/SqlOperations.cpp
  Database/SqlOperations.h
  Database/SqlPreparedStatement.cpp
  Database/SqlPreparedStatement.h
)
source_group("Database" FILES ${SRC_GRP_DATABASE})

set(SRC_GRP_DYNAMIC
  Dynamic/FactoryHolder.h
  Dynamic/ObjectRegistry.h
)
source_group("Dynamic" FILES ${SRC_GRP_DYNAMIC})

set(SRC_GRP_GAMESYSTEM
  GameSystem/Grid.h
  GameSystem/GridLoader.h
  GameSystem/GridRefManager.h
  GameSystem/GridReference.h
  GameSystem/NGrid.h
  GameSystem/TypeContainer.h
  GameSystem/TypeContainerVisitor.h
)
source_group("GameSystem" FILES ${SRC_GRP_GAMESYSTEM})

set(SRC_GRP_LOCKQ
  LockedQueue/LockedQueue.h
)
source_group("LockedQueue" FILES ${SRC_GRP_LOCKQ})

set(SRC_GRP_LOG
  Log/Log.cpp
  Log/Log.h
  Log/ConsoleLogWriter.cpp
  Log/ConsoleLogWriter.h
)
source_group("Log" FILES ${SRC_GRP_LOG})

set(SRC_GRP_THREAD
  Threading/ThreadLocalStore.h
  Threading/Threading.cpp
  Threading/Threading.h
)
source_group("Threading" FILES ${SRC_GRP_THREAD})

# Optional io_uring backend. Off by default: the epoll reactor is the supported Linux
# default, and liburing is not present on every distro CI builds on. When enabled it
# defines MANGOS_USE_IO_URING, which is what net/Server.hpp keys the backend choice on.
option(WITH_IO_URING "Build the io_uring networking backend (Linux only, needs liburing)" OFF)
set(MANGOS_USE_IO_URING OFF)
if(WITH_IO_URING AND UNIX AND NOT APPLE)
  find_library(URING_LIBRARY NAMES uring)
  find_path(URING_INCLUDE_DIR NAMES liburing.h)
  if(URING_LIBRARY AND URING_INCLUDE_DIR)
    set(MANGOS_USE_IO_URING ON)
    message(STATUS "Networking backend: io_uring (${URING_LIBRARY})")
  else()
    message(WARNING "WITH_IO_URING requested but liburing was not found; "
                    "falling back to the epoll reactor")
  endif()
endif()

# Shared networking engine. The backends self-collapse on the platforms they do not
# serve (IocpServer is #ifdef _WIN32, the reactor is #ifndef _WIN32), so they can all
# be listed unconditionally. io_uring is the exception: it needs liburing, so it is
# only compiled when the option above resolved.
set(SRC_GRP_NET
  net/BindAddress.cpp
  net/BindAddress.hpp
  net/FlowControl.hpp
  net/ISession.hpp
  net/SendQueue.hpp
  net/Server.hpp
  net/iocp/IocpServer.cpp
  net/iocp/IocpServer.hpp
  net/reactor/Connection.hpp
  net/reactor/EpollPoller.cpp
  net/reactor/EpollPoller.hpp
  net/reactor/KqueuePoller.cpp
  net/reactor/KqueuePoller.hpp
  net/reactor/Poller.hpp
  net/reactor/PollerFactory.cpp
  net/reactor/ReactorServer.cpp
  net/reactor/ReactorServer.hpp
)
if(MANGOS_USE_IO_URING)
  list(APPEND SRC_GRP_NET
    net/uring/UringServer.cpp
    net/uring/UringServer.hpp
  )
endif()
source_group("net" FILES ${SRC_GRP_NET})

set(SRC_GRP_PLATFORM
  Platform/CompilerDefs.h
  Platform/Define.h
)
source_group("Platform" FILES ${SRC_GRP_PLATFORM})

set(SRC_GRP_POLICIES
  Policies/Singleton.h
)
source_group("Policies" FILES ${SRC_GRP_POLICIES})

if(WIN32)
    set(SRC_GRP_SVC
      Win/ServiceWin32.cpp
      Win/ServiceWin32.h
      Win/WheatyExceptionReport.cpp
      Win/WheatyExceptionReport.h
    )
else()
    set(SRC_GRP_SVC
      Linux/PosixDaemon.cpp
      Linux/PosixDaemon.h
    )
endif()
source_group("Services" FILES ${SRC_GRP_SVC})

set(SRC_GRP_UTILITIES
  Utilities/ByteBuffer.cpp
  Utilities/ByteBuffer.h
  Utilities/ConsoleStyle.cpp
  Utilities/ConsoleStyle.h
  Utilities/Errors.h
  Utilities/ProgressBar.cpp
  Utilities/ProgressBar.h
  Utilities/ProgressBarRender.h
  Utilities/StartupUI.cpp
  Utilities/StartupUI.h
  Utilities/StartupUIDemo.cpp
  Utilities/RNGen.h
  Utilities/ScheduledExit.cpp
  Utilities/ScheduledExit.h
  Utilities/Timer.h
  Utilities/Util.cpp
  Utilities/Util.h
  Utilities/WorldPacket.h
  Utilities/ByteConverter.h
  Utilities/Callback.h
  Utilities/EventProcessor.cpp
  Utilities/EventProcessor.h
  Utilities/LinkedList.h
  Utilities/LinkedReference/RefManager.h
  Utilities/LinkedReference/Reference.h
  Utilities/TypeList.h
  Utilities/UnorderedMapSet.h
)
source_group("Utilities" FILES ${SRC_GRP_UTILITIES})

set(TGT_INCL
  Auth
  Common
  Config
  DataStores
  Database
  Linux
  LockedQueue
  Log
  Threading
  Utilities
  Utilities/LinkedReference
  Win
  Dynamic
  GameSystem
  Platform
  Policies
)

configure_file(revision_data.h.in ${CMAKE_CURRENT_BINARY_DIR}/revision_data.h)
configure_file(SystemConfig.h.in ${CMAKE_CURRENT_BINARY_DIR}/SystemConfig.h)

TEST_BIG_ENDIAN(ENDIAN_VALUE)

add_library(shared STATIC
    ${SRC_GRP_AUTH}
    ${SRC_GRP_COMMON}
    ${SRC_GRP_CONFIG}
    ${SRC_GRP_DATASTORE}
    ${SRC_GRP_DATABASE}
    ${SRC_GRP_DYNAMIC}
    ${SRC_GRP_GAMESYSTEM}
    ${SRC_GRP_LOCKQ}
    ${SRC_GRP_LOG}
    ${SRC_GRP_NET}
    ${SRC_GRP_THREAD}
    ${SRC_GRP_PLATFORM}
    ${SRC_GRP_POLICIES}
    ${SRC_GRP_SVC}
    ${SRC_GRP_UTILITIES}
    ${CMAKE_CURRENT_BINARY_DIR/revision_data.h}
    ${CMAKE_CURRENT_BINARY_DIR/SystemConfig.h}
)

target_include_directories(shared
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
        ${TGT_INCL}
)

target_compile_definitions(shared
    PUBLIC
        ${MANGOS_EXP}
        $<$<CONFIG:Debug>:MANGOS_DEBUG>
        MANGOS_ENDIAN=${ENDIAN_VALUE}
        $<$<BOOL:${ENDIAN_VALUE}>:ARCH_IS_BIG_ENDIAN>
        $<$<BOOL:${SCRIPT_LIB_ELUNA}>:ENABLE_ELUNA ELUNA_EXPANSION=0 ELUNA_MANGOS>
        # PUBLIC: net/Server.hpp is a header, so every consumer must agree on which
        # backend it selects, or they would disagree about what net::Server even is.
        $<$<BOOL:${MANGOS_USE_IO_URING}>:MANGOS_USE_IO_URING>
)

target_link_libraries(shared
    PUBLIC
        utf8
        MySQL::MySQL
        ${CMAKE_DL_LIBS}
        mangos_openssl
        $<$<BOOL:${WIN32}>:dbghelp>
        # Winsock + the AcceptEx/GetAcceptExSockaddrs extensions used by IocpServer.
        $<$<BOOL:${WIN32}>:ws2_32>
        $<$<BOOL:${WIN32}>:mswsock>
        $<$<BOOL:${MANGOS_USE_IO_URING}>:${URING_LIBRARY}>
    PRIVATE
        mangos_openssl_strict
)

if(MANGOS_USE_IO_URING)
    target_include_directories(shared PUBLIC ${URING_INCLUDE_DIR})
endif()
