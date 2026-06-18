cmake_minimum_required(VERSION 3.16)
project(Docutaz)

# Find mongocxx/bsoncxx BEFORE adding our custom cmake/ module path.
# Our cmake/FindOpenSSL.cmake conflicts with the system finder that
# mongocxx uses internally via find_dependency(OpenSSL).
#
# When a vcpkg toolchain is active it manages CMAKE_PREFIX_PATH itself.
# Otherwise fall back to the default local-install path (/usr/local/lib64).
if(NOT CMAKE_TOOLCHAIN_FILE)
    list(APPEND CMAKE_PREFIX_PATH "/usr/local/lib64/cmake")
endif()
find_package(mongocxx REQUIRED)
find_package(bsoncxx REQUIRED)

# Where to look first for CMake modules, before "${CMAKE_ROOT}/Modules" is checked
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

# Project version
set(PROJECT_VERSION_MAJOR "2")
set(PROJECT_VERSION_MINOR "3")
set(PROJECT_VERSION_PATCH "0")
set(PROJECT_VERSION_BUILD "")
set(PROJECT_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}${PROJECT_VERSION_BUILD}")

# Enable C++17 features 
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Modules
include(DocutazPrintUtils)
include(DocutazCMakeDefaults)
include(DocutazDefaults)
include(DocutazCommon)
include(DocutazTrashSymbols)
include(DocutazTargetArch)
include(DocutazInstallQt)
include(DocutazPackage)

# Search for dependencies
find_package(Threading REQUIRED)    # Wrapper arround CMake "Threads" module
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets PrintSupport Network Xml)
find_package(OpenSSL REQUIRED)

if (SYSTEM_MACOSX)
    # arm64 (Apple Silicon) requires a minimum deployment target of 11.0.
    # Allow the value to be overridden from the command line (-D...).
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET 11.0)
    endif()
    # Qt6 dropped QtMacExtras; the macOS-specific calls it provided are gone
    # (see the source migration).
endif()

# Projects
set(GOOGLE_TEST_VERSION 1.15.2)
set(GOOGLE_TEST_COMMIT b514bdc898e2951020cbdca1304b75f5950d1f59)  # v1.15.2
# libssh2 is provided by the platform package manager instead of being
# vendored, so it always tracks upstream security fixes:
#   Linux   - system libssh2         (apt: libssh2-1-dev,  via pkg-config)
#   macOS   - Homebrew libssh2       (brew: libssh2,       via pkg-config)
#   Windows - vcpkg libssh2 (pinned) (CMake config: Libssh2::libssh2)
# Each path is wrapped in an INTERFACE target named "libssh2" so the rest of
# the build (src/docutaz/ssh) stays platform-agnostic. LIBSSH2_VERSION is set
# from the resolved package and surfaced in the About dialog.
if(SYSTEM_LINUX OR SYSTEM_MACOSX)
    find_package(PkgConfig REQUIRED)
    # IMPORTED_TARGET (PkgConfig::LIBSSH2) carries the absolute library path,
    # link dirs and include dirs. This matters on macOS: Homebrew's libssh2 is
    # not in the default linker search path, so a bare "-lssh2" fails to link.
    pkg_check_modules(LIBSSH2 REQUIRED IMPORTED_TARGET libssh2)
    add_library(libssh2 INTERFACE IMPORTED GLOBAL)
    target_link_libraries(libssh2 INTERFACE PkgConfig::LIBSSH2)
else()
    find_package(Libssh2 CONFIG REQUIRED)
    add_library(libssh2 INTERFACE IMPORTED GLOBAL)
    target_link_libraries(libssh2 INTERFACE Libssh2::libssh2)
    set(LIBSSH2_VERSION "${Libssh2_VERSION}")
endif()
# QScintilla (Qt6 editor component) is provided per platform instead of being
# vendored, mirroring the libssh2 approach so it tracks upstream fixes:
#   Linux   - system          (apt: libqscintilla2-qt6-dev / dnf: qscintilla-qt6-devel)
#   macOS   - Homebrew         (brew: qscintilla2)
#   Windows - built from source against the aqtinstall Qt6 (see CI), installed
#             into the Qt prefix so the same find_path/find_library locate it.
# Resolved uniformly via find_path/find_library and wrapped in an INTERFACE
# target named "qscintilla" so src/docutaz stays platform-agnostic. The version
# shown in the About dialog comes from QSCINTILLA_VERSION_STR in
# <Qsci/qsciglobal.h>; QSCINTILLA_VERSION here is parsed from the same header
# purely for the configuration summary.
find_path(QSCINTILLA_INCLUDE_DIR
    NAMES Qsci/qsciscintilla.h
    PATH_SUFFIXES qt6
    REQUIRED)
find_library(QSCINTILLA_LIBRARY NAMES qscintilla2_qt6 REQUIRED)
add_library(qscintilla INTERFACE IMPORTED GLOBAL)
target_include_directories(qscintilla INTERFACE ${QSCINTILLA_INCLUDE_DIR})
target_link_libraries(qscintilla INTERFACE
    ${QSCINTILLA_LIBRARY} Qt6::Widgets Qt6::PrintSupport)
if(WIN32)
    # The Windows QScintilla is built from source as a DLL (see CI). Its headers
    # only mark exported symbols __declspec(dllimport) when QSCINTILLA_DLL is
    # defined; without it, data symbols such as QsciScintilla::staticMetaObject
    # are treated as local and fail to link (function imports get auto-thunked,
    # data imports do not). Define it for every consumer of this target.
    target_compile_definitions(qscintilla INTERFACE QSCINTILLA_DLL)
endif()
if(QSCINTILLA_INCLUDE_DIR AND EXISTS "${QSCINTILLA_INCLUDE_DIR}/Qsci/qsciglobal.h")
    file(STRINGS "${QSCINTILLA_INCLUDE_DIR}/Qsci/qsciglobal.h" _qsci_ver
         REGEX "QSCINTILLA_VERSION_STR")
    string(REGEX MATCH "\"([0-9.]+)\"" _ "${_qsci_ver}")
    set(QSCINTILLA_VERSION "${CMAKE_MATCH_1}")
endif()

add_subdirectory(src/docutaz/ssh)

# libxlsxwriter (BSD-2) — vendored in-tree for .xlsx export. Self-contained: it
# bundles minizip and links the system zlib (already a dependency), so it needs
# no Qt-private headers. Build just the static lib (the `xlsxwriter` target).
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_FUZZERS OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/libxlsxwriter)

add_subdirectory(src/docutaz)

# Unit tests (and the GoogleTest dependency they need) are optional. They are
# not part of release builds; CI configures with -DDOCUTAZ_BUILD_TESTS=OFF.
option(DOCUTAZ_BUILD_TESTS "Build unit tests" ON)
if(DOCUTAZ_BUILD_TESTS)
    # enable_testing() must run in the top-level scope so `ctest` from the build
    # root discovers tests that add_test() registers in the subdirectories below.
    enable_testing()
    # GoogleTest is fetched (pinned to the v${GOOGLE_TEST_VERSION} commit) only
    # when tests are enabled; CI sets -DDOCUTAZ_BUILD_TESTS=OFF and never
    # downloads it. No vendored googletest tree is kept in the repo.
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        ${GOOGLE_TEST_COMMIT}
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)  # MSVC: link the dynamic CRT
    FetchContent_MakeAvailable(googletest)
    add_subdirectory(src/docutaz-unit-tests)
    add_subdirectory(src/docutaz-gui-tests)
endif()

# Show configuration summary
include(DocutazConfigurationSummary)
