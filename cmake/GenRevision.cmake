
if(NOT BUILDDIR)
  # Workaround for funny MSVC behaviour - this segment is only used when using cmake gui
  set(BUILDDIR ${CMAKE_BINARY_DIR})
endif()

if(WITHOUT_GIT)
  set(rev_date                "1970-01-01 00:00:00 +0000" )
  set(rev_hash                "unknown"                   )
  set(rev_branch              "Archived"                  )

  if(SCRIPT_LIB_ELUNA)
    set(dep_eluna_rev_date    "1970-01-01 00:00:00 +0000" )
    set(dep_eluna_rev_hash    "unknown"                   )
    set(dep_eluna_rev_branch  "Archived"                  )
  endif()
  if(SCRIPT_LIB_SD3)
    set(dep_sd3_rev_date      "1970-01-01 00:00:00 +0000" )
    set(dep_sd3_rev_hash      "unknown"                   )
    set(dep_sd3_rev_branch    "Archived"                  )
  endif()

  # No valid git commit date, use compiled date
  string(TIMESTAMP rev_date_fallback            "%Y-%m-%d %H:%M:%S" UTC)

  if(SCRIPT_LIB_ELUNA)
    string(TIMESTAMP dep_eluna_rev_date_fallback  "%Y-%m-%d %H:%M:%S" UTC)
  endif()
  if(SCRIPT_LIB_SD3)
    string(TIMESTAMP dep_sd3_rev_date_fallback    "%Y-%m-%d %H:%M:%S" UTC)
  endif()
else()
  if(GIT_EXECUTABLE)
    # Create a revision-string that we can use
    execute_process(
      COMMAND "${GIT_EXECUTABLE}" describe --long --match init --dirty=+ --abbrev=12 --always
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
      OUTPUT_VARIABLE rev_info
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
    )
    # And grab the commits timestamp
    execute_process(
      COMMAND "${GIT_EXECUTABLE}" show -s --format=%ci
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
      OUTPUT_VARIABLE rev_date
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
    )
    # Also retrieve branch name
    execute_process(
      COMMAND "${GIT_EXECUTABLE}" rev-parse --abbrev-ref HEAD
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
      OUTPUT_VARIABLE rev_branch
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
    )

    if(SCRIPT_LIB_ELUNA)
      # Create a revision-string that we can use
      execute_process(
        COMMAND "${GIT_EXECUTABLE}" describe --long --match init --dirty=+ --abbrev=12 --always
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/src/modules/Eluna"
        OUTPUT_VARIABLE dep_eluna_rev_info
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
      )
      # And grab the commits timestamp
      execute_process(
        COMMAND "${GIT_EXECUTABLE}" show -s --format=%ci
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/src/modules/Eluna"
        OUTPUT_VARIABLE dep_eluna_rev_date
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
      )
      # Also retrieve branch name
      execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse --abbrev-ref HEAD
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/src/modules/Eluna"
        OUTPUT_VARIABLE dep_eluna_rev_branch
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
      )
    endif()
    if(SCRIPT_LIB_SD3)
      # Create a revision-string that we can use
      execute_process(
        COMMAND "${GIT_EXECUTABLE}" describe --long --match init --dirty=+ --abbrev=12 --always
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/src/modules/SD3"
        OUTPUT_VARIABLE dep_sd3_rev_info
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
      )
      # And grab the commits timestamp
      execute_process(
        COMMAND "${GIT_EXECUTABLE}" show -s --format=%ci
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/src/modules/SD3"
        OUTPUT_VARIABLE dep_sd3_rev_date
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
      )
      # Also retrieve branch name
      execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse --abbrev-ref HEAD
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/src/modules/SD3"
        OUTPUT_VARIABLE dep_sd3_rev_branch
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
      )
    endif()
  endif()

  # Last minute check - ensure that we have a proper revision
  # If everything above fails (means the user has erased the git revision control directory or removed the origin/HEAD tag)
  if(NOT rev_info)
    # No valid ways available to find/set the revision/hash, so let's force some defaults
    message(STATUS "
    Could not find a proper repository signature (hash) - you may need to pull tags with git fetch -t
    Continuing anyway - note that the versionstring will be set to \"unknown 1970-01-01 00:00:00 (Archived)\"")
    set(rev_date              "1970-01-01 00:00:00 +0000" )
    set(rev_hash              "unknown"                   )
    set(rev_branch            "Archived"                  )

    if(SCRIPT_LIB_ELUNA)
      set(dep_eluna_rev_date    "1970-01-01 00:00:00 +0000" )
      set(dep_eluna_rev_hash    "unknown"                   )
      set(dep_eluna_rev_branch  "Archived"                  )
    endif()
    if(SCRIPT_LIB_SD3)
      set(dep_sd3_rev_date      "1970-01-01 00:00:00 +0000" )
      set(dep_sd3_rev_hash      "unknown"                   )
      set(dep_sd3_rev_branch    "Archived"                  )
    endif()

    # No valid git commit date, use compiled date
    string(TIMESTAMP rev_date_fallback            "%Y-%m-%d %H:%M:%S" UTC)
    if(SCRIPT_LIB_ELUNA)
      string(TIMESTAMP dep_eluna_rev_date_fallback  "%Y-%m-%d %H:%M:%S" UTC)
    endif()
    if(SCRIPT_LIB_SD3)
      string(TIMESTAMP dep_sd3_rev_date_fallback    "%Y-%m-%d %H:%M:%S" UTC)
    endif()
  else()
    # We have valid date from git commit, use it
    set(rev_date_fallback           ${rev_date}           )
    if(SCRIPT_LIB_ELUNA)
      set(dep_eluna_rev_date_fallback ${dep_eluna_rev_date} )
    endif()
    if(SCRIPT_LIB_SD3)
      set(dep_sd3_rev_date_fallback   ${dep_sd3_rev_date}   )
    endif()

    # Extract information required to build a proper versionstring
    string(REGEX REPLACE init-|[0-9]+-g "" rev_hash           ${rev_info}           )
    if(SCRIPT_LIB_ELUNA)
      string(REGEX REPLACE init-|[0-9]+-g "" dep_eluna_rev_hash ${dep_eluna_rev_info} )
    endif()
    if(SCRIPT_LIB_SD3)
      string(REGEX REPLACE init-|[0-9]+-g "" dep_sd3_rev_hash   ${dep_sd3_rev_info}   )
    endif()
  endif()
endif()

# For package / copyright information we always need proper date
string(REGEX MATCH "([0-9]+)-([0-9]+)-([0-9]+)" rev_date_fallback_match ${rev_date_fallback})
set(rev_year  ${CMAKE_MATCH_1})
set(rev_month ${CMAKE_MATCH_2})
set(rev_day   ${CMAKE_MATCH_3})

# message("rev_hash_cached            : ${rev_hash_cached}"             )
# message("rev_hash                   : ${rev_hash}"                    )
# message("rev_branch_cached          : ${rev_branch_cached}"           )
# message("rev_branch                 : ${rev_branch}"                  )
# message("dep_eluna_rev_hash_cached  : ${dep_eluna_rev_hash_cached}"   )
# message("dep_eluna_rev_hash         : ${dep_eluna_rev_hash}"          )
# message("dep_eluna_rev_branch_cached: ${dep_eluna_rev_branch_cached}" )
# message("dep_eluna_rev_branch       : ${dep_eluna_rev_branch}"        )
# message("dep_sd3_rev_hash_cached    : ${dep_sd3_rev_hash_cached}"     )
# message("dep_sd3_rev_hash           : ${dep_sd3_rev_hash}"            )
# message("dep_sd3_rev_branch_cached  : ${dep_sd3_rev_branch_cached}"   )
# message("dep_sd3_rev_branch         : ${dep_sd3_rev_branch}"          )
# message("CMAKE_CURRENT_BINARY_DIR   : ${CMAKE_CURRENT_BINARY_DIR}"    )
# if(EXISTS "${CMAKE_CURRENT_BINARY_DIR}/src/shared/revision_data.h")
#   message("revision_data.h exists.")
# else()
#   message("revision_data.h does not exist.")
# endif()
# Create the actual revision_data.h file from the above params
if(
     NOT "${rev_hash_cached}"             MATCHES "${rev_hash}"
  OR NOT "${rev_branch_cached}"           MATCHES "${rev_branch}"
  OR NOT "${dep_eluna_rev_hash_cached}"   MATCHES "${dep_eluna_rev_hash}"
  OR NOT "${dep_eluna_rev_branch_cached}" MATCHES "${dep_eluna_rev_branch}"
  OR NOT "${dep_sd3_rev_hash_cached}"     MATCHES "${dep_sd3_rev_hash}"
  OR NOT "${dep_sd3_rev_branch_cached}"   MATCHES "${dep_sd3_rev_branch}"
  OR NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/src/shared/revision_data.h"
)
  configure_file(
    "${CMAKE_SOURCE_DIR}/src/shared/revision_data.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/src/shared/revision_data.h"
    @ONLY
  )
  set(rev_hash_cached             "${rev_hash}"             CACHE INTERNAL "Cached commit-hash"       )
  set(rev_branch_cached           "${rev_branch}"           CACHE INTERNAL "Cached branch name"       )
  if(SCRIPT_LIB_ELUNA)
    set(dep_eluna_rev_hash_cached   "${dep_eluna_rev_hash}"   CACHE INTERNAL "Cached Eluna commit-hash" )
    set(dep_eluna_rev_branch_cached "${dep_eluna_rev_branch}" CACHE INTERNAL "Cached Eluna branch name" )
  endif()
  if(SCRIPT_LIB_SD3)
    set(dep_sd3_rev_hash_cached     "${dep_sd3_rev_hash}"      CACHE INTERNAL "Cached SD3 commit-hash"   )
    set(dep_sd3_rev_branch_cached   "${dep_sd3_rev_branch}"    CACHE INTERNAL "Cached SD3 branch name"   )
  endif()
endif()
