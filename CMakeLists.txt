cmake_minimum_required(VERSION 3.25)
project(c_mutli_pingpong VERSION 1.0.0 LANGUAGES C)

set(BUILD_SHARED_LIBS OFF)
#set(CMAKE_EXE_LINKER_FLAGS "-static")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_compile_options(-Wall -Wextra -Wconversion -Wconversion -pedantic -g)

set(PROJECT_INSTALL_PATH ${PROJECT_SOURCE_DIR}/bin)
add_subdirectory(src)