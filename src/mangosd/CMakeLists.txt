# This code is part of MaNGOS. Contributor & Copyright details are in AUTHORS/THANKS.
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

set(EXECUTABLE_NAME mangosd)

set(EXECUTABLE_SRCS
    CliRunnable.cpp
    CliRunnable.h
    Main.cpp
    Master.cpp
    Master.h
    RASocket.cpp
    RASocket.h
    WorldRunnable.cpp
    WorldRunnable.h
)

if(SOAP)
    LIST(APPEND EXECUTABLE_SRCS
        MaNGOSsoap.cpp
        MaNGOSsoap.h
        soapC.cpp
        soapH.h
        soapServer.cpp
        soapStub.h
    )
endif()

if(WIN32)
    # add resource file to windows build
    set(EXECUTABLE_SRCS ${EXECUTABLE_SRCS} mangosd.rc)
endif()

include_directories(
    "${CMAKE_SOURCE_DIR}/src/shared"
    "${CMAKE_SOURCE_DIR}/src/framework"
    "${CMAKE_SOURCE_DIR}/src/game/AuctionHouseBot"
    "${CMAKE_SOURCE_DIR}/src/game/BattleGround"
    "${CMAKE_SOURCE_DIR}/src/game/Server"
    "${CMAKE_SOURCE_DIR}/src/game/WorldHandlers"
    "${CMAKE_SOURCE_DIR}/src/game/Object"
    "${CMAKE_SOURCE_DIR}/src/game/Tools"
    "${CMAKE_SOURCE_DIR}/src/game/References"
    "${CMAKE_SOURCE_DIR}/src/game/MotionGenerators"
    "${CMAKE_SOURCE_DIR}/src/game/vmap"
    "${CMAKE_BINARY_DIR}"
    "${CMAKE_BINARY_DIR}/src/shared"
    "${ACE_INCLUDE_DIR}"
    "${OPENSSL_INCLUDE_DIR}"
)

if(SCRIPT_LIB_ELUNA)
    include_directories(${CMAKE_SOURCE_DIR}/src/modules/Eluna)
    include_directories(${CMAKE_SOURCE_DIR}/dep/lualib)
endif()

if(SCRIPT_LIB_SD2)
    include_directories(${CMAKE_SOURCE_DIR}/src/modules/SD2)
endif()

if(SOAP)
    include_directories("${CMAKE_SOURCE_DIR}/dep/include/gsoap")
endif()

if(POSTGRESQL)
  include_directories("${PGSQL_INCLUDE_DIR}")
else()
  include_directories("${MYSQL_INCLUDE_DIR}")
endif()

add_executable(${EXECUTABLE_NAME}
  ${EXECUTABLE_SRCS}
)

if(NOT ACE_USE_EXTERNAL)
  add_dependencies(${EXECUTABLE_NAME} ace)
  target_link_libraries(${EXECUTABLE_NAME} ace)
else()
  target_link_libraries(${EXECUTABLE_NAME} ACE)
endif()

target_link_libraries(${EXECUTABLE_NAME}
    game
    shared
    framework
    g3dlite
)

add_dependencies(${EXECUTABLE_NAME} game)

if(SOAP)
    target_link_libraries(${EXECUTABLE_NAME}
        gsoap
    )
endif()

if(WIN32)
  target_link_libraries(${EXECUTABLE_NAME}
    zlib
    optimized ${MYSQL_LIBRARY}
    optimized ${OPENSSL_LIBRARIES}
    debug ${MYSQL_DEBUG_LIBRARY}
    debug ${OPENSSL_DEBUG_LIBRARIES}
  )
  if(PLATFORM MATCHES X86)
    target_link_libraries(${EXECUTABLE_NAME})
  endif()
endif()

if(UNIX)
  target_link_libraries(${EXECUTABLE_NAME}
    ${MYSQL_LIBRARY}
    ${OPENSSL_LIBRARIES}
    ${OPENSSL_EXTRA_LIBRARIES}
    ${ZLIB_LIBRARIES}
  )
endif()

set(EXECUTABLE_LINK_FLAGS "")

if(UNIX)
  if(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
    set(EXECUTABLE_LINK_FLAGS "-Wl,--no-as-needed -pthread -lrt ${EXECUTABLE_LINK_FLAGS} -rdynamic")
  elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(EXECUTABLE_LINK_FLAGS "-Wl,--no-as-needed -ldl -pthread -lrt ${EXECUTABLE_LINK_FLAGS} -rdynamic")
  endif()
endif()

if(APPLE)
  set(EXECUTABLE_LINK_FLAGS "-framework Carbon ${EXECUTABLE_LINK_FLAGS}")
endif()

set_target_properties(${EXECUTABLE_NAME} PROPERTIES LINK_FLAGS
  "${EXECUTABLE_LINK_FLAGS}"
)

install(TARGETS ${EXECUTABLE_NAME} DESTINATION "${BIN_DIR}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/mangosd.conf.dist.in" "${CMAKE_CURRENT_BINARY_DIR}/mangosd.conf.dist")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/mangosd.conf.dist" DESTINATION "${CONF_INSTALL_DIR}")

if(WIN32 AND MSVC)
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/\${BUILD_TYPE}/${EXECUTABLE_NAME}.pdb" DESTINATION "${BIN_DIR}" CONFIGURATIONS Debug)
  add_custom_command(TARGET ${EXECUTABLE_NAME}
    POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_BINARY_DIR}/mangosd.conf.dist" "${CONF_COPY_DIR}"
  )
endif()

#Copy dlls to build dir
if(WIN32)
  if(MSVC)
   add_custom_command(TARGET ${EXECUTABLE_NAME}
     POST_BUILD
     COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/dep/lib/${DEP_ARCH}/libeay32.dll" "${CMAKE_BINARY_DIR}/bin/$(Configuration)/"
     COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/dep/lib/${DEP_ARCH}/ssleay32.dll" "${CMAKE_BINARY_DIR}/bin/$(Configuration)/"
     COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/dep/lib/${DEP_ARCH}/libmySQL.dll" "${CMAKE_BINARY_DIR}/bin/$(Configuration)/"
   )
  elseif(XCODE)
   add_custom_command(TARGET ${EXECUTABLE_NAME}
     POST_BUILD
     COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/dep/lib/${DEP_ARCH}/libeay32.dll" "${CMAKE_BINARY_DIR}/bin/"
     COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/dep/lib/${DEP_ARCH}/ssleay32.dll" "${CMAKE_BINARY_DIR}/bin/"
     COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/dep/lib/${DEP_ARCH}/libmySQL.dll" "${CMAKE_BINARY_DIR}/bin/"
   )
  endif()
endif()
