# MaNGOS is a full featured server for World of Warcraft, supporting
# the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
#
# Copyright (C) 2005-2022 MaNGOS <https://getmangos.eu>
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

MACRO(NORMALIZE_VERSION _requested_version _normalized_version)
    STRING(REGEX MATCH "[^0-9]*[0-9]+\\.[0-9]+\\.[0-9]+.*" _threePartMatch "${_requested_version}")
    if(_threePartMatch)
    # parse the parts of the version string
        STRING(REGEX REPLACE "[^0-9]*([0-9]+)\\.[0-9]+\\.[0-9]+.*" "\\1" _major_vers "${_requested_version}")
        STRING(REGEX REPLACE "[^0-9]*[0-9]+\\.([0-9]+)\\.[0-9]+.*" "\\1" _minor_vers "${_requested_version}")
        STRING(REGEX REPLACE "[^0-9]*[0-9]+\\.[0-9]+\\.([0-9]+).*" "\\1" _patch_vers "${_requested_version}")
    else(_threePartMatch)
        STRING(REGEX REPLACE "([0-9]+)\\.[0-9]+" "\\1" _major_vers "${_requested_version}")
        STRING(REGEX REPLACE "[0-9]+\\.([0-9]+)" "\\1" _minor_vers "${_requested_version}")
        set(_patch_vers "0")
    endif(_threePartMatch)

    # compute an overall version number which can be compared at once
    MATH(EXPR ${_normalized_version} "${_major_vers}*10000 + ${_minor_vers}*100 + ${_patch_vers}")
ENDMACRO(NORMALIZE_VERSION)

MACRO(CHECK_RANGE_INCLUSIVE_LOWER _lower_limit _value _upper_limit _ok)
   if(${_value} LESS ${_lower_limit})
      set(${_ok} FALSE)
  elseif(${_value} EQUAL ${_lower_limit})
      set(${_ok} TRUE)
  elseif(${_value} EQUAL ${_upper_limit})
      set(${_ok} FALSE)
  elseif(${_value} GREATER ${_upper_limit})
      set(${_ok} FALSE)
  else(${_value} LESS ${_lower_limit})
      set(${_ok} TRUE)
  endif(${_value} LESS ${_lower_limit})
ENDMACRO(CHECK_RANGE_INCLUSIVE_LOWER)

MACRO(ENSURE_VERSION requested_version found_version var_too_old)
    NORMALIZE_VERSION(${requested_version} req_vers_num)
    NORMALIZE_VERSION(${found_version} found_vers_num)

    if(found_vers_num LESS req_vers_num)
        set(${var_too_old} FALSE)
    else(found_vers_num LESS req_vers_num)
        set(${var_too_old} TRUE)
    endif(found_vers_num LESS req_vers_num)

ENDMACRO(ENSURE_VERSION)

MACRO(ENSURE_VERSION2 requested_version2 found_version2 var_too_old2)
    ENSURE_VERSION(${requested_version2} ${found_version2} ${var_too_old2})
ENDMACRO(ENSURE_VERSION2)

MACRO(ENSURE_VERSION_RANGE min_version found_version max_version var_ok)
    NORMALIZE_VERSION(${min_version} req_vers_num)
    NORMALIZE_VERSION(${found_version} found_vers_num)
    NORMALIZE_VERSION(${max_version} max_vers_num)

    CHECK_RANGE_INCLUSIVE_LOWER(${req_vers_num} ${found_vers_num} ${max_vers_num} ${var_ok})
ENDMACRO(ENSURE_VERSION_RANGE)
