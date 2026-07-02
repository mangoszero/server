# Differential drift guard between the worker's ah_usability port and the
# production AhUsabilityRef evaluator. Separate EXCLUDE_FROM_ALL executable: it
# is NEVER linked into the shipped mangosd (the worker-private Usability type
# must not reach production). Built only when BOTH the worker and the game-side
# reference source are available.
#
# AhUsabilityRef.cpp is compiled DIRECTLY as a source here — NOT by linking
# the game library. This is intentional: if AhUsabilityRef.cpp ever gains a
# dependency on a game header it would fail to compile here, immediately
# surfacing the boundary violation.
add_executable(ah_usability_difftest EXCLUDE_FROM_ALL
    "${CMAKE_CURRENT_SOURCE_DIR}/AhUsabilityDiffTest.cpp"
    "${CMAKE_SOURCE_DIR}/src/game/Object/AhUsabilityRef.cpp")

target_include_directories(ah_usability_difftest PRIVATE
    "${CMAKE_SOURCE_DIR}/src/game")   # for "Object/AhUsabilityRef.h"

target_link_libraries(ah_usability_difftest PRIVATE ah_usability)
# ah_usability pulls in ah_ipc (which provides Common.h / ByteBuffer.h /
# BrowseMessages.h). AhUsabilityRef.cpp must compile with only Common.h; if it
# ever needs a pure-game header the difftest will fail to build — that is the
# intended boundary signal, not something to paper over by linking game.
