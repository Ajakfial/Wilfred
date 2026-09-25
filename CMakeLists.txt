cmake_minimum_required(VERSION 3.16)
project(Wilfred VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(APPLE)
  enable_language(OBJCXX)
endif()

option(WILFRED_BUILD_TESTS "Build unit tests" ON)
option(WILFRED_BUILD_BENCH "Build benchmarks" ON)
option(WILFRED_ENABLE_SANITIZERS "Enable ASan/UBSan in Debug" OFF)

if(MSVC)
  add_compile_options(/W4 /permissive- /Zc:__cplusplus /utf-8)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

if(WILFRED_ENABLE_SANITIZERS AND NOT MSVC)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

set(WILFRED_CORE_SOURCES
  src/core/log.cpp
  src/core/paths.cpp
  src/core/utf8.cpp
  src/core/time_util.cpp
  src/core/thread_pool.cpp
  src/core/mmap.cpp
  src/core/crc32.cpp
  src/config/yaml.cpp
  src/config/config.cpp
  src/index/string_pool.cpp
  src/index/record.cpp
  src/index/wal.cpp
  src/index/store.cpp
  src/index/tokenizer.cpp
  src/index/engine.cpp
  src/fs/classify.cpp
  src/fs/walker.cpp
  src/fs/volumes.cpp
  src/fs/watcher.cpp
  src/search/fuzzy.cpp
  src/search/rank.cpp
  src/search/filter.cpp
  src/search/engine.cpp
  src/search/clipboard.cpp
  src/search/context.cpp
  src/search/content.cpp
  src/search/macros.cpp
  src/search/minis.cpp
  src/query/classify.cpp
  src/query/interpreter.cpp
  src/math/expr.cpp
  src/math/convert.cpp
  src/apps/discovery.cpp
  src/browser/browser.cpp
  src/history/history.cpp
  src/ipc/protocol.cpp
  src/ipc/server.cpp
  src/hotkey/hotkey.cpp
  src/platform/platform.cpp
  src/service/service.cpp
  src/ui/web_ui.cpp
  src/ui/icon.cpp
)

set(WILFRED_CORE_HEADERS
  include/wilfred/core/log.hpp
  include/wilfred/core/paths.hpp
  include/wilfred/core/utf8.hpp
  include/wilfred/core/time_util.hpp
  include/wilfred/core/thread_pool.hpp
  include/wilfred/core/mmap.hpp
  include/wilfred/core/crc32.hpp
  include/wilfred/core/span.hpp
  include/wilfred/config/yaml.hpp
  include/wilfred/config/config.hpp
  include/wilfred/index/string_pool.hpp
  include/wilfred/index/record.hpp
  include/wilfred/index/wal.hpp
  include/wilfred/index/store.hpp
  include/wilfred/index/tokenizer.hpp
  include/wilfred/index/engine.hpp
  include/wilfred/fs/classify.hpp
  include/wilfred/fs/walker.hpp
  include/wilfred/fs/volumes.hpp
  include/wilfred/fs/watcher.hpp
  include/wilfred/search/fuzzy.hpp
  include/wilfred/search/rank.hpp
  include/wilfred/search/filter.hpp
  include/wilfred/search/engine.hpp
  include/wilfred/search/clipboard.hpp
  include/wilfred/search/context.hpp
  include/wilfred/search/content.hpp
  include/wilfred/search/macros.hpp
  include/wilfred/search/minis.hpp
  include/wilfred/query/classify.hpp
  include/wilfred/query/interpreter.hpp
  include/wilfred/math/expr.hpp
  include/wilfred/apps/discovery.hpp
  include/wilfred/browser/browser.hpp
  include/wilfred/history/history.hpp
  include/wilfred/ipc/protocol.hpp
  include/wilfred/ipc/server.hpp
  include/wilfred/hotkey/hotkey.hpp
  include/wilfred/platform/platform.hpp
  include/wilfred/service/service.hpp
  include/wilfred/ui/overlay.hpp
  include/wilfred/ui/web_ui.hpp
  include/wilfred/ui/icon.hpp
  include/wilfred/platform/native.hpp
  include/wilfred/providers/provider.hpp
)

if(WIN32)
  list(APPEND WILFRED_CORE_SOURCES
    src/platform/win/fs_watch.cpp
    src/platform/win/apps.cpp
    src/platform/win/browser.cpp
    src/platform/win/hotkey.cpp
    src/platform/win/launch.cpp
    src/platform/win/volumes.cpp
    src/ui/win_overlay.cpp
  )
elseif(APPLE)
  list(APPEND WILFRED_CORE_SOURCES
    src/platform/mac/fs_watch.mm
    src/platform/mac/apps.mm
    src/platform/mac/browser.mm
    src/platform/mac/hotkey.mm
    src/platform/mac/launch.mm
    src/platform/mac/volumes.mm
    src/ui/mac_overlay.mm
  )
else()
  list(APPEND WILFRED_CORE_SOURCES
    src/platform/linux/fs_watch.cpp
    src/platform/linux/apps.cpp
    src/platform/linux/browser.cpp
    src/platform/linux/hotkey.cpp
    src/platform/linux/launch.cpp
    src/platform/linux/volumes.cpp
    src/ui/x11_overlay.cpp
  )
endif()

add_library(wilfred_core STATIC ${WILFRED_CORE_SOURCES} ${WILFRED_CORE_HEADERS})
target_include_directories(wilfred_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(WIN32)
  target_link_libraries(wilfred_core PUBLIC ole32 uuid shell32 advapi32 user32 gdi32 comdlg32 oleaut32 dwmapi version
    winhttp psapi iphlpapi ws2_32)

  include(FetchContent)
  if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
  endif()
  FetchContent_Declare(
    microsoft_webview2
    URL https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/1.0.2903.40
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  FetchContent_GetProperties(microsoft_webview2)
  if(NOT microsoft_webview2_POPULATED)
    FetchContent_Populate(microsoft_webview2)
  endif()

  if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64")
    set(WILFRED_WV2_ARCH arm64)
  elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(WILFRED_WV2_ARCH x64)
  else()
    set(WILFRED_WV2_ARCH x86)
  endif()

  target_include_directories(wilfred_core PUBLIC
    ${microsoft_webview2_SOURCE_DIR}/build/native/include)
  target_link_libraries(wilfred_core PUBLIC
    ${microsoft_webview2_SOURCE_DIR}/build/native/${WILFRED_WV2_ARCH}/WebView2LoaderStatic.lib)
elseif(APPLE)
  target_link_libraries(wilfred_core PUBLIC
    "-framework Foundation"
    "-framework AppKit"
    "-framework Cocoa"
    "-framework CoreServices"
    "-framework ApplicationServices"
    "-framework Carbon"
    "-framework CoreFoundation"
    "-framework WebKit"
  )
else()
  find_package(Threads REQUIRED)
  find_package(X11)
  target_link_libraries(wilfred_core PUBLIC Threads::Threads)
  if(X11_FOUND)
    target_include_directories(wilfred_core PUBLIC ${X11_INCLUDE_DIR})
    target_link_libraries(wilfred_core PUBLIC ${X11_LIBRARIES})
    target_compile_definitions(wilfred_core PUBLIC WILFRED_HAS_X11=1)
  endif()
  target_link_libraries(wilfred_core PUBLIC dl)
endif()

if(WIN32)
  add_executable(wilfred WIN32 src/main.cpp)
else()
  add_executable(wilfred src/main.cpp)
endif()
target_link_libraries(wilfred PRIVATE wilfred_core)

add_custom_command(TARGET wilfred POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/ui/overlay
    $<TARGET_FILE_DIR:wilfred>/ui/overlay)

install(DIRECTORY ui/overlay DESTINATION share/wilfred/ui)

if(APPLE)
  set_target_properties(wilfred PROPERTIES
    MACOSX_BUNDLE FALSE
  )
endif()

set(WILFRED_TEST_SOURCES
  tests/test_main.cpp
  tests/test_yaml.cpp
  tests/test_config.cpp
  tests/test_tokenizer.cpp
  tests/test_fuzzy.cpp
  tests/test_rank.cpp
  tests/test_filter.cpp
  tests/test_math.cpp
  tests/test_classify.cpp
  tests/test_index.cpp
  tests/test_paths.cpp
  tests/test_history.cpp
  tests/test_apps.cpp
  tests/test_protocol.cpp
  tests/test_search_extras.cpp
)

if(WILFRED_BUILD_TESTS)
  add_executable(wilfred_tests ${WILFRED_TEST_SOURCES})
  target_link_libraries(wilfred_tests PRIVATE wilfred_core)
  target_include_directories(wilfred_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  enable_testing()
  add_test(NAME wilfred_tests COMMAND wilfred_tests)
endif()

if(WILFRED_BUILD_BENCH)
  add_executable(wilfred_bench benches/bench_main.cpp)
  target_link_libraries(wilfred_bench PRIVATE wilfred_core)
endif()

install(TARGETS wilfred RUNTIME DESTINATION bin)
install(FILES config/wilfred.default.yml DESTINATION share/wilfred)
