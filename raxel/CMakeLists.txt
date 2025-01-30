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

# --- Fetching Dependencies (Vulkan, GLFW, etc) ---

# Vulkan is a required dependency.
include(FindVulkan)
if(NOT VULKAN_FOUND)
    message(FATAL_ERROR "Vulkan SDK not installed.")
endif()

# GLFW is a required dependency.
set(GLFW_VERSION 3.3.8)
include(FetchContent)
FetchContent_Declare(
    glfw
    URL https://github.com/glfw/glfw/archive/refs/tags/${GLFW_VERSION}.tar.gz)

include(FetchContent)
FetchContent_Declare(CSFML
    GIT_REPOSITORY https://github.com/SFML/CSFML.git
    GIT_TAG 3.0.0
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL
    SYSTEM)
FetchContent_MakeAvailable(CSFML)


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

# set raxel to be a c99 library
set_property(TARGET raxel PROPERTY C_STANDARD 99)

# call the build_project function, which should be defined in the raxel.cmake file
set(RAXEL_PROJECT_CMAKE ${RAXEL_PROJECT_ROOT_DIR}/raxel.cmake)

build_project()