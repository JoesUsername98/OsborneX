file(GLOB_RECURSE MESSAGES_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/inc/Messages/*.hpp")

add_library(Messages INTERFACE ${MESSAGES_HEADERS})
target_include_directories(Messages INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/inc")
target_link_libraries(Messages INTERFACE Orderbook)
target_compile_features(Messages INTERFACE cxx_std_23)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/inc/Messages"
             PREFIX "Header Files" FILES ${MESSAGES_HEADERS})
