```
██████╗  █████╗ ██╗  ██╗███████╗██╗     
██╔══██╗██╔══██╗╚██╗██╔╝██╔════╝██║     
██████╔╝███████║ ╚███╔╝ █████╗  ██║     
██╔══██╗██╔══██║ ██╔██╗ ██╔══╝  ██║
██║  ██║██║  ██║██╔╝ ██╗███████╗███████╗
╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝╚══════╝╚══════╝
```
# raxel

**A Raymarched Voxel Engine Developed with Vulkan, in C99**
```
Author: Evan Bertis-Sample
Date: 3/21/2025
Course: COMP_SCI 499 (Prof. Geisler)
```

# Overview

This report covers the development of `raxel` within the past quarter. These developments include, but are not limited to:

* The implementation of a Vulkan-based rendering pipeline, that relies on compute shaders for raymarching.
* The development of a voxel data structure that is used to store and render voxel data.
* The implementation of a basic input system that allows for user interaction with the engine.
* The creation of a basic logging system that allows for debugging and logging of engine events.
* The development of a basic windowing system that allows for the creation of windows and surfaces.
* The implementation of a basic memory allocator that is used throughout the engine.
* The creation of a command line interface that allows for easy building and running of the engine, and for the creation of new projects.


# Table of Contents

- [Engine](#engine)
  - [Objectives](#objectives)
  - [High Level Description](#high-level-description)
  - [Notable Design Choices](#notable-design-choices)
- [Rendering Systems](#rendering-systems)
  - [Core Renderer](#core-renderer)
    - [Compute Shader-Driven Graphics Pipeline](#compute-shader-driven-graphics-pipeline)
    - [Abstractions of the Core Renderer](#abstractions-of-the-core-renderer)
  - [Voxel Renderer](#voxel-renderer)
    - [Objectives of the Voxel Renderer](#objectives-of-the-voxel-renderer)
    - [Voxels in Memory](#voxels-in-memory)
    - [Memory Management](#memory-management)
    - [Acceleration Structures (BVH)](#acceleration-structures-bvh)
    - [Rendering Voxels in Compute Shader](#rendering-voxels-in-compute-shader)
- [Demonstration](#demonstration)
  - [Building and Running the Demo](#building-and-running-the-demo)
- [Reflections \& Further Work](#reflections--further-work)
  - [Workflow of using my own Engine](#workflow-of-using-my-own-engine)
  - [Plans for the Spring](#plans-for-the-spring)



# Engine

`raxel` is split into two core parts, the *engine* and the *rendering systems.*

The engine encapsulates the `raxel` that aren't interacting directly with the Vulkan code. Rather, the engine is concerned with building an abstraction around the rendering systems, providing tooling, and making the developer experience better. It also provides many utilities that would be fundamental to building games and technical demos.

Whereas the rendering systems is the portion of `raxel` that directly interact with graphics drivers and utilities, and the abstractions around them.

As of writing this report, the responsibilities of the engine and the rendering systems are split as follows:

* **Engine**
  * Building and compiling games, and linking it with the engine
  * Providing a standard library for the user, namely data structures and wrappers around the rendering system
  * Providing a suite of tools to make development easier - handling unit tests, enabling intellisense (in VSCode), updating the engine, running raxel games, etc.
* **Rendering Systems**
  * Setting up graphics resources, making lower level calls to graphics libraries
  * Creating an abstraction above these resources, allowing the developer to focus on their application of these graphics tools
  * Defining voxels, voxel worlds, and handling the rendering of them

## Objectives

Building the engine was a semi-selfish effort, and it's design is to cater to my (Evan's) workflow. This workflow involves minimum usage of GUI's, a reliance on the CLI, and creating the minimum possible friction to get a project up and running.

The engine is designed to be a tool that I can use to quickly prototype ideas, and to build small games and technical demos. It is not designed to be a full-fledged game engine, nor is it designed to be a general purpose engine. It is designed to be a tool that I can use to quickly prototype ideas, and to build small games and technical demos. To make it an actually full-fledged engine, I would need to additional features -- right now, all of the features are for rendering. Building any game logic would require me to build it from scratch.

## Directory Structure

The engine is split into several directories, and the main focus of the engine thus far have been the `core` and `scripts` directories.

The `core` directory contains the core engine code, and is split into several subdirectories:
* `graphics` - Contains the code for the rendering systems, notably, the implementation of `raxel_surface_t`, `raxel_pipeline_t`, and other Vulkan wrappers. It also contains implementations of `raxel_pipeline_pass_t` which is the bulk, and most important part of the rendering system.
* `util` - Contains common utilities that are used throughout the engine, such as memory allocators, logging, and standard data structures, like lists, arrays, hashtables and strings.
* `input` - Contains the code for the input system, which is used to handle user input, such as keyboard and mouse events. This is a very thin wrapper around GLFW's input system.
* `voxel` - Contains the code for the voxel rendering system, which is used to render voxel data. This is a thin wrapper around the core rendering system, and is used to provide a higher level abstraction for rendering voxels. It also contains the implementation of things like the BVH acceleration structure, and the voxel data structure.

Within this directory, we have a suite of header files that include the headers contained within the subdirectories of `core`. These headers are used to provide a single include point for the user.

In practice, using the engine involves using include statements like this:

```c
#include <raxel/core/graphics.h>
#include <raxel/core/util.h>
```

The `scripts` directory contains the scripts that are used to build and run the engine. These scripts are used to build the engine, build games, run games, and update the engine. The scripts are written in Python, and are used to automate the process of building and running the engine.

Here, you'll find the `raxel.py` script, which is invoked whenever you use a `raxel` command in the terminal. This script simply scans within the `scripts/tools` directory for the subcommands, and runs the appropriate one. Within `scripts/tools`, you'll find the implementation of commands like `build`, `run`, `update`, etc. These are located in files that are named `raxel_<command>.py`. Any files that are not named in this format are not considered subcommands, and cannot be driectly run by the `raxel.py` script.

Before using any of the scripts, you can optionally run `scripts/raxel_install.sh`, which will place `raxel.py` in your path, and allow you to run `raxel` commands from anywhere in your terminal. This is rather important for the use of the engine, as per the [build system section](#build-system), the core raxel engine and the user's game code are seperated, and the user's game code is dynamically linked with the engine.

## Build System

The engine dynamically links with the user's game code. The build system for the engine is built in a manner such that it is not opinionated about the structure of the user's game code.

This choice was made in able to enforce a seperation of concerns between the engine and the user's game code. The engine is responsible for rendering, and the user's game code is responsible for game logic. The engine is not responsible for the user's game logic, and the user's game logic is not responsible for rendering. This seperation of concerns allows for the user to build their game in any way they see fit, and to use any libraries that they may need. 

By setting up this system early in the processs, it has pushed for a more modular design of the engine. In order to create a demo, the engine would have to provide a series of abstractions to build the demo on top of. This has manifested via the creation of thing like the `raxel_voxel_world_t`, and `raxel_input_manager_t` abstractions.

The only requirement of building a game with `raxel` is that the user must provide an entry point, via `main`, and provide a `raxel.cmake` file within the directory of the game. The `raxel.cmake` file is used to specify the name of the game, and any additional dependencies that the game may have. This system enables the user to build their game in any way they see fit, and to use any libraries that they may need.

Here is an example of such a `raxel.cmake` file:

```cmake
# This will be called from raxel's internal cmake
# The objective of this file is to add the sources and includes to the project
# Then, raxel will take care of linking the libraries and setting the flags

set(SOURCES
    ${RAXEL_PROJECT_ROOT_DIR}/main.c
)

set(INCLUDES
    ${RAXEL_PROJECT_ROOT_DIR}
)

# add the sources and includes to PROJECT executable
target_sources(${PROJECT} PRIVATE ${SOURCES})
target_include_directories(${PROJECT} PRIVATE ${INCLUDES})
```

This builds one of the simplest possible games, with a single source file, `main.c`. The `raxel.cmake` file specifies that the game should be built with this source file, and that the source file should be included in the project.

Internally, the raxel build system uses it's own `CMakeLists.txt` file to build the engine, the dependencies, and the user's game code. The abridged version of the build process is as follows:
```
1. Set up CMake, specifying the project name, minimum version, the languages used, and the project version
2. Find if the user has a `raxel.cmake` file in their project directory. If not, throw an error.
3. Compile the raxel engine as a static library, recursively searching for folders within the raxel/core directory
4. Build and find the dependencies of raxel. These dependencies are Vulkan, GLFW, and CGLM (which is the C version of GLM)
5. Compile the user's game code, in accordance with the `raxel.cmake` file.
6. Link the user's game code with the raxel engine, and the dependencies
```

Creating such a build system required leveraging a work-directory `.raxel`, which gets automatically created to the game's root directory.

For the most part, this folder is used to store the build artifacts of the engine, and the user's game code. This folder is also used to store the `CMakeCache.txt` file, which is used to store the configuration of the build system. This folder is also used to store the `CMakeFiles` folder, which is used to store the build files of the engine, and the user's game code.

A few book-keeping files are also added to this directory. The most useful for development has been the `checksum.txt` file, which is used to store the checksum of the user's game code, and the source files of the engine. This file is used to determine if the user's game code has changed, and if the engine needs to be rebuilt. Whenever the user attemps to run their game, the checksum of the user's game code is compared to the checksum stored in the `checksum.txt` file. If the checksums do not match, the user is alerted that either the engine/game code has changed, and that the engine needs to be rebuilt.

Furthermore, assets are stored in the `.raxel` directory, and are copied to the build directory whenever the user runs their game. This is done to ensure that the assets are always in the correct location, and that the user does not have to worry about copying the assets to the build directory.

## Raxel CLI Tools

There are a few commands that are available:

* `raxel build` - This command is used to build the engine, and the user's game code. This command is used to compile the engine, and the user's game code in the manner described in the [build system](#build-system) section. 
* `raxel run` - This command is used to run the user's game code. This command is used to run the user's game code. This simply runs the executable that is created within the `.raxel` directory.
* `raxel update` - Updates the user's version of raxel by pulling the latest version of the core engine from the repository. Because the user's game code is seperate from the engine, this is one of the affordances that the engine provides. The user can update the engine without having to worry about their game code being affected. In development, this has not been used much.
* `raxel index` - Creates necessary JSON files (`c_cpp_properties.json`) for intellisense in VSCode. Because the game code is seperate from the engine, intellisense doesn't work outside the box, one of the drawbacks from seperating engine and game code so strictly.
* `raxel test` - Runs the core raxel unit tests. The unit testing system is a raxel "game" located next to the engine code. This is an end-to-end test of the engine, as running the testing suite involves actually building a "game," which in reality, is just a series of unit tests.
* `raxel sc` - Compiles the shaders in the user's and raxel's code, and places them in the `.raxel` directory. The shaders are compiled into `SPIR-V` format, which is the format that Vulkan uses. Shaders that are part of the engine core are placed in a seperate directory within `.raxel`, `shaders/internal`, and the user's shaders are placed in `shaders/` directly.
* `raxel clean` - Cleans the build directory, and removes the `.raxel` directory. This is useful for cleaning up the build artifacts, and for starting fresh.
* `raxel br` - Compiles the engine, user's game code, and shaders, and runs the game. This is a convenience command that is used to combine `raxel build`, `raxel run`, and `raxel sc` into a single command.

Each of these commands have different subcommands, and can be run with the `--help` flag to see the available subcommands. For example, running `raxel build --help` will show the available subcommands for the `raxel build` command.


# Rendering Systems

The most developed part of `raxel` are the rendering systems. The rendering systems are split into two parts, the core renderer, and the voxel renderer. The core renderer is the wrapper around the Vulkan API, and is used to set up the graphics resources, and make lower level calls to the graphics libraries. The voxel renderer is the wrapper around the core renderer, and is used to provide a higher level abstraction for rendering voxels.

Because the engine is a voxel engine, the voxel renderer has been developed such that the interface is as simple as possible, with minimal knowledged of graphics programming required. The core renderer is more complex, but it is significantly easier than programming directly with Vulkan.

The core renderer achieves:
* Initialization of Vulkan, and all of the boilerplate code that is required to set up the graphics resources
* Creating graphics pipelines and resources
* Taking care of the synchronization of the graphics resources
* Abstracting multi-pass rendering into an easy, declarative interface
* Abstracting over compute shaders
* Passing data between the CPU and GPU

Whereas the voxel renderer achieves:
* Storing voxel data in memory
* Rendering voxels in a compute shader
* Creating acceleration structures for the voxels
* Handling the memory management of the voxel data


## Core Renderer

Below is an abridged version of the demonstration code that will be discussedin the [demonstration](#demonstration) section. This code demonstrates the core renderer and voxel renderer, and how it is used to create a simple raymarched voxel scene.

```cpp
// Called when the surface is destroyed.
static void on_destroy(raxel_surface_t *surface) {
    RAXEL_CORE_LOG("Destroying surface...\n");
    raxel_pipeline_t *pipeline = (raxel_pipeline_t *)surface->user_data;
    raxel_pipeline_cleanup(pipeline);
    raxel_pipeline_destroy(pipeline);
}

int main(void) {
    // Create a default allocator.
    raxel_allocator_t allocator = raxel_default_allocator();

    // Create a window and Vulkan surface.
    raxel_surface_t *surface = raxel_surface_create(&allocator, "Voxel Raymarch", WIDTH, HEIGHT);
    surface->callbacks.on_destroy = on_destroy;

    // Setup input.
    raxel_input_manager_t *input_manager = raxel_input_manager_create(&allocator, surface);

    // Create the pipeline.
    raxel_pipeline_t *pipeline = raxel_pipeline_create(&allocator, surface);
    surface->user_data = pipeline;

    // Initialize the pipeline (creates instance, device, swapchain, etc.).
    raxel_pipeline_initialize(pipeline);

    // Set the debug target to the internal color target.
    raxel_pipeline_set_debug_target(pipeline, RAXEL_PIPELINE_TARGET_COLOR);

    // Create a clear pass to clear the internal color target.
    raxel_pipeline_pass_t clear_pass = clear_color_pass_create((vec4){0.0f, 0.3f, 0.8f, 1.0f});
    raxel_pipeline_add_pass(pipeline, clear_pass);

    // Create the compute shader and pass.
    raxel_pc_buffer_desc_t pc_desc = RAXEL_PC_DESC(
        (raxel_pc_entry_t){.name = "view", .offset = 0, .size = 16 * sizeof(float)},
        (raxel_pc_entry_t){.name = "fov", .offset = 16 * sizeof(float), .size = sizeof(float)},
        (raxel_pc_entry_t){.name = "rays_per_pixel", .offset = 16 * sizeof(float) + sizeof(float), .size = sizeof(int)},
        (raxel_pc_entry_t){.name = "debug_mode", .offset = 16 * sizeof(float) + sizeof(float) + sizeof(int), .size = sizeof(int)}, );
    raxel_compute_shader_t *compute_shader = raxel_compute_shader_create(pipeline, "internal/shaders/voxel.comp.spv", &pc_desc);

    // Create a compute pass context, which is just a struct that holds information about how to dispatch the compute shader.
    raxel_compute_pass_context_t *compute_ctx = raxel_malloc(&allocator, sizeof(raxel_compute_pass_context_t));
    compute_ctx->compute_shader = compute_shader;
    // Set dispatch dimensions based on our window size and workgroup size.
    compute_ctx->dispatch_x = (WIDTH + 15) / 16;
    compute_ctx->dispatch_y = (HEIGHT + 15) / 16;
    compute_ctx->dispatch_z = 1;
    // What resources are we writing to?
    compute_ctx->targets[0] = RAXEL_PIPELINE_TARGET_COLOR;
    compute_ctx->targets[1] = -1;  // Sentinel.
    // You can set a callback to be called when the dispatch is finished.
    compute_ctx->on_dispatch_finished = NULL;

    // Create the compute pass and add it to the pipeline.
    raxel_pipeline_pass_t compute_pass = raxel_compute_pass_create(compute_ctx);
    raxel_pipeline_add_pass(pipeline, compute_pass);

    // Create and populate a voxel world
    raxel_voxel_world_t *world = raxel_voxel_world_create(&allocator);

    // Code to populate the voxel world...

    vec3 camera_position = {0.0f, 0.0f, -50.0f};
    float camera_rotation = 0.0f;

    // Code to initially load/unload chunks based on camera position, and pass the voxel world to the compute shader...

    raxel_pipeline_start(pipeline);

    // Main loop
    // For demonstration, we update the push constants per frame.
    double time = 0.0;
    double delta_time = 0.01;

    while (!raxel_pipeline_should_close(pipeline)) {
        // Poor man's delta time.
        time += delta_time;

        // Simple WASD and QE controls to move the camera.
        if (raxel_input_manager_is_key_down(input_manager, RAXEL_KEY_W)) {
            camera_position[2] += 0.1f;
        }
        // Code to handle other key presses...

        // Update the view matrix.
        mat4 view;
        glm_mat4_identity(view);
        // Rotate the view matrix by the camera rotation.
        glm_rotate(view, camera_rotation, (vec3){0.0f, 1.0f, 0.0f});
        // Translate the view matrix by the negative camera position.
        glm_translate(view, (vec3){camera_position[0], camera_position[1], camera_position[2]});

        // Update the push constants
        raxel_pc_buffer_set(compute_shader->pc_buffer, "view", view);
        // Update fov (e.g., 60 degrees converted to radians).
        float fov = glm_rad(60.0f);
        raxel_pc_buffer_set(compute_shader->pc_buffer, "fov", &fov);
        // Update rays per pixel (e.g., 4 rays per pixel).
        int rpp = 1;
        raxel_pc_buffer_set(compute_shader->pc_buffer, "rays_per_pixel", &rpp);

        // Code to load/unload chunks based on camera position, and pass the voxel world to the compute shader...

        raxel_pipeline_update(pipeline);
    }

    return 0;
}
```

This code is only slightly abridged, only really removing the code for populating the voxel world, and loading/unloading chunks based on camera position. The code demonstrates how the core renderer and voxel renderer are used to create a simple raymarched voxel scene. 

In plain English, the code does the following:
1. Creates a window and a Vulkan surface
1. Sets up input handling
1. Creates a pipeline, and initializes it
1. Creates a clear pass to clear the internal color target, making the screen blue
1. Creates a compute shader and pass, which is used to render the voxel scene
1. Creates a voxel world, and populates it with voxel data
1. In the main loop, updates the camera position based on user input
1. Updates the view matrix based on the camera position and rotation
1. Updates the push constants for the compute shader, which includes the view matrix, field of view, and rays per pixel
1. Updates the voxel world based on the camera position
1. Updates the pipeline, which is rendering and presenting the scene
1. Repeats the main loop until the window is closed

The implementation of the core renderer (and thus, the example) will be discussed in the following sections. Full details of the individual Vulkan resources are not covered, but the most important parts, namely the handling of the swapchain, compute shaders, and the pipeline, are covered.

### Compute Shader-Driven Graphics Pipeline

`raxel` runs purely on compute shaders. There are no vertex or fragment shaders, thus how to fundamentally think about rendering in `raxel` is different than in traditional graphics programming.

Instead of worrying about fragments, vertices, and the like, users only need worry about a few things:
* The compute shader
* The data being passed to the compute shader
* The resources that are being written to
* The order in which the compute passes are executed

The pipeline will keep large resources, specifically the "targets," which are textures that are read and rendered to by compute shaders on the GPU. The effect of this is that the effects of a single pass can be seen in the next pass, thus the order of the passes is important. Because of this, however, this makes multi-pass rendering very easy to implement.

![Compute Shader Pipeline](/images/compute_shader_pipeline.png)

Upon the "presentation" step, which is part of the `raxel_pipeline_update` function, the pipeline will grab one of the targets as specified via the `raxel_pipeline_set_debug_target` function, and present it to the window. This is how the user sees the final result of the compute passes. 

Currently, there are two types of targets, although this can be expanded in the future:
* `RAXEL_PIPELINE_TARGET_COLOR` - This is the internal color target, which is the target that is presented to the window. This is the target that the user sees.
* `RAXEL_PIPELINE_TARGET_DEPTH` - This is the internal depth target, which is used for depth testing. This is not presented to the window, but is used for depth testing in the compute passes.

Some targets to consider in the future are:
* `RAXEL_PIPELINE_TARGET_NORMAL` - This is the internal normal target, which is used for normal mapping. This is not presented to the window, but is used for normal mapping in the compute passes.
* `RAXEL_PIPELINE_TARGET_WORK_BUFFER_X` - This would be a series of work buffers, which are used for intermediate computations. These are not presented to the window, but are used for intermediate computations in the compute passes.

Adding such targets would be simple, and would allow for different types of rendering effects to be achieved, like deferred rendering, normal mapping, and more.

At the Vulkan level, this is achieved by using the swapchain. All of these targets are created as part of the swap chain, and exist in the GPU's memory, persisting between frames and compute passes.

The full initialization of the pipeline looks like this:
```c
int raxel_pipeline_initialize(raxel_pipeline_t *pipeline) {
    __create_instance(&pipeline->resources);
    __pick_physical_device(&pipeline->resources);
    __create_logical_device(&pipeline->resources);
    __create_command_pools(&pipeline->resources);
    __create_sync_objects(&pipeline->resources);
    // Create swapchain using surface dimensions.
    raxel_surface_initialize(pipeline->resources.surface, pipeline->resources.instance);
    __create_swapchain(&pipeline->resources, pipeline->resources.surface->width, pipeline->resources.surface->height, &pipeline->resources.swapchain);
    __create_targets(&pipeline->resources, &pipeline->resources.targets, pipeline->resources.surface->width, pipeline->resources.surface->height);
    __create_descriptor_pool(pipeline);
    return 0;
}
```

### Abstractions of the Core Renderer

...

## Voxel Renderer

...

### Objectives of the Voxel Renderer

...

### Voxels in Memory

...

### Memory Management

...

### Acceleration Structures (BVH)

...

### Rendering Voxels in Compute Shader

...

# Demonstration

...


## Building and Running the Demo

...

# Reflections & Further Work

...

## Workflow of using my own Engine

...

## Plans for the Spring

...


