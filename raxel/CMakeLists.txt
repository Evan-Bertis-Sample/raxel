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


set(SFML_VERSION "2.6.0")
# Remove C++ stdlib flag for C compilation
if(APPLE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-command-line-argument")
endif()

# Fetch SFML
FetchContent_Declare(
    SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG "${SFML_VERSION}"
)
FetchContent_MakeAvailable(SFML)

# Configure paths before fetching CSFML
set(SFML_ROOT "${sfml_SOURCE_DIR}")
set(SFML_DIR "${sfml_BINARY_DIR}")
set(SFML_PATH "${sfml_SOURCE_DIR}")
set(SFML_INCLUDE_DIR "${sfml_SOURCE_DIR}/include")
set(SFML_LIBRARY_DIR "${sfml_BINARY_DIR}/lib")
set(SFML_DEPENDENCIES_DIR "${sfml_SOURCE_DIR}/extlibs")

# Create SFML config file that CSFML can find. This overwrites the already existing one because the original one causes conflict.
file(WRITE "${sfml_BINARY_DIR}/SFMLConfig.cmake" 
"set(SFML_VERSION ${SFML_VERSION})
set(SFML_INCLUDE_DIR \"${SFML_INCLUDE_DIR}\")
set(SFML_LIBRARY_DIR \"${SFML_LIBRARY_DIR}\")
set(SFML_DEPENDENCIES_DIR \"${SFML_DEPENDENCIES_DIR}\")
set(SFML_FOUND TRUE)
")

# Fetch CSFML
FetchContent_Declare(
    CSFML
    GIT_REPOSITORY https://github.com/SFML/CSFML.git
    GIT_TAG "${SFML_VERSION}"
)
FetchContent_MakeAvailable(CSFML)

# Link all CSFML libraries
target_link_libraries(raxel PRIVATE
    sfml-system
    sfml-window
    sfml-graphics
    sfml-audio
    sfml-network
    csfml-system
    csfml-window
    csfml-graphics
    csfml-audio
    csfml-network
)

# Include directories
target_include_directories(${PROJECT} PRIVATE
    ${csfml_SOURCE_DIR}/include
)


# set raxel to be a c99 library
set_property(TARGET raxel PROPERTY C_STANDARD 99)

# call the build_project function, which should be defined in the raxel.cmake file
set(RAXEL_PROJECT_CMAKE ${RAXEL_PROJECT_ROOT_DIR}/raxel.cmake)
if(EXISTS ${RAXEL_PROJECT_CMAKE})
    include(${RAXEL_PROJECT_CMAKE})
else()
    message(FATAL_ERROR "raxel.cmake not found in project root directory.")
endif()