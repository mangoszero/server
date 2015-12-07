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

# Specify ace lib that was build externally
# add_library(ace SHARED IMPORTED)
# Sadly doesn't work in current version of cmake
# add_dependencies(ace ACE_Project)
# set_target_properties(ace PROPERTIES DEPENDS ACE_Project)

set(ACE_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/dep/acelite)
set(ACE_LIBRARIES_DIR ${CMAKE_BINARY_DIR}/dep/acelite/ace)
set(ACE_LIBRARIES optimized ACE debug ACE)

# Little Hack to remove the link warnings because of not found directories
#if(XCODE)
#  foreach(DIR ${ACE_LIBRARIES_DIR})
#    foreach(CONF ${CMAKE_CONFIGURATION_TYPES})
#      file(MAKE_DIRECTORY ${DIR}/${CONF})
#    endforeach(CONF)
#  endforeach(DIR)
#endif()

link_directories(
  ${ACE_LIBRARIES_DIR}
)

#if(WIN32)
#  foreach(DIR ${ACE_LIBRARIES_DIR})
#    install(
#      DIRECTORY ${DIR}/ DESTINATION ${LIBS_DIR}
#      FILES_MATCHING PATTERN "*.dll*" #"*.${LIB_SUFFIX}*"
#      PATTERN "pkgconfig" EXCLUDE
#    )
#  endforeach(DIR)
#endif()
