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

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/ahbot.conf.dist.in" "${CMAKE_CURRENT_BINARY_DIR}/ahbot.conf.dist")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/ahbot.conf.dist" DESTINATION "${CONF_INSTALL_DIR}")

if(WIN32 AND MSVC)
  add_custom_command(TARGET ${LIBRARY_NAME}
    POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_BINARY_DIR}/ahbot.conf.dist" "${CONF_COPY_DIR}"
  )
endif()
