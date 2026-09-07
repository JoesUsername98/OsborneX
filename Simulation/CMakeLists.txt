file(GLOB_RECURSE SIMULATION_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE SIMULATION_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/inc/Simulation/*.hpp")
file(GLOB_RECURSE SIMULATION_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp")

add_library(Simulation STATIC ${SIMULATION_SOURCES} ${SIMULATION_HEADERS})
target_include_directories(Simulation PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/inc")
target_link_libraries(Simulation PUBLIC Orderbook readerwriterqueue concurrentqueue)
target_compile_features(Simulation PUBLIC cxx_std_23)
osbornex_set_warnings(Simulation)

add_executable(SimulationTest ${SIMULATION_TEST_SOURCES})
target_link_libraries(SimulationTest PRIVATE Simulation GTest::gtest_main)
osbornex_set_warnings(SimulationTest)
include(GoogleTest)
gtest_discover_tests(SimulationTest)

add_executable(SimulationMain apps/simulation_main.cpp)
target_link_libraries(SimulationMain PRIVATE Simulation)
osbornex_set_warnings(SimulationMain)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/inc/Simulation"
             PREFIX "Header Files" FILES ${SIMULATION_HEADERS})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src"
             PREFIX "Source Files" FILES ${SIMULATION_SOURCES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/test"
             PREFIX "Source Files" FILES ${SIMULATION_TEST_SOURCES})
