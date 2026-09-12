file(GLOB_RECURSE SERVER_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE SERVER_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/inc/Server/*.hpp")
file(GLOB_RECURSE SERVER_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp")

add_library(Server STATIC ${SERVER_SOURCES} ${SERVER_HEADERS})
target_include_directories(Server PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/inc")
target_link_libraries(Server PUBLIC Simulation Net Queue)
target_compile_features(Server PUBLIC cxx_std_23)
osbornex_set_warnings(Server)

add_executable(ServerTest ${SERVER_TEST_SOURCES})
target_link_libraries(ServerTest PRIVATE Server TestSupport GTest::gtest_main)
osbornex_set_warnings(ServerTest)
if(UNIX AND NOT APPLE)
    target_link_libraries(ServerTest PRIVATE pthread)
endif()
include(GoogleTest)
gtest_discover_tests(ServerTest)

add_executable(ServerMain apps/server_main.cpp)
target_link_libraries(ServerMain PRIVATE Server)
osbornex_set_warnings(ServerMain)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/inc/Server"
             PREFIX "Header Files" FILES ${SERVER_HEADERS})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src"
             PREFIX "Source Files" FILES ${SERVER_SOURCES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/test"
             PREFIX "Source Files" FILES ${SERVER_TEST_SOURCES})
