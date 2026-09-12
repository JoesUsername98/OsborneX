file(GLOB_RECURSE BOT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE BOT_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/inc/Bot/*.hpp")
file(GLOB_RECURSE BOT_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp")

add_library(Bot STATIC ${BOT_SOURCES} ${BOT_HEADERS})
target_include_directories(Bot PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/inc")
target_link_libraries(Bot PUBLIC Net)
target_compile_features(Bot PUBLIC cxx_std_23)
osbornex_set_warnings(Bot)

add_executable(BotTest ${BOT_TEST_SOURCES})
target_link_libraries(BotTest PRIVATE Bot TestSupport GTest::gtest_main)
osbornex_set_warnings(BotTest)
if(UNIX AND NOT APPLE)
    target_link_libraries(BotTest PRIVATE pthread)
endif()
include(GoogleTest)
gtest_discover_tests(BotTest)

add_executable(BotMain apps/bot_main.cpp)
target_link_libraries(BotMain PRIVATE Bot)
osbornex_set_warnings(BotMain)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/inc/Bot"
             PREFIX "Header Files" FILES ${BOT_HEADERS})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src"
             PREFIX "Source Files" FILES ${BOT_SOURCES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/test"
             PREFIX "Source Files" FILES ${BOT_TEST_SOURCES})
