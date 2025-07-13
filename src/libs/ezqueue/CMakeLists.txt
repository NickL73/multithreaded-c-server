add_library(ezqueue STATIC ezqueue.c)
target_include_directories(ezqueue PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ezqueue PRIVATE utils)