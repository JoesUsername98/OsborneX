file(GLOB_RECURSE TUI_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE TUI_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/inc/Tui/*.hpp")
file(GLOB_RECURSE TUI_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp")

# The testable core (MarketViewModel) deliberately has no FTXUI dependency --
# only the TuiMain executable below links the terminal-rendering library.
add_library(Tui STATIC ${TUI_SOURCES} ${TUI_HEADERS})
target_include_directories(Tui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/inc")
target_link_libraries(Tui PUBLIC Net)
target_compile_features(Tui PUBLIC cxx_std_23)
osbornex_set_warnings(Tui)

add_executable(TuiTest ${TUI_TEST_SOURCES})
target_link_libraries(TuiTest PRIVATE Tui TestSupport GTest::gtest_main)
osbornex_set_warnings(TuiTest)
if(UNIX AND NOT APPLE)
    target_link_libraries(TuiTest PRIVATE pthread)
endif()
include(GoogleTest)
gtest_discover_tests(TuiTest)

add_executable(TuiMain apps/tui_main.cpp)
target_link_libraries(TuiMain PRIVATE Tui ftxui::screen ftxui::dom ftxui::component)
osbornex_set_warnings(TuiMain)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/inc/Tui"
             PREFIX "Header Files" FILES ${TUI_HEADERS})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src"
             PREFIX "Source Files" FILES ${TUI_SOURCES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/test"
             PREFIX "Source Files" FILES ${TUI_TEST_SOURCES})
