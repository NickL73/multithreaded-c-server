add_library(protocols STATIC pong.c)
target_include_directories(protocols PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(protocols PRIVATE utils)