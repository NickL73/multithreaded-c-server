cmake_minimum_required(3.25)
project(PROJECT_NAME c_mutli_pingpong VERSION 1.0.0 LANGUAGES C)

set(BUILD_SHARED_LIBS OFF)
set(CMAKE_EXE_LINKER_FLAGS "-static")
add_compile_options(-Wall -Wextra -Wconversion -Wconversion -pedantic)

set(PROJECT_INSTALL_PATH ${PROJECT_SOURCE_DIR}/bin)
add_subdirectory(src)