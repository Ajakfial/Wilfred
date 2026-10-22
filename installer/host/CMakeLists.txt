cmake_minimum_required(VERSION 3.20)

# Standalone project: never part of the main Wilfred build. It is configured
# by scripts/Build-Setup.ps1 with the wizard HTML and MSI payload baked in.
project(wilfred_setup LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

foreach(required IN ITEMS WEBVIEW2_ROOT SETUP_MSI SETUP_HTML SETUP_ICON SETUP_VERSION SETUP_VERSION_COMMA)
    if(NOT ${required})
        message(FATAL_ERROR "Build-Setup.ps1 must pass -D${required}=... to this project.")
    endif()
endforeach()

set(WEBVIEW2_INCLUDE_DIR "${WEBVIEW2_ROOT}/build/native/include")
set(WEBVIEW2_LOADER "${WEBVIEW2_ROOT}/build/native/x64/WebView2LoaderStatic.lib")
foreach(path IN ITEMS "${WEBVIEW2_INCLUDE_DIR}/WebView2.h" "${WEBVIEW2_LOADER}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "WebView2 SDK is incomplete: ${path} not found.")
    endif()
endforeach()

foreach(path IN ITEMS "${SETUP_MSI}" "${SETUP_HTML}" "${SETUP_ICON}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Missing setup input: ${path}")
    endif()
endforeach()

# rc.exe processes C-style escapes inside quoted file names, so every
# backslash in an absolute path must be doubled.
function(escape_rc_path out_var path)
    file(TO_NATIVE_PATH "${path}" native)
    string(REPLACE "\\" "\\\\" escaped "${native}")
    set(${out_var} "${escaped}" PARENT_SCOPE)
endfunction()

escape_rc_path(SETUP_MSI_NATIVE "${SETUP_MSI}")
escape_rc_path(SETUP_HTML_NATIVE "${SETUP_HTML}")
escape_rc_path(SETUP_ICON_NATIVE "${SETUP_ICON}")

# CMake/link.exe generate and embed a default manifest. Merge ours in as an
# input instead of adding a second RT_MANIFEST resource (CVT1100 duplicate).
file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src/app.manifest" SETUP_MANIFEST_NATIVE)

configure_file(src/app.rc.in "${CMAKE_CURRENT_BINARY_DIR}/app.rc" @ONLY)
configure_file(src/version.h.in "${CMAKE_CURRENT_BINARY_DIR}/version.h" @ONLY)

add_executable(wilfred-setup WIN32
    src/main.cpp
    src/msi_runner.cpp
    src/json.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/app.rc"
)

target_include_directories(wilfred-setup PRIVATE
    src
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${WEBVIEW2_INCLUDE_DIR}"
)

target_compile_definitions(wilfred-setup PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

target_compile_options(wilfred-setup PRIVATE /utf-8 /W4 /permissive-)

# Merge our DPI-awareness/per-monitor manifest into the generated one.
target_link_options(wilfred-setup PRIVATE "/MANIFESTINPUT:\"${SETUP_MANIFEST_NATIVE}\"")

target_link_libraries(wilfred-setup PRIVATE
    "${WEBVIEW2_LOADER}"
    ole32
    oleaut32
    shell32
    shlwapi
    user32
    gdi32
    advapi32
    dwmapi
    msi
    version
)

set_target_properties(wilfred-setup PROPERTIES
    OUTPUT_NAME "wilfred-setup"
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:wilfred-setup>"
)
