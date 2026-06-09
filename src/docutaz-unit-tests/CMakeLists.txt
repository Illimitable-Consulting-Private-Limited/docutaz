#########################################
###   Unit Testing with Google Test   ###
#########################################

# The tests link the shared docutaz_core OBJECT library (defined in
# src/docutaz/CMakeLists.txt), which carries all of the application's sources,
# include directories, compile definitions and link dependencies as PUBLIC
# usage requirements. That replaces the old approach of scraping docutaz's
# compiled object files by hardcoded path (including fragile AUTOGEN hash
# directories), and lets the tests build and run on every platform — Linux
# included, where they used to be disabled outright.

enable_testing()

set(ROBO_SRC_DIR ${CMAKE_HOME_DIRECTORY}/src/docutaz)

# Test/source pairs. New tests follow the convention:
#   Test file:   /path/Foo_test.cpp   tested code: /path/Foo.{h,cpp}
set(SOURCES_TEST
    ${ROBO_SRC_DIR}/utils/DocutazCrypt_test.cpp
    ${ROBO_SRC_DIR}/utils/StringOperations_test.cpp
    ${ROBO_SRC_DIR}/core/HexUtils_test.cpp
)

add_executable(robo_unit_tests ${SOURCES_TEST})

target_include_directories(robo_unit_tests PRIVATE ${CMAKE_HOME_DIRECTORY}/src)

target_link_libraries(robo_unit_tests
    PRIVATE
        docutaz_core   # app sources + all their usage requirements (Qt, mongo, ssl, ssh, ...)
        gtest
        gtest_main)

if(SYSTEM_LINUX)
    # Mirror the application's rpath so the test binary finds the locally built
    # libmongocxx/libbson (and their transitive libmongoc2/libbson2) at runtime.
    set_target_properties(robo_unit_tests PROPERTIES BUILD_RPATH "/usr/local/lib64")
    target_link_options(robo_unit_tests PRIVATE "LINKER:--disable-new-dtags")
endif()

# Register the gtest binary as a single ctest entry. Running it executes every
# TEST(...) case; ctest is invoked after CI has exposed the runtime libraries.
add_test(NAME robo_unit_tests COMMAND robo_unit_tests)
