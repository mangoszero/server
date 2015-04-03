#-----------------------------------------------------------------------------
# Build the mangos script library if wished for
if(SCRIPT_LIB_SD2)
    add_subdirectory(SD2)
endif()

#-----------------------------------------------------------------------------
# Build the Eluna library if wished for
if(SCRIPT_LIB_ELUNA)
    add_subdirectory(Eluna)
endif()

#-----------------------------------------------------------------------------
# Include Player Bots if wished for
if(PLAYERBOTS)
    add_subdirectory(Bots)
endif()