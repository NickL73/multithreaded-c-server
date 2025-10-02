option(CARDS_BUILD_TEST_SUITE "Build the unit test suite for CARDS" OFF)
option(CARDS_BUILD_SHARED_LIBS "Build the library as a shared object" OFF)

add_subdirectory(src)
target_include_directories(cards PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

if (CARDS_BUILD_TEST_SUITE)
    add_subdirectory(tests)
endif ()