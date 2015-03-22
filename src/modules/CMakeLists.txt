#-----------------------------------------------------------------------------
# Build the mangos-zero script library if wished for
if(SCRIPT_LIB_SD2)
    add_subdirectory(SD2)
endif()

#-----------------------------------------------------------------------------
# Build the Eluna library if wished for
if(SCRIPT_LIB_ELUNA)
    add_subdirectory(Eluna)
endif()