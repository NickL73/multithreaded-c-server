# Make the utilitiy macros available for both the main target and libraries
add_library(common INTERFACE)
target_include_directories(common INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# Build the libraries the server  will use
add_subdirectory(cards)
add_subdirectory(concurinc)
add_subdirectory(libs)

# Build the main server
set(PROJECT_TARGET pingpong-${CMAKE_SYSTEM_PROCESSOR})
add_executable(${PROJECT_TARGET} main.c)
target_link_libraries(${PROJECT_TARGET} PRIVATE pthread netlib ezqueue connmgr common cards coin)

set_target_properties(${PROJECT_TARGET} PROPERTIES DEBUG_POSTFIX "-debug" RELEASE_POSTFIX "-release")
install(TARGETS ${PROJECT_TARGET} DESTINATION ${PROJECT_INSTALL_PATH})