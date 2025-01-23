# raxel CMakeLists.txt
# this is placed in the root of the raxel engine, and
# is used to compile and link your game with the engine.

# so the the directory structure looks kinda like:
# raxel/
#   CMakeLists.txt
#   core/
#     ...header files...
#     ...source files...
#   include/
#     ...
#     header files for interacting with the engine

# somewhere else in your computer
# my_game/
#   raxel.cmake
#   whatever files you have

# Required Definitions:
# - PROJECT: The name of the project
# - RAXEL_PROJECT_ROOT_DIR: The root directory of the project,
#   this is where the raxel project (the game) is located.
#   Remember that the game is separate from the engine.


# --- CMAKE SETUP ---

cmake_minimum_required(VERSION 3.13)

project(${PROJECT} C CXX)

set(RAXEL_PROJECT_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(${PROJECT})

# --- COMPILING RAXEL ENGINE ---

message(STATUS "Compiling Raxel Engine")

set(RAXEL_ENGINE_DIR ${CMAKE_SOURCE_DIR})

# collect all core .c and .h files recursively
file(GLOB_RECURSE RAXEL_CORE_SOURCES ${RAXEL_ENGINE_DIR}/core/*.c)
file(GLOB_RECURSE RAXEL_CORE_HEADERS ${RAXEL_ENGINE_DIR}/core/*.h)


add_library(raxel STATIC 
    ${RAXEL_CORE_SOURCES} 
    ${RAXEL_CORE_HEADERS}
)