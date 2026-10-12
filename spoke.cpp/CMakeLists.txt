cmake_minimum_required(VERSION 3.20)
project(spoke.cpp VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SPOKE_BUILD_TESTS "Build tests" ON)
option(SPOKE_BUILD_BENCH "Build benchmarks" ON)
option(SPOKE_BUILD_CLI "Build CLI" ON)
option(SPOKE_BUILD_SHARED "Build shared library for .NET P/Invoke" ON)
option(SPOKE_BUILD_EXAMPLES "Build examples" OFF)
option(SPOKE_ENABLE_MARCH_NATIVE "Use -march=native where supported (disable for distributable binaries)" ON)

# --- Platform / install layout ------------------------------------------------
# CMAKE_INSTALL_LIBDIR etc. come from GNUInstallDirs so `lib` vs `lib64`
# and multiarch paths are correct on every Linux distro and Homebrew layout.
include(GNUInstallDirs)
set(CMAKE_MACOSX_RPATH ON)
# Installed binaries find sibling shared libs without LD_LIBRARY_PATH hacks.
if(APPLE)
  set(CMAKE_INSTALL_RPATH "@loader_path;@loader_path/../${CMAKE_INSTALL_LIBDIR}")
elseif(UNIX)
  set(CMAKE_INSTALL_RPATH "$ORIGIN;$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
endif()

# --- Optimization flags -------------------------------------------------------
if(MSVC)
  add_compile_options(/W3 /EHsc /O2 /Ob2 /Oi /Ot /GL-)
  add_compile_definitions(_USE_MATH_DEFINES NOMINMAX)
else()
  add_compile_options(-O3 -Wall -Wextra -ffast-math -funroll-loops)
  # -march=native is rejected by AppleClang on some Xcode versions and is
  # wrong for distributable binaries; keep it opt-out instead of always-on.
  if(SPOKE_ENABLE_MARCH_NATIVE AND NOT APPLE AND NOT CMAKE_CROSSCOMPILING)
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag("-march=native" SPOKE_HAS_MARCH_NATIVE)
    if(SPOKE_HAS_MARCH_NATIVE)
      add_compile_options(-march=native)
    endif()
  endif()
  # Helpful, low-noise warnings on GCC/Clang without breaking -Wall builds.
  include(CheckCXXCompilerFlag)
  check_cxx_compiler_flag("-Wshadow" SPOKE_HAS_WSHADOW)
  if(SPOKE_HAS_WSHADOW)
    add_compile_options(-Wshadow)
  endif()
endif()

# --- Library ------------------------------------------------------------------
set(SPOKE_HEADERS
  include/spoke/version.h
  include/spoke/status.h
  include/spoke/config.h
  include/spoke/platform.h
  include/spoke/audio.h
  include/spoke/fft.h
  include/spoke/tensor.h
  include/spoke/features.h
  include/spoke/quant.h
  include/spoke/threading.h
  include/spoke/model.h
  include/spoke/tokenizer.h
  include/spoke/decoder.h
  include/spoke/vad.h
  include/spoke/transcriber.h
  include/spoke/api.h
)

set(SPOKE_SOURCES
  src/audio.cpp
  src/config.cpp
  src/fft.cpp
  src/features.cpp
  src/platform.cpp
  src/quant.cpp
  src/status.cpp
  src/tensor.cpp
  src/threading.cpp
  src/model.cpp
  src/tokenizer.cpp
  src/decoder.cpp
  src/vad.cpp
  src/transcriber.cpp
  src/api.cpp
)

add_library(spoke STATIC ${SPOKE_SOURCES} ${SPOKE_HEADERS})
target_include_directories(spoke PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_features(spoke PUBLIC cxx_std_17)
set_target_properties(spoke PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(SPOKE_BUILD_SHARED)
  add_library(spoke_shared SHARED ${SPOKE_SOURCES})
  target_include_directories(spoke_shared PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
  target_compile_definitions(spoke_shared PRIVATE SPOKE_BUILDING_DLL)
  # libspoke.so.0 / libspoke.0.dylib versioning; plain libspoke.so symlink kept.
  set_target_properties(spoke_shared PROPERTIES
    OUTPUT_NAME "spoke"
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON)
endif()

# Threads (pthreads on Linux/macOS, Win32 threads via MSVC CRT on Windows)
find_package(Threads REQUIRED)
target_link_libraries(spoke PUBLIC Threads::Threads)
if(SPOKE_BUILD_SHARED)
  target_link_libraries(spoke_shared PUBLIC Threads::Threads)
endif()

# On some older Linux toolchains libm must be linked explicitly.
if(UNIX AND NOT APPLE)
  target_link_libraries(spoke PUBLIC m)
  if(SPOKE_BUILD_SHARED)
    target_link_libraries(spoke_shared PUBLIC m)
  endif()
endif()

# --- CLI ----------------------------------------------------------------------
if(SPOKE_BUILD_CLI)
  add_executable(spoke_cli tools/spoke_cli.cpp)
  target_link_libraries(spoke_cli PRIVATE spoke)
endif()

# --- Tests --------------------------------------------------------------------
if(SPOKE_BUILD_TESTS)
  enable_testing()
  add_executable(spoke_tests tests/test_all.cpp)
  target_link_libraries(spoke_tests PRIVATE spoke)
  add_test(NAME spoke_tests COMMAND spoke_tests)
endif()

# --- Benchmarks ----------------------------------------------------------------
if(SPOKE_BUILD_BENCH)
  add_executable(spoke_bench bench/bench_main.cpp)
  target_link_libraries(spoke_bench PRIVATE spoke)
endif()

# --- Examples ------------------------------------------------------------------
if(SPOKE_BUILD_EXAMPLES)
  add_executable(spoke_streaming_stdin examples/streaming_stdin.cpp)
  target_link_libraries(spoke_streaming_stdin PRIVATE spoke)
endif()

# --- pkg-config (Linux/macOS developers: `pkg-config --cflags --libs spoke`) ---
configure_file(packaging/spoke.pc.in spoke.pc @ONLY)

# --- Install --------------------------------------------------------------------
install(TARGETS spoke
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
if(SPOKE_BUILD_SHARED)
  install(TARGETS spoke_shared
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
if(SPOKE_BUILD_CLI)
  install(TARGETS spoke_cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/spoke.pc
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

# --- Packages (CPack: TGZ everywhere, plus DEB/RPM on Linux, DragNDrop on macOS) -
set(CPACK_PACKAGE_NAME "spoke")
set(CPACK_PACKAGE_VENDOR "Spoke Contributors")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "From-scratch offline English transcription engine")
set(CPACK_PACKAGE_CONTACT "spoke contributors")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
if(UNIX AND NOT APPLE)
  set(CPACK_GENERATOR "TGZ;DEB")
  set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Spoke Contributors")
  set(CPACK_DEBIAN_PACKAGE_SECTION "sound")
elseif(APPLE)
  set(CPACK_GENERATOR "TGZ;DragNDrop")
elseif(WIN32)
  set(CPACK_GENERATOR "ZIP")
endif()
include(CPack)
