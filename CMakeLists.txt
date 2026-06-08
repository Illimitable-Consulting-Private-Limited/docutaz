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
set(PROJECT_VERSION_MINOR "0")
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
# Boost is used header-only (filesystem/date_time calls are inline; MSVC
# auto-linking is disabled via -DBOOST_ALL_NO_LIB). We only need the headers
# on the include path — explicit discovery makes this work where Boost is not
# in the compiler's default search path (e.g. MSVC on Windows).
find_package(Boost REQUIRED)
find_package(Qt5Core REQUIRED)
find_package(Qt5Gui REQUIRED)
find_package(Qt5Widgets REQUIRED)
find_package(Qt5PrintSupport REQUIRED)
find_package(Qt5Network REQUIRED)
find_package(Qt5Xml REQUIRED)
find_package(OpenSSL REQUIRED)

if (SYSTEM_MACOSX)
    # arm64 (Apple Silicon) requires a minimum deployment target of 11.0.
    # Allow the value to be overridden from the command line (-D...).
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET 11.0)
    endif()
    find_package(Qt5MacExtras REQUIRED)
endif()

# Projects
set(QSCINTILLA_VERSION 2.8.4)
set(QSCINTILLA_DIR src/third-party/qscintilla-${QSCINTILLA_VERSION})
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
add_subdirectory(src/docutaz/ssh)
add_subdirectory(${QSCINTILLA_DIR})
add_subdirectory(src/docutaz)

# Unit tests (and the GoogleTest dependency they need) are optional. They are
# not part of release builds; CI configures with -DDOCUTAZ_BUILD_TESTS=OFF.
option(DOCUTAZ_BUILD_TESTS "Build unit tests" ON)
if(DOCUTAZ_BUILD_TESTS)
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
    add_subdirectory(src/tests EXCLUDE_FROM_ALL)
endif()

# Show configuration summary
include(DocutazConfigurationSummary)
