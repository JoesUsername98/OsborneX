file(GLOB_RECURSE SHARDING_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE SHARDING_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/inc/Sharding/*.hpp")
file(GLOB_RECURSE SHARDING_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp")

add_library(Sharding STATIC ${SHARDING_SOURCES} ${SHARDING_HEADERS})
target_include_directories(Sharding PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/inc")
target_link_libraries(Sharding PUBLIC Orderbook Messages Queue)
target_compile_features(Sharding PUBLIC cxx_std_23)
osbornex_set_warnings(Sharding)

add_executable(ShardingTest ${SHARDING_TEST_SOURCES})
target_link_libraries(ShardingTest PRIVATE Sharding TestSupport GTest::gtest_main)
osbornex_set_warnings(ShardingTest)
include(GoogleTest)
gtest_discover_tests(ShardingTest)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/inc/Sharding"
             PREFIX "Header Files" FILES ${SHARDING_HEADERS})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src"
             PREFIX "Source Files" FILES ${SHARDING_SOURCES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/test"
             PREFIX "Source Files" FILES ${SHARDING_TEST_SOURCES})
