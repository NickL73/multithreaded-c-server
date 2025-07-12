add_library(netlib STATIC netio.c)
target_include_directories(netlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(netlib PRIVATE utils)