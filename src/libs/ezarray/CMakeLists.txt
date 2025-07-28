add_library(ezarray STATIC ezarray.c)
target_include_directories(ezarray PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ezarray PRIVATE common)