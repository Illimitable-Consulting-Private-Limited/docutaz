add_executable(bson_bridge_test EXCLUDE_FROM_ALL
    bson_bridge_roundtrip.cpp)

target_include_directories(bson_bridge_test PRIVATE
    ${CMAKE_HOME_DIRECTORY}/src
    ${CMAKE_HOME_DIRECTORY}/src/mongo_compat)

target_link_libraries(bson_bridge_test PRIVATE
    mongo::bsoncxx_shared)

if(SYSTEM_LINUX)
    set_target_properties(bson_bridge_test PROPERTIES
        BUILD_RPATH "/usr/local/lib64")
    target_link_options(bson_bridge_test PRIVATE "LINKER:--disable-new-dtags")
endif()
