if(NOT IS_DIRECTORY "${PROTO_DIR}")
  message(FATAL_ERROR "Protocol boundary missing: ${PROTO_DIR}")
endif()

file(GLOB PROTO_SOURCES
  "${PROTO_DIR}/*.h" "${PROTO_DIR}/*.hpp"
  "${PROTO_DIR}/*.cpp" "${PROTO_DIR}/*.cc")

set(FORBIDDEN_PATTERNS
  "#[ \t]*include[ \t]*[\"<](Database/|World\\.h|WorldSession\\.h|AddonHandler\\.h|LuaEngine\\.h|Warden)"
  "(^|[^A-Za-z0-9_])(WorldSession|sWorld|LoginDatabase|CharacterDatabase|WorldDatabase|sAddOnHandler|LuaEngine|Warden)([^A-Za-z0-9_]|$)")

foreach(FILE_PATH IN LISTS PROTO_SOURCES)
  file(READ "${FILE_PATH}" CONTENTS)
  foreach(PATTERN IN LISTS FORBIDDEN_PATTERNS)
    if(CONTENTS MATCHES "${PATTERN}")
      message(FATAL_ERROR "Forbidden protocol dependency in ${FILE_PATH}: ${PATTERN}")
    endif()
  endforeach()
endforeach()
