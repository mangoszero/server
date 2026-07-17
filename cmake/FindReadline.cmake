#[==[
Locates the GNU Readline library, used by the Lua interpreter build (see
dep/lualib/lua/CMakeLists.txt) for interactive line editing/history.
Unix-only: there is no Windows build of readline in this project.

dep/lualib/lua/CMakeLists.txt links against it by the bare name `readline`
(e.g. `target_link_libraries(lualib readline)`) and is not itself touched by
this module. Instead, this module declares a GLOBAL IMPORTED target literally
named `readline`, so CMake resolves those existing bare-name references to
this properly-detected library instead of falling back to an unqualified
linker search for `-lreadline`.

Also provides the following variables:

  * `Readline_INCLUDE_DIRS`: Include directories necessary to use Readline.
  * `Readline_LIBRARIES`: Libraries necessary to use Readline.
#]==]

set(Readline_FOUND 0)

find_path(Readline_INCLUDE_DIR
  NAMES readline/readline.h
  PATHS
    /usr/include
    /usr/local/include
    /opt/homebrew/opt/readline/include # Homebrew, Apple Silicon (keg-only)
    /usr/local/opt/readline/include    # Homebrew, Intel macOS (keg-only)
  DOC "Location of readline/readline.h")
mark_as_advanced(Readline_INCLUDE_DIR)

find_library(Readline_LIBRARY
  NAMES readline
  PATHS
    /usr/lib
    /usr/local/lib
    /opt/homebrew/opt/readline/lib
    /usr/local/opt/readline/lib
  DOC "Location of the readline library")
mark_as_advanced(Readline_LIBRARY)

# On several platforms (e.g. macOS) readline's history API ships as a
# separate library that must also be linked; harmless to link where it
# doesn't exist separately, so it is optional rather than required below.
find_library(Readline_HISTORY_LIBRARY
  NAMES history
  PATHS
    /usr/lib
    /usr/local/lib
    /opt/homebrew/opt/readline/lib
    /usr/local/opt/readline/lib
  DOC "Location of the readline history library")
mark_as_advanced(Readline_HISTORY_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Readline
  REQUIRED_VARS Readline_INCLUDE_DIR Readline_LIBRARY)

if (Readline_FOUND)
  if (NOT TARGET readline)
    add_library(readline UNKNOWN IMPORTED GLOBAL)
    set_target_properties(readline PROPERTIES
      IMPORTED_LOCATION "${Readline_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${Readline_INCLUDE_DIR}")
    if (Readline_HISTORY_LIBRARY)
      set_property(TARGET readline APPEND PROPERTY
        INTERFACE_LINK_LIBRARIES "${Readline_HISTORY_LIBRARY}")
    endif ()
  endif ()

  set(Readline_INCLUDE_DIRS "${Readline_INCLUDE_DIR}")
  set(Readline_LIBRARIES "${Readline_LIBRARY}")
  if (Readline_HISTORY_LIBRARY)
    list(APPEND Readline_LIBRARIES "${Readline_HISTORY_LIBRARY}")
  endif ()
endif ()
