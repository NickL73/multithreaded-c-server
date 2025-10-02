option(COIN_BUILD_TEST_SUITE "Build the unit test suite for COIN" OFF)

add_subdirectory(src)
target_include_directories(coin PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

if (COIN_BUILD_TEST_SUITE)
    add_subdirectory(tests)
endif ()