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
)
source_group("Log" FILES ${SRC_GRP_LOG})

set(SRC_GRP_THREAD
  Threading/DelayExecutor.cpp
  Threading/DelayExecutor.h
  Threading/Threading.cpp
  Threading/Threading.h
)
source_group("Threading" FILES ${SRC_GRP_THREAD})

set(SRC_GRP_PLATFORM
  Platform/CompilerDefs.h
  Platform/Define.h
)
source_group("Platform" FILES ${SRC_GRP_PLATFORM})

set(SRC_GRP_POLICIES
  Policies/CreationPolicy.h
  Policies/ObjectLifeTime.cpp
  Policies/ObjectLifeTime.h
  Policies/Singleton.h
  Policies/ThreadingModel.h
)
source_group("Policies" FILES ${SRC_GRP_POLICIES})

if(WIN32)
    set(SRC_GRP_SVC
      Win/ServiceWin32.cpp
      Win/ServiceWin32.h
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
  Utilities/Errors.h
  Utilities/ProgressBar.cpp
  Utilities/ProgressBar.h
  Utilities/RNGen.h
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
    ${SRC_GRP_THREAD}
    ${SRC_GRP_PLATFORM}
    ${SRC_GRP_POLICIES}
    ${SRC_GRP_SVC}
    ${SRC_GRP_UTILITIES}
    revision.h
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
)

target_link_libraries(shared
    PUBLIC
        ace
        utf8
        MySQL::MySQL
        OpenSSL::Crypto
)
