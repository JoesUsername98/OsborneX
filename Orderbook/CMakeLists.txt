file(GLOB_RECURSE ORDERBOOK_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE ORDERBOOK_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/inc/Orderbook/*.hpp")
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp")
file(GLOB_RECURSE BENCH_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/bench/*.cpp")

add_library(Orderbook STATIC ${ORDERBOOK_SOURCES} ${ORDERBOOK_HEADERS})
target_include_directories(Orderbook PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/inc")
target_compile_features(Orderbook PUBLIC cxx_std_23)
osbornex_set_warnings(Orderbook)

add_executable(OrderbookTest ${TEST_SOURCES})
target_link_libraries(OrderbookTest PRIVATE Orderbook GTest::gtest_main)
osbornex_set_warnings(OrderbookTest)
include(GoogleTest)
gtest_discover_tests(OrderbookTest)

add_executable(OrderbookBench ${BENCH_SOURCES})
target_link_libraries(OrderbookBench PRIVATE Orderbook benchmark::benchmark)
osbornex_set_warnings(OrderbookBench)
if(UNIX AND NOT APPLE)
    target_link_libraries(OrderbookBench PRIVATE pthread)
endif()

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/inc/Orderbook"
             PREFIX "Header Files" FILES ${ORDERBOOK_HEADERS})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src"
             PREFIX "Source Files" FILES ${ORDERBOOK_SOURCES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/test"
             PREFIX "Source Files" FILES ${TEST_SOURCES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/bench"
             PREFIX "Source Files" FILES ${BENCH_SOURCES})
