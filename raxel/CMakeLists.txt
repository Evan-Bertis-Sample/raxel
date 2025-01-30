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

add_executable(${PROJECT})

# --- Do some initial checking ---

if(NOT DEFINED PROJECT)
    message(FATAL_ERROR "PROJECT not defined.")
endif()

if(NOT DEFINED RAXEL_PROJECT_ROOT_DIR)
    message(FATAL_ERROR "RAXEL_PROJECT_ROOT_DIR not defined.")
endif()

# check there is a raxel.cmake file in the project root directory
if(NOT EXISTS ${RAXEL_PROJECT_ROOT_DIR}/raxel.cmake)
    message(FATAL_ERROR "raxel.cmake not found in project root directory.")
endif()

# --- COMPILING RAXEL ENGINE ---

message(STATUS "Compiling Raxel Engine")

set(RAXEL_ENGINE_DIR ${CMAKE_SOURCE_DIR})

# collect all core .c and .h files recursively
file(GLOB_RECURSE RAXEL_CORE_SOURCES ${RAXEL_ENGINE_DIR}/core/*.c)
file(GLOB_RECURSE RAXEL_CORE_HEADERS ${RAXEL_ENGINE_DIR}/core/*.h)

# print out the files
message(STATUS "Raxel Core Sources: ${RAXEL_CORE_SOURCES}")
message(STATUS "Raxel Core Headers: ${RAXEL_CORE_HEADERS}")

add_library(raxel STATIC 
    ${RAXEL_CORE_SOURCES} 
    ${RAXEL_CORE_HEADERS}
)

# sets where the raxel engine is located, and how to include it
# I think that being a bit more explicit about the include directories is best
# so you have to do <raxel/...> instead of just <...>
# or if you just want to do <...> just place the inlcude file
# in raxel/include
set(RAXEL_PARENT_DIR ${RAXEL_ENGINE_DIR}/..)

# set raxel to be a c99 library
set_property(TARGET raxel PROPERTY C_STANDARD 99)

# --- Fetching Dependencies (Vulkan, GLFW, GLM, etc) ---

include(FetchContent)

set(RAXEL_ENGINE_VENDOR_DIR ${RAXEL_ENGINE_DIR}/vendor)

message(STATUS "Vendor Directory: ${RAXEL_ENGINE_VENDOR_DIR}")

# Vulkan is a required dependency.
include(FindVulkan)
if(NOT VULKAN_FOUND)
    message(FATAL_ERROR "Vulkan SDK not installed.")
endif()

# now add GLFW and GLM
# I just put them as gitsubmodules, no cmake magic here
# glfw
set(GLFW_BUILD_DOCS OFF CACHE BOOL  "GLFW lib only")
set(GLFW_INSTALL OFF CACHE BOOL  "GLFW lib only")
set(GLAD_GL "")
set(GLFW_LIBRARY_TYPE "STATIC")
add_subdirectory(${RAXEL_ENGINE_VENDOR_DIR}/glfw)
# glm
set(GLM_BUILD_LIBRARY ON CACHE BOOL "GLM lib only")
add_subdirectory(${RAXEL_ENGINE_VENDOR_DIR}/glm)

# Include directories
target_include_directories(raxel 
    PUBLIC ${RAXEL_PARENT_DIR}
    PRIVATE ${Vulkan_INCLUDE_DIRS} libs/glfw/include
)

target_link_libraries(raxel 
    Vulkan::Vulkan
    glm glfw
)

# --- COMPILING GAME ---

message(STATUS "Compiling Game")
include(${RAXEL_PROJECT_ROOT_DIR}/raxel.cmake)

# link the game with the engine
target_link_libraries(${PROJECT} PUBLIC raxel)

get_target_property(INCLUDE_DIRS raxel INCLUDE_DIRECTORIES)
message(STATUS "Raxel Include Directories: ${INCLUDE_DIRS}")
