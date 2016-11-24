#
# Find the MySQL client includes and library
#

# This module defines
# MYSQL_INCLUDE_DIR, where to find mysql.h
# MYSQL_LIBRARY, the libraries needed to use MySQL.
# MYSQL_FOUND, If false, do not try to use MySQL.

if(MYSQL_INCLUDE_DIR AND MYSQL_LIBRARY)
  set(MYSQL_FOUND TRUE)
else(MYSQL_INCLUDE_DIR AND MYSQL_LIBRARY)

  SET(PROGRAM_FILES_ARCH_PATH)
  if(PLATFORM EQUAL 32)
    SET(PROGRAM_FILES_ARCH_PATH $ENV{ProgramFiles})
  elseif(PLATFORM EQUAL 64)
    SET(PROGRAM_FILES_ARCH_PATH $ENV{ProgramW6432})
  endif()

  if (${PROGRAM_FILES_ARCH_PATH})
    STRING(REPLACE "\\\\" "/" PROGRAM_FILES_ARCH_PATH ${PROGRAM_FILES_ARCH_PATH})
  endif(${PROGRAM_FILES_ARCH_PATH})

  find_path(MYSQL_INCLUDE_DIR mysql.h
    /usr/include
    /usr/include/mysql
    /usr/local/include
    /usr/local/include/mysql
    /usr/local/mysql/include
    /opt/local/include/mysql*/mysql
    "${PROGRAM_FILES_ARCH_PATH}/MySQL/MySQL Server 5.0/include"
    "${PROGRAM_FILES_ARCH_PATH}/MySQL/MySQL Server 5.1/include"
    "${PROGRAM_FILES_ARCH_PATH}/MySQL/MySQL Server 5.2/include"
    "${PROGRAM_FILES_ARCH_PATH}/MySQL/MySQL Server 5.3/include"
    "${PROGRAM_FILES_ARCH_PATH}/MySQL/MySQL Server 5.4/include"
    "${PROGRAM_FILES_ARCH_PATH}/MySQL/MySQL Server 5.5/include"
    "${PROGRAM_FILES_ARCH_PATH}/MySQL/MySQL Server 5.6/include"
    "${PROGRAM_FILES_ARCH_PATH}/MySQL/MySQL Server 5.7/include"
  )

  if(WIN32 AND MSVC)
    find_library(MYSQL_LIBRARY
      NAMES
        libmysql
      PATHS
        "${PROGRAM_FILES_ARCH_PATH}/MySQL/MySQL Server 5.0/lib/opt"
        "${PROGRAM_FILES_ARCH_PATH}/MySQL/MySQL Server 5.1/lib/opt"
        "${PROGRAM_FILES_ARCH_PATH}/MySQL/MySQL Server 5.2/lib"
        "${PROGRAM_FILES_ARCH_PATH}/MySQL/MySQL Server 5.3/lib"
        "${PROGRAM_FILES_ARCH_PATH}/MySQL/MySQL Server 5.4/lib"
        "${PROGRAM_FILES_ARCH_PATH}/MySQL/MySQL Server 5.5/lib"
        "${PROGRAM_FILES_ARCH_PATH}/MySQL/MySQL Server 5.6/lib"
        "${PROGRAM_FILES_ARCH_PATH}/MySQL/MySQL Server 5.7/lib"
      )
  else(WIN32 AND MSVC)
    find_library(MYSQL_LIBRARY
      NAMES
        mysqlclient
        mysqlclient_r
        mysql
        libmysql
      PATHS
        /usr/lib/mysql
        /usr/local/lib/mysql
        /usr/local/mysql/lib
        /opt/local/lib/mysql*/mysql
      )
  endif(WIN32 AND MSVC)

  if(MYSQL_INCLUDE_DIR AND MYSQL_LIBRARY)
    set(MYSQL_FOUND TRUE)
    message(STATUS "Found MySQL library: ${MYSQL_LIBRARY}")
    message(STATUS "Found MySQL headers: ${MYSQL_INCLUDE_DIR}")
  else(MYSQL_INCLUDE_DIR AND MYSQL_LIBRARY)
    set(MYSQL_FOUND FALSE)
    message(FATAL_ERROR "Could not find ${PLATFORM}-bit MySQL headers or libraries! Please install the development libraries and headers.")
  endif(MYSQL_INCLUDE_DIR AND MYSQL_LIBRARY)

  mark_as_advanced(MYSQL_INCLUDE_DIR MYSQL_LIBRARY)

endif(MYSQL_INCLUDE_DIR AND MYSQL_LIBRARY)
