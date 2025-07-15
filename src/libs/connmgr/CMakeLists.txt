add_library(connmgr STATIC connmgr.c)
target_include_directories(connmgr PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(connmgr PUBLIC utils ezqueue ezarray pthread)