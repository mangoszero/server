# MaNGOS is a full featured server for World of Warcraft, supporting
# the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
#
# Copyright (C) 2005-2015  MaNGOS project <http://getmangos.eu>
# Copyright (C) 2006-2013  ScriptDev2 project <http://www.scriptdev2.com>
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

include(MacroMangosSourceGroup)

#Add PCH Files
set(mangosscript_PCH "${CMAKE_CURRENT_SOURCE_DIR}/precompiled.h")
if(PCH)
    LIST(APPEND mangosscript_PCH "${CMAKE_CURRENT_SOURCE_DIR}/precompiled.cpp")
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/base
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/shared
    ${CMAKE_SOURCE_DIR}/src/framework
    ${CMAKE_SOURCE_DIR}/src/game/Server
    ${CMAKE_SOURCE_DIR}/src/game/BattleGround
    ${CMAKE_SOURCE_DIR}/src/game/WorldHandlers
    ${CMAKE_SOURCE_DIR}/src/game/Object
    ${CMAKE_SOURCE_DIR}/src/game/References
    ${CMAKE_SOURCE_DIR}/src/game/MotionGenerators
    ${CMAKE_SOURCE_DIR}/src/game/vmap
    ${CMAKE_SOURCE_DIR}/src/shared
    ${CMAKE_SOURCE_DIR}/src/shared/Utilities
    ${CMAKE_SOURCE_DIR}/src/shared/Log
    ${CMAKE_SOURCE_DIR}/src/shared/DataStores
    ${CMAKE_SOURCE_DIR}/src/shared/Common
    ${CMAKE_BINARY_DIR}
    ${CMAKE_BINARY_DIR}/src/shared
    ${ACE_INCLUDE_DIR}
    ${MYSQL_INCLUDE_DIR}
)

# Define the SD2 library
set(mangosscript_LIB_SRCS ${mangosscript_PCH})

#Include Files
file(GLOB sources_include include/*.cpp include/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_include})
    source_group("Include Files" FILES ${sources_include})

#System Files
file(GLOB sources_system system/*.cpp system/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_system})
    source_group("System Files" FILES ${sources_system})

#Base Files
file(GLOB sources_base base/*.cpp base/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_base})
    source_group("Base Files" FILES ${sources_base})

#Battleground Scripts
file(GLOB sources_battlegrounds scripts/battlegrounds/*.cpp scripts/battlegrounds/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_battlegrounds})
    source_group("Battleground Scripts" FILES ${sources_battlegrounds})

#World Scripts
file(GLOB sources_world scripts/world/*.cpp scripts/world/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_world})
    source_group("World Scripts" FILES ${sources_world})

#Eastern Kingdom Scripts
file(GLOB sources_eastern_kingdoms scripts/eastern_kingdoms/*.cpp scripts/eastern_kingdoms/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_eastern_kingdoms})
    source_group("Eastern Kingdoms Scripts" FILES ${sources_eastern_kingdoms})

#Kalimdor Scripts
file(GLOB sources_kalimdor scripts/kalimdor/*.cpp scripts/kalimdor/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_kalimdor})
    source_group("Kalimdor Scripts" FILES ${sources_kalimdor})

#Instance: Blackrock Depths Scripts
file(GLOB sources_instance_ek_brd scripts/eastern_kingdoms/blackrock_mountain/blackrock_depths/*.cpp scripts/eastern_kingdoms/blackrock_mountain/blackrock_depths/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_instance_ek_brd})
    source_group("Eastern Kingdoms Scripts\\Instances\\Blackrock Depths" FILES ${sources_instance_ek_brd})

#Instance: Blackrock Spire Scripts
file(GLOB sources_instance_ek_brs scripts/eastern_kingdoms/blackrock_mountain/blackrock_spire/*.cpp scripts/eastern_kingdoms/blackrock_mountain/blackrock_spire/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_instance_ek_brs})
    source_group("Eastern Kingdoms Scripts\\Instances\\Blackrock Spire" FILES ${sources_instance_ek_brs})

#Instance: Deadmines Scripts
file(GLOB sources_instance_ek_tdm scripts/eastern_kingdoms/deadmines/*.cpp scripts/eastern_kingdoms/deadmines/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_instance_ek_tdm})
    source_group("Eastern Kingdoms Scripts\\Instances\\Deadmines" FILES ${sources_instance_ek_tdm})

#Instance: Gnomeregan Scripts
file(GLOB sources_instance_ek_gno scripts/eastern_kingdoms/gnomeregan/*.cpp scripts/eastern_kingdoms/gnomeregan/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_instance_ek_gno})
    source_group("Eastern Kingdoms Scripts\\Instances\\Gnomeregan" FILES ${sources_instance_ek_gno})

#Instance: Scarlet Monastery Scripts
file(GLOB sources_instance_ek_sm scripts/eastern_kingdoms/scarlet_monastery/*.cpp scripts/eastern_kingdoms/scarlet_monastery/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_instance_ek_sm})
    source_group("Eastern Kingdoms Scripts\\Instances\\Scarlet Monastery" FILES ${sources_instance_ek_sm})

#Instance: Scholomance Scripts
file(GLOB sources_instance_ek_scholo scripts/eastern_kingdoms/scholomance/*.cpp scripts/eastern_kingdoms/scholomance/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_instance_ek_scholo})
    source_group("Eastern Kingdoms Scripts\\Instances\\Scholomance" FILES ${sources_instance_ek_scholo})

#Instance: Uldaman Scripts
file(GLOB sources_instance_ek_uld scripts/eastern_kingdoms/uldaman/*.cpp scripts/eastern_kingdoms/uldaman/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_instance_ek_uld})
    source_group("Eastern Kingdoms Scripts\\Instances\\Uldaman" FILES ${sources_instance_ek_uld})

#Instance: Shadowfang Keep Scripts
file(GLOB sources_instance_ek_sfk scripts/eastern_kingdoms/shadowfang_keep/*.cpp scripts/eastern_kingdoms/shadowfang_keep/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_instance_ek_sfk})
    source_group("Eastern Kingdoms Scripts\\Instances\\Shadowfang Keep" FILES ${sources_instance_ek_sfk})

#Instance: Sunken Temple Scripts
file(GLOB sources_instance_ek_st scripts/eastern_kingdoms/sunken_temple/*.cpp scripts/eastern_kingdoms/sunken_temple/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_instance_ek_st})
    source_group("Eastern Kingdoms Scripts\\Instances\\Sunken Temple" FILES ${sources_instance_ek_st})

#Instance: Stratholme Scripts
file(GLOB sources_instance_ek_files scripts/eastern_kingdoms/stratholme/*.cpp scripts/eastern_kingdoms/stratholme/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_instance_ek_files})
    source_group("Eastern Kingdoms Scripts\\Instances\\Stratholme" FILES ${sources_instance_ek_files})

#Raid: Molten Core Scripts
file(GLOB sources_raid_ek_mc scripts/eastern_kingdoms/blackrock_mountain/molten_core/*.cpp scripts/eastern_kingdoms/blackrock_mountain/molten_core/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_raid_ek_mc})
    source_group("Eastern Kingdoms Scripts\\Raids\\Molten Core" FILES ${sources_raid_ek_mc})

#Raid: Blackwing Lair Scripts
file(GLOB sources_raid_ek_bwl scripts/eastern_kingdoms/blackrock_mountain/blackwing_lair/*.cpp scripts/eastern_kingdoms/blackrock_mountain/blackwing_lair/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_raid_ek_bwl})
    source_group("Eastern Kingdoms Scripts\\Raids\\Blackwing Lair" FILES ${sources_raid_ek_bwl})

#Raid: Naxxramas Scripts
file(GLOB sources_raid_ek_naxx scripts/eastern_kingdoms/naxxramas/*.cpp scripts/eastern_kingdoms/naxxramas/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_raid_ek_naxx})
    source_group("Eastern Kingdoms Scripts\\Raids\\Naxxramas" FILES ${sources_raid_ek_naxx})

#Raid: Zul Gurub Scripts
file(GLOB sources_raid_ek_zg scripts/eastern_kingdoms/zulgurub/*.cpp scripts/eastern_kingdoms/zulgurub/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_raid_ek_zg})
    source_group("Eastern Kingdoms Scripts\\Raids\\Zul Gurub" FILES ${sources_raid_ek_zg})

#Instance: Blackfathom Deeps Scripts
file(GLOB sources_instance_k_bdf scripts/kalimdor/blackfathom_deeps/*.cpp scripts/kalimdor/blackfathom_deeps/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_instance_k_bdf})
    source_group("Kalimdor Scripts\\Instances\\Blackfathom Deeps" FILES ${sources_instance_k_bdf})

#Instance: Dire Maul Scripts
file(GLOB sources_instance_k_dm scripts/kalimdor/dire_maul/*.cpp scripts/kalimdor/dire_maul/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_instance_k_dm})
    source_group("Kalimdor Scripts\\Instances\\Dire Maul" FILES ${sources_instance_k_dm})

#Instance: Maraudon Scripts
file(GLOB sources_instance_k_mara scripts/kalimdor/maraudon/*.cpp scripts/kalimdor/maraudon/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_instance_k_mara})
    source_group("Kalimdor Scripts\\Instances\\Maraudon Scripts" FILES ${sources_instance_k_mara})

#Instance: Razorfen Downs Scripts
file(GLOB sources_instance_k_rfd scripts/kalimdor/razorfen_downs/*.cpp scripts/kalimdor/razorfen_downs/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_instance_k_rfd})
    source_group("Kalimdor Scripts\\Instances\\Razorfen Downs" FILES ${sources_instance_k_rfd})

#Instance: Razorfen Kraul Scripts
file(GLOB sources_instance_k_rfk scripts/kalimdor/razorfen_kraul/*.cpp scripts/kalimdor/razorfen_kraul/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_instance_k_rfk})
    source_group("Kalimdor Scripts\\Instances\\Razorfen Kraul" FILES ${sources_instance_k_rfk})

#Instance: Wailing Caverns Scripts
file(GLOB sources_instance_k_wc scripts/kalimdor/wailing_caverns/*.cpp scripts/kalimdor/wailing_caverns/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_instance_k_wc})
    source_group("Kalimdor Scripts\\Instances\\Wailing Caverns" FILES ${sources_instance_k_wc})

#Instance: Zul Farrak Scripts
file(GLOB sources_instance_k_zf scripts/kalimdor/zulfarrak/*.cpp scripts/kalimdor/zulfarrak/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_instance_k_zf})
    source_group("Kalimdor Scripts\\Instances\\Zul Farrak" FILES ${sources_instance_k_zf})

#Raid: Onyxias Lair Scripts
file(GLOB sources_raid_k_ony scripts/kalimdor/onyxias_lair/*.cpp scripts/kalimdor/onyxias_lair/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_raid_k_ony})
    source_group("Kalimdor Scripts\\Raids\\Onyxias Lair" FILES ${sources_raid_k_ony})

#Raid: Ruins of AQ Scripts
file(GLOB sources_raid_k_raq scripts/kalimdor/ruins_of_ahnqiraj/*.cpp scripts/kalimdor/ruins_of_ahnqiraj/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_raid_k_raq})
    source_group("Kalimdor Scripts\\Raids\\Ruins of AQ" FILES ${sources_raid_k_raq})

#Raid: Temple of AQ Scripts
file(GLOB sources_raid_k_taq scripts/kalimdor/temple_of_ahnqiraj/*.cpp scripts/kalimdor/temple_of_ahnqiraj/*.h)
    LIST(APPEND mangosscript_LIB_SRCS ${sources_raid_k_taq})
    source_group("Kalimdor Scripts\\Raids\\Temple of AQ" FILES ${sources_raid_k_taq})

add_library(mangosscript STATIC ${mangosscript_LIB_SRCS})
  
# Generate PCH
if(PCH)
    ADD_CXX_PCH(mangosscript ${mangosscript_PCH})
endif()

if(NOT ACE_USE_EXTERNAL)
    add_dependencies(mangosscript ace)
endif()

if(UNIX)
    set(mangosscript_LINK_FLAGS "-pthread")
    if(APPLE)
        set(mangosscript_LINK_FLAGS "-framework Carbon ${mangosscript_LINK_FLAGS}")
        # Needed for the linking because of the missing symbols
        set(mangosscript_LINK_FLAGS "-Wl,-undefined -Wl,dynamic_lookup ${mangosscript_LINK_FLAGS}")
    endif()

    if(APPLE)
        set(mangosscript_PROPERTIES INSTALL_NAME_DIR ${LIBS_DIR})
    else()
        set(mangosscript_PROPERTIES INSTALL_RPATH ${LIBS_DIR})
    endif()

    # Run out of build tree
    set(mangosscript_PROPERTIES
        ${mangosscript_PROPERTIES}
        BUILD_WITH_INSTALL_RPATH OFF
    )

    set_target_properties(mangosscript PROPERTIES
        LINK_FLAGS ${mangosscript_LINK_FLAGS}
        ${mangosscript_PROPERTIES}
    )
endif()