file(GLOB_RECURSE TESTSUPPORT_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/inc/TestSupport/*.hpp")

add_library(TestSupport INTERFACE ${TESTSUPPORT_HEADERS})
target_include_directories(TestSupport INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/inc")
target_compile_features(TestSupport INTERFACE cxx_std_23)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/inc/TestSupport"
             PREFIX "Header Files" FILES ${TESTSUPPORT_HEADERS})
