file(GLOB_RECURSE QUEUE_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/inc/Queue/*.hpp")
file(GLOB_RECURSE QUEUE_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp")

add_library(Queue INTERFACE ${QUEUE_HEADERS})
target_include_directories(Queue INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/inc")
target_compile_features(Queue INTERFACE cxx_std_23)

add_executable(QueueTest ${QUEUE_TEST_SOURCES})
target_link_libraries(QueueTest PRIVATE Queue TestSupport GTest::gtest_main)
osbornex_set_warnings(QueueTest)
if(UNIX AND NOT APPLE)
    target_link_libraries(QueueTest PRIVATE pthread)
endif()
include(GoogleTest)
gtest_discover_tests(QueueTest)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/inc/Queue"
             PREFIX "Header Files" FILES ${QUEUE_HEADERS})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/test"
             PREFIX "Source Files" FILES ${QUEUE_TEST_SOURCES})
