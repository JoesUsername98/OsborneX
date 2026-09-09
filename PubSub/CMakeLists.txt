file(GLOB_RECURSE PUBSUB_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE PUBSUB_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/inc/PubSub/*.hpp")
file(GLOB_RECURSE PUBSUB_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp")

add_library(PubSub STATIC ${PUBSUB_SOURCES} ${PUBSUB_HEADERS})
target_include_directories(PubSub PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/inc")
target_link_libraries(PubSub PUBLIC Messages Queue)
target_compile_features(PubSub PUBLIC cxx_std_23)
osbornex_set_warnings(PubSub)

add_executable(PubSubTest ${PUBSUB_TEST_SOURCES})
target_link_libraries(PubSubTest PRIVATE PubSub TestSupport GTest::gtest_main)
osbornex_set_warnings(PubSubTest)
include(GoogleTest)
gtest_discover_tests(PubSubTest)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/inc/PubSub"
             PREFIX "Header Files" FILES ${PUBSUB_HEADERS})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src"
             PREFIX "Source Files" FILES ${PUBSUB_SOURCES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/test"
             PREFIX "Source Files" FILES ${PUBSUB_TEST_SOURCES})
