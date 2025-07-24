include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
set(COIN_SRC_FILES
        src/coin_queue.c
        src/concurinc.c
)

add_library(concurinc STATIC ${COIN_SRC_FILES})
target_include_directories(concurinc PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(concurinc PUBLIC pthread)