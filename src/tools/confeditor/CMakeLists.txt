# Windows conf editors: classic Win32 dialog, and HTML UI hosted in WebView2.
# Both parse only — they link no server code and know no setting by name.
#
# Gated from the root by BUILD_CONFEDITOR / BUILD_CONFEDITOR_WEB.

if(NOT WIN32)
    return()
endif()

if(NOT BUILD_CONFEDITOR AND NOT BUILD_CONFEDITOR_WEB)
    return()
endif()

# ---------------------------------------------------------------------------
# Shared model (needed by either editor)
# ---------------------------------------------------------------------------

add_library(mangos-conf-model STATIC
    ConfModel.cpp
    ConfModel.h)

target_include_directories(mangos-conf-model PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(mangos-conf-model PROPERTIES FOLDER "tools")

# ---------------------------------------------------------------------------
# Win32 editor — BUILD_CONFEDITOR
# ---------------------------------------------------------------------------

if(BUILD_CONFEDITOR)
    add_executable(mangos-conf-editor WIN32
        ConfEditor.cpp
        mangos-conf-editor.rc)

    target_link_libraries(mangos-conf-editor PRIVATE
        mangos-conf-model comctl32 comdlg32 shell32 ole32 uxtheme)

    target_include_directories(mangos-conf-editor PRIVATE ${CMAKE_BINARY_DIR}/src/shared)
    if(TARGET revision_data.h)
        add_dependencies(mangos-conf-editor revision_data.h)
    endif()

    target_compile_definitions(mangos-conf-editor
        PRIVATE UNICODE _UNICODE MANGOS_CLIENT_NAME="${MANGOS_CLIENT_NAME}")

    set_target_properties(mangos-conf-editor PROPERTIES FOLDER "tools")
    install(TARGETS mangos-conf-editor DESTINATION ${BIN_DIR}/tools/confeditor)
    message(STATUS "confeditor Win32: ON → ${BIN_DIR}/tools/confeditor")
else()
    message(STATUS "confeditor Win32: OFF (BUILD_CONFEDITOR=OFF)")
endif()

# ---------------------------------------------------------------------------
# WebView2 (HTML) editor — BUILD_CONFEDITOR_WEB
#
# SDK     — headers + WebView2Loader*.lib for *building* (NuGet; can be fetched)
# Runtime — Evergreen on the *user machine* (detected; not installed by CMake)
# ---------------------------------------------------------------------------

set(WEBVIEW2_VER "1.0.2903.40" CACHE STRING "Microsoft.Web.WebView2 NuGet version")
set(WEBVIEW2_ROOT "" CACHE PATH "Extracted Microsoft.Web.WebView2 package root")
option(WEBVIEW2_FETCH_SDK "Download WebView2 NuGet SDK into the build tree if missing" ON)
option(WEBVIEW2_FETCH_BOOTSTRAPPER
    "Download Evergreen Bootstrapper next to the web editor (for redistributing Runtime)" OFF)

function(confeditor_webview2_find_sdk out_root)
    set(_found "")
    if(WEBVIEW2_ROOT AND EXISTS "${WEBVIEW2_ROOT}/build/native/include/WebView2.h")
        set(_found "${WEBVIEW2_ROOT}")
    elseif(DEFINED ENV{WEBVIEW2_SDK} AND EXISTS "$ENV{WEBVIEW2_SDK}/build/native/include/WebView2.h")
        set(_found "$ENV{WEBVIEW2_SDK}")
    else()
        file(GLOB _nuget_hits
            "$ENV{USERPROFILE}/.nuget/packages/microsoft.web.webview2/*/build/native/include/WebView2.h")
        if(_nuget_hits)
            list(SORT _nuget_hits ORDER DESCENDING)
            list(GET _nuget_hits 0 _hdr)
            get_filename_component(_inc "${_hdr}" DIRECTORY)
            get_filename_component(_native "${_inc}" DIRECTORY)
            get_filename_component(_build "${_native}" DIRECTORY)
            get_filename_component(_pkg "${_build}" DIRECTORY)
            set(_found "${_pkg}")
        endif()
    endif()
    set(${out_root} "${_found}" PARENT_SCOPE)
endfunction()

function(confeditor_webview2_fetch_sdk out_root)
    set(_dst "${CMAKE_BINARY_DIR}/_deps/webview2-${WEBVIEW2_VER}")
    set(_zip "${CMAKE_BINARY_DIR}/_deps/webview2-${WEBVIEW2_VER}.zip")
    if(NOT EXISTS "${_dst}/build/native/include/WebView2.h")
        message(STATUS "WebView2 SDK: downloading Microsoft.Web.WebView2 ${WEBVIEW2_VER}…")
        file(DOWNLOAD
            "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/${WEBVIEW2_VER}"
            "${_zip}"
            SHOW_PROGRESS
            STATUS _st
            TLS_VERIFY ON)
        list(GET _st 0 _code)
        if(NOT _code EQUAL 0)
            message(STATUS "WebView2 SDK: download failed (${_st})")
            set(${out_root} "" PARENT_SCOPE)
            return()
        endif()
        file(ARCHIVE_EXTRACT INPUT "${_zip}" DESTINATION "${_dst}")
    endif()
    if(EXISTS "${_dst}/build/native/include/WebView2.h")
        set(${out_root} "${_dst}" PARENT_SCOPE)
    else()
        set(${out_root} "" PARENT_SCOPE)
    endif()
endfunction()

function(confeditor_webview2_detect_runtime out_ok out_ver)
    set(_ok FALSE)
    set(_ver "")
    set(_keys
        "HKLM\\SOFTWARE\\Microsoft\\EdgeUpdate\\Clients\\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}"
        "HKLM\\SOFTWARE\\WOW6432Node\\Microsoft\\EdgeUpdate\\Clients\\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}"
        "HKCU\\SOFTWARE\\Microsoft\\EdgeUpdate\\Clients\\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}")
    foreach(_k IN LISTS _keys)
        execute_process(
            COMMAND reg query "${_k}" /v pv
            OUTPUT_VARIABLE _out
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(_out MATCHES "pv[ \t]+REG_SZ[ \t]+([0-9.]+)")
            set(_ok TRUE)
            set(_ver "${CMAKE_MATCH_1}")
            break()
        endif()
    endforeach()
    set(${out_ok} ${_ok} PARENT_SCOPE)
    set(${out_ver} "${_ver}" PARENT_SCOPE)
endfunction()

if(BUILD_CONFEDITOR_WEB)
    confeditor_webview2_find_sdk(_wv_sdk)
    if(NOT _wv_sdk AND WEBVIEW2_FETCH_SDK)
        confeditor_webview2_fetch_sdk(_wv_sdk)
    endif()
    if(_wv_sdk)
        set(WEBVIEW2_ROOT "${_wv_sdk}" CACHE PATH "Extracted Microsoft.Web.WebView2 package root" FORCE)
    endif()

    confeditor_webview2_detect_runtime(_wv_rt_ok _wv_rt_ver)
    if(_wv_rt_ok)
        message(STATUS "WebView2 Runtime: found (pv=${_wv_rt_ver})")
    else()
        message(STATUS "WebView2 Runtime: not detected — install Evergreen Runtime to run the HTML editor")
    endif()
endif()

if(BUILD_CONFEDITOR_WEB AND WEBVIEW2_ROOT AND EXISTS "${WEBVIEW2_ROOT}/build/native/include/WebView2.h")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(_wv_arch "x64")
    else()
        set(_wv_arch "x86")
    endif()

    add_executable(mangos-conf-editor-web WIN32
        web/ConfEditorWeb.cpp
        mangos-conf-editor.rc)

    target_include_directories(mangos-conf-editor-web PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_BINARY_DIR}/src/shared
        "${WEBVIEW2_ROOT}/build/native/include")

    target_link_libraries(mangos-conf-editor-web PRIVATE
        mangos-conf-model
        comdlg32 shell32 ole32 version
        "${WEBVIEW2_ROOT}/build/native/${_wv_arch}/WebView2LoaderStatic.lib")

    if(TARGET revision_data.h)
        add_dependencies(mangos-conf-editor-web revision_data.h)
    endif()

    target_compile_definitions(mangos-conf-editor-web
        PRIVATE UNICODE _UNICODE MANGOS_CLIENT_NAME="${MANGOS_CLIENT_NAME}")

    set_target_properties(mangos-conf-editor-web PROPERTIES FOLDER "tools")

    add_custom_command(TARGET mangos-conf-editor-web POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/web/ui"
            "$<TARGET_FILE_DIR:mangos-conf-editor-web>/confeditor-ui"
        COMMENT "Copy confeditor HTML UI next to mangos-conf-editor-web")

    if(WEBVIEW2_FETCH_BOOTSTRAPPER)
        set(_boot_cache "${CMAKE_BINARY_DIR}/_deps/MicrosoftEdgeWebview2Setup.exe")
        if(NOT EXISTS "${_boot_cache}")
            message(STATUS "WebView2: downloading Evergreen Bootstrapper…")
            file(DOWNLOAD
                "https://go.microsoft.com/fwlink/p/?LinkId=2124703"
                "${_boot_cache}"
                SHOW_PROGRESS STATUS _boot_st TLS_VERIFY ON)
            list(GET _boot_st 0 _boot_code)
            if(NOT _boot_code EQUAL 0)
                message(STATUS "WebView2 Bootstrapper download failed (${_boot_st})")
                file(REMOVE "${_boot_cache}")
            endif()
        endif()
        if(EXISTS "${_boot_cache}")
            add_custom_command(TARGET mangos-conf-editor-web POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_boot_cache}"
                    "$<TARGET_FILE_DIR:mangos-conf-editor-web>/MicrosoftEdgeWebview2Setup.exe"
                COMMENT "Copy WebView2 Evergreen Bootstrapper next to web editor")
            install(FILES "${_boot_cache}"
                DESTINATION ${BIN_DIR}/tools/confeditor-web
                RENAME MicrosoftEdgeWebview2Setup.exe)
        endif()
    endif()

    install(TARGETS mangos-conf-editor-web DESTINATION ${BIN_DIR}/tools/confeditor-web)
    install(DIRECTORY web/ui/ DESTINATION ${BIN_DIR}/tools/confeditor-web/confeditor-ui)

    message(STATUS "confeditor HTML (WebView2): ON  SDK=${WEBVIEW2_ROOT} → ${BIN_DIR}/tools/confeditor-web")
elseif(BUILD_CONFEDITOR_WEB)
    message(STATUS "confeditor HTML (WebView2): OFF — SDK missing (set WEBVIEW2_ROOT or WEBVIEW2_FETCH_SDK=ON)")
else()
    message(STATUS "confeditor HTML (WebView2): OFF (BUILD_CONFEDITOR_WEB=OFF)")
endif()
