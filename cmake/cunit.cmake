function(Fetch_CUnit)
    include(FetchContent)

    FetchContent_Declare(
            cunit
            GIT_REPOSITORY https://gitlab.com/cunity/cunit.git
            GIT_TAG 9cf44aa3a37cfa5570dd70e122b09bfa6e711fee
            SOURCE_SUBDIR CUnit
    )

    message(STATUS "Fetching CUnit...")
    set(CUNIT_DISABLE_TESTS True)
    set(CUNIT_DISABLE_EXAMPLES True)
    FetchContent_MakeAvailable(cunit)
endfunction()

function(add_cunit_test test_name test_file lib_to_test)
    set(TEST_BIN_NAME ${test_name}-${CMAKE_SYSTEM_PROCESSOR})
    add_executable(${TEST_BIN_NAME} ${test_file})
    target_link_libraries(${TEST_BIN_NAME} PUBLIC cunit ${lib_to_test})
    add_test(NAME ${test_name} COMMAND ${TEST_BIN_NAME})
endfunction()