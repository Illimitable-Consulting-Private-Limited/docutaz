#########################################
###   GUI Tests (Qt Test + Google Test) ###
#########################################

# GUI tests drive real widgets headlessly. They need a QApplication (created in
# main.cpp on the offscreen platform), so they live in their own executable
# rather than the logic-only unit-test binary (which uses gtest_main and no
# QApplication). They link the same docutaz_core OBJECT library, plus Qt6::Test
# for input simulation / QSignalSpy.

find_package(Qt6 REQUIRED COMPONENTS Test)

set(GUI_TEST_SOURCES
    main.cpp
    CopyResultsDialog_gui_test.cpp
    ExportResultsDialog_gui_test.cpp
)

add_executable(docutaz_gui_tests ${GUI_TEST_SOURCES})

target_include_directories(docutaz_gui_tests PRIVATE ${CMAKE_HOME_DIRECTORY}/src)

target_link_libraries(docutaz_gui_tests
    PRIVATE
        docutaz_core   # app sources + all their usage requirements
        gtest          # custom main() lives in main.cpp, so not gtest_main
        Qt6::Test)

if(SYSTEM_LINUX)
    # Mirror the application's rpath so the locally built libmongocxx/libbson
    # (transitively linked via docutaz_core) resolve at runtime.
    set_target_properties(docutaz_gui_tests PROPERTIES BUILD_RPATH "/usr/local/lib64")
    target_link_options(docutaz_gui_tests PRIVATE "LINKER:--disable-new-dtags")
endif()

add_test(NAME docutaz_gui_tests COMMAND docutaz_gui_tests)
