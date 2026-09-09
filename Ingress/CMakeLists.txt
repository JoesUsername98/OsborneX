file(GLOB_RECURSE INGRESS_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE INGRESS_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/inc/Ingress/*.hpp")
file(GLOB_RECURSE INGRESS_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp")

add_library(Ingress STATIC ${INGRESS_SOURCES} ${INGRESS_HEADERS})
target_include_directories(Ingress PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/inc")
target_link_libraries(Ingress PUBLIC Messages)
target_compile_features(Ingress PUBLIC cxx_std_23)
osbornex_set_warnings(Ingress)

add_executable(IngressTest ${INGRESS_TEST_SOURCES})
target_link_libraries(IngressTest PRIVATE Ingress GTest::gtest_main)
osbornex_set_warnings(IngressTest)
if(UNIX AND NOT APPLE)
    target_link_libraries(IngressTest PRIVATE pthread)
endif()
include(GoogleTest)
gtest_discover_tests(IngressTest)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/inc/Ingress"
             PREFIX "Header Files" FILES ${INGRESS_HEADERS})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src"
             PREFIX "Source Files" FILES ${INGRESS_SOURCES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/test"
             PREFIX "Source Files" FILES ${INGRESS_TEST_SOURCES})
