set(REQUIRED_FILES
  "${SOURCE_ROOT}/src/game/Server/WorldGateway.h"
  "${SOURCE_ROOT}/src/game/Server/WorldGateway.cpp"
  "${SOURCE_ROOT}/src/game/Server/SessionMailbox.h"
  "${SOURCE_ROOT}/src/game/Server/SessionMailbox.cpp")

if(EXPECT_LEGACY_REMOVED)
  list(APPEND REQUIRED_FILES
    "${SOURCE_ROOT}/src/game/Server/WorldNetwork.h"
    "${SOURCE_ROOT}/src/game/Server/WorldNetwork.cpp")
endif()

foreach(FILE_PATH IN LISTS REQUIRED_FILES)
  if(NOT EXISTS "${FILE_PATH}")
    message(FATAL_ERROR "Required decoupling file is missing: ${FILE_PATH}")
  endif()
endforeach()

if(EXPECT_LEGACY_REMOVED)
  string(CONCAT OLD_SOCKET_NAME "World" "Socket")
  string(CONCAT OLD_SOCKET_MANAGER_NAME "World" "Socket" "Mgr")
  string(CONCAT OLD_LEASE_NAME "Leased" "Ptr")
  set(REMOVED_FILES
    "${SOURCE_ROOT}/src/game/Server/${OLD_SOCKET_NAME}.h"
    "${SOURCE_ROOT}/src/game/Server/${OLD_SOCKET_NAME}.cpp"
    "${SOURCE_ROOT}/src/game/Server/${OLD_SOCKET_MANAGER_NAME}.h"
    "${SOURCE_ROOT}/src/game/Server/${OLD_SOCKET_MANAGER_NAME}.cpp"
    "${SOURCE_ROOT}/src/shared/Threading/${OLD_LEASE_NAME}.h"
    "${SOURCE_ROOT}/tests/LeaseTests.cpp")
  foreach(FILE_PATH IN LISTS REMOVED_FILES)
    if(EXISTS "${FILE_PATH}")
      message(FATAL_ERROR "Obsolete coupled file still exists: ${FILE_PATH}")
    endif()
  endforeach()
endif()

foreach(TRANSPORT_DIR IN ITEMS
    "${SOURCE_ROOT}/src/shared/net/iocp"
    "${SOURCE_ROOT}/src/shared/net/reactor"
    "${SOURCE_ROOT}/src/shared/net/uring")
  file(GLOB_RECURSE TRANSPORT_SOURCES
    "${TRANSPORT_DIR}/*.h" "${TRANSPORT_DIR}/*.hpp" "${TRANSPORT_DIR}/*.cpp")
  foreach(FILE_PATH IN LISTS TRANSPORT_SOURCES)
    file(READ "${FILE_PATH}" CONTENTS)
    if(CONTENTS MATCHES "#[ \t]*include[ \t]*[\"<](World|WorldSession|Opcode|Database|Addon|Warden)")
      message(FATAL_ERROR "Transport gained game dependency: ${FILE_PATH}")
    endif()
  endforeach()
endforeach()

file(READ "${SOURCE_ROOT}/src/game/Server/WorldGateway.cpp" WORLD_GATEWAY_SOURCE)
foreach(REQUIRED_TRANSACTION_STEP IN ITEMS
    "WorldSession* const publishedSession = session.release()"
    "session.reset(publishedSession)"
    "Detach(sessionId)")
  string(FIND "${WORLD_GATEWAY_SOURCE}" "${REQUIRED_TRANSACTION_STEP}" STEP_POSITION)
  if(STEP_POSITION EQUAL -1)
    message(FATAL_ERROR
      "WorldGateway session publication is missing rollback step: ${REQUIRED_TRANSACTION_STEP}")
  endif()
endforeach()

foreach(REQUIRED_ALLOCATOR_STEP IN ITEMS
    "sessionId = ++m_nextSessionId"
    "sessionId == proto::INVALID_SESSION_ID"
    "m_routes.find(sessionId) != m_routes.end()")
  string(FIND "${WORLD_GATEWAY_SOURCE}" "${REQUIRED_ALLOCATOR_STEP}" ALLOCATOR_POSITION)
  if(ALLOCATOR_POSITION EQUAL -1)
    message(FATAL_ERROR
      "WorldGateway monotonic route allocation is missing: ${REQUIRED_ALLOCATOR_STEP}")
  endif()
endforeach()
if(WORLD_GATEWAY_SOURCE MATCHES "m_nextSessionId[ \t]*=")
  message(FATAL_ERROR "WorldGateway must not recycle detached route IDs")
endif()

string(FIND "${WORLD_GATEWAY_SOURCE}" "link->SendPacket(addonResponse)" EARLY_ADDON_SEND)
if(NOT EARLY_ADDON_SEND EQUAL -1)
  message(FATAL_ERROR "WorldGateway sends addon info before the world-thread auth response")
endif()

file(READ "${SOURCE_ROOT}/src/game/Server/SessionMailbox.cpp" SESSION_MAILBOX_SOURCE)
foreach(REQUIRED_DRAIN_STEP IN ITEMS
    "while (m_packets.next(packet))"
    "delete packet")
  string(FIND "${SESSION_MAILBOX_SOURCE}" "${REQUIRED_DRAIN_STEP}" DRAIN_POSITION)
  if(DRAIN_POSITION EQUAL -1)
    message(FATAL_ERROR
      "SessionMailbox::Close is missing residual ownership cleanup: ${REQUIRED_DRAIN_STEP}")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" WORLD_SESSION_SOURCE)
string(FIND "${WORLD_SESSION_SOURCE}" "m_mailbox->Close()" MAILBOX_CLOSE)
string(FIND "${WORLD_SESSION_SOURCE}" "m_link->Close()" LINK_CLOSE)
if(MAILBOX_CLOSE EQUAL -1 OR LINK_CLOSE EQUAL -1 OR NOT MAILBOX_CLOSE LESS LINK_CLOSE)
  message(FATAL_ERROR "WorldSession must close its mailbox before its client link")
endif()

foreach(REQUIRED_SESSION_BEHAVIOR IN ITEMS
    "void WorldSession::HandlePingOpcode"
    "void WorldSession::HandleKeepAliveOpcode"
    "std::chrono::steady_clock::now()"
    "packet->GetOpcode() != CMSG_PING"
    "packet->GetOpcode() != CMSG_KEEP_ALIVE")
  string(FIND "${WORLD_SESSION_SOURCE}" "${REQUIRED_SESSION_BEHAVIOR}" POSITION)
  if(POSITION EQUAL -1)
    message(FATAL_ERROR
      "WorldSession is missing queued network behavior: ${REQUIRED_SESSION_BEHAVIOR}")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/src/game/Server/OpcodeTable.cpp" OPCODE_TABLE_SOURCE)
foreach(REQUIRED_OPCODE_ROUTE IN ITEMS
    "OPCODE(CMSG_PING,                                      STATUS_AUTHED,   PROCESS_THREADUNSAFE, &WorldSession::HandlePingOpcode)"
    "OPCODE(CMSG_KEEP_ALIVE,                                STATUS_AUTHED,   PROCESS_THREADUNSAFE, &WorldSession::HandleKeepAliveOpcode)")
  string(FIND "${OPCODE_TABLE_SOURCE}" "${REQUIRED_OPCODE_ROUTE}" OPCODE_POSITION)
  if(OPCODE_POSITION EQUAL -1)
    message(FATAL_ERROR
      "World-thread protocol opcode registration is missing: ${REQUIRED_OPCODE_ROUTE}")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/World.cpp" WORLD_SOURCE)
foreach(REQUIRED_AUTH_ORDER IN ITEMS
    "AddQueuedSession(s);\n        s->SendPendingAddonInfo();"
    "s->SendPacket(&packet);\n    s->SendPendingAddonInfo();")
  string(FIND "${WORLD_SOURCE}" "${REQUIRED_AUTH_ORDER}" AUTH_ORDER_POSITION)
  if(AUTH_ORDER_POSITION EQUAL -1)
    message(FATAL_ERROR
      "World-thread auth/addon ordering is missing: ${REQUIRED_AUTH_ORDER}")
  endif()
endforeach()

foreach(REQUIRED_ACCEPTED_AUTH_FIELD IN ITEMS
    "WorldPacket packet(SMSG_AUTH_RESPONSE, 1 + 4 + 1 + 4)"
    "packet << uint8(AUTH_OK)"
    "packet << uint32(0)"
    "packet << uint8(0)")
  string(FIND "${WORLD_SOURCE}" "${REQUIRED_ACCEPTED_AUTH_FIELD}" FIELD_POSITION)
  if(FIELD_POSITION EQUAL -1)
    message(FATAL_ERROR
      "Zero accepted authentication response is incomplete: ${REQUIRED_ACCEPTED_AUTH_FIELD}")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/WorldSessionMgr.cpp" SESSION_MGR_SOURCE)
foreach(REQUIRED_QUEUE_AUTH_STEP IN ITEMS
    "WorldPacket packet(SMSG_AUTH_RESPONSE, 1 + 4 + 1 + 4 + 4)"
    "packet << uint8(AUTH_WAIT_QUEUE)"
    "packet << uint32(GetQueuedSessionPos(sess))"
    "pop_sess->SendAuthWaitQue(0)")
  string(FIND "${SESSION_MGR_SOURCE}" "${REQUIRED_QUEUE_AUTH_STEP}" QUEUE_POSITION)
  if(QUEUE_POSITION EQUAL -1)
    message(FATAL_ERROR
      "Zero queued authentication flow is incomplete: ${REQUIRED_QUEUE_AUTH_STEP}")
  endif()
endforeach()

file(GLOB PROTO_SOURCES
  "${SOURCE_ROOT}/src/proto/*.h" "${SOURCE_ROOT}/src/proto/*.cpp")
foreach(FILE_PATH IN LISTS PROTO_SOURCES)
  file(READ "${FILE_PATH}" CONTENTS)
  if(CONTENTS MATCHES "WorldSession[ \t]*\\*")
    message(FATAL_ERROR "Protocol layer contains a WorldSession pointer: ${FILE_PATH}")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/src/proto/ClientConnection.cpp" CLIENT_CONNECTION_SOURCE)
foreach(REQUIRED_SEND_STEP IN ITEMS
    "std::lock_guard<std::mutex> guard(m_sendOrderLock)"
    "m_sender(frame.data(), frame.size())")
  string(FIND "${CLIENT_CONNECTION_SOURCE}" "${REQUIRED_SEND_STEP}" SEND_POSITION)
  if(SEND_POSITION EQUAL -1)
    message(FATAL_ERROR
      "ClientConnection send ordering is missing: ${REQUIRED_SEND_STEP}")
  endif()
endforeach()

foreach(REQUIRED_SESSION_SNAPSHOT_STEP IN ITEMS
    "SessionId ClientConnection::CurrentSession()"
    "SessionId const session = CurrentSession()"
    "m_gateway.Deliver(session, std::move(packet))")
  string(FIND "${CLIENT_CONNECTION_SOURCE}" "${REQUIRED_SESSION_SNAPSHOT_STEP}" SNAPSHOT_POSITION)
  if(SNAPSHOT_POSITION EQUAL -1)
    message(FATAL_ERROR
      "ClientConnection delivery is missing a locked session snapshot: ${REQUIRED_SESSION_SNAPSHOT_STEP}")
  endif()
endforeach()
