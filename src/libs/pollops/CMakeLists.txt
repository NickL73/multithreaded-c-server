add_library(pollops STATIC pollops.c)
target_include_directories(pollops PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pollops PRIVATE utils)