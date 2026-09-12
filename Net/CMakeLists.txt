file(GLOB_RECURSE NET_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE NET_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/inc/Net/*.hpp")
file(GLOB_RECURSE NET_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp")

add_library(Net STATIC ${NET_SOURCES} ${NET_HEADERS})
target_include_directories(Net PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/inc")
target_link_libraries(Net PUBLIC Messages)
target_compile_features(Net PUBLIC cxx_std_23)
osbornex_set_warnings(Net)

if(WIN32)
    target_link_libraries(Net PUBLIC ws2_32)
endif()

add_executable(NetTest ${NET_TEST_SOURCES})
target_link_libraries(NetTest PRIVATE Net TestSupport GTest::gtest_main)
osbornex_set_warnings(NetTest)
if(UNIX AND NOT APPLE)
    target_link_libraries(NetTest PRIVATE pthread)
endif()
include(GoogleTest)
gtest_discover_tests(NetTest)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/inc/Net"
             PREFIX "Header Files" FILES ${NET_HEADERS})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src"
             PREFIX "Source Files" FILES ${NET_SOURCES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/test"
             PREFIX "Source Files" FILES ${NET_TEST_SOURCES})
