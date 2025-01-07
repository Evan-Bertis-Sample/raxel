# Timeline

## Winter 2025

The main objective of the winter quarter will be to set up the rendering engine, and get a basic scene rendering. I hope the final scene for this quarter will be a simple terrain (likely procedurally generated using Perlin noise) with a skybox, a few point lights, a large directional light, and a few material. Imagine the minecraft overworld, but with (hopefully) significantly smaller voxels and better lighting.

For the purpose of scheduling, I consider weeks to start on Monday and end on Sunday.

### Week 1 : Planning and Project Setup

- Create a project plan
- Set up the project repository
- Set up any administrative tasks, like figuring out meeting times/enrolling in the class
- A simple CMake-based build system, and project structure
- Getting all the core libraries set up and running (Vulkan, GLFW, GLM, etc.)
- Beginning research on existing raymarched voxel renders, and how to implement them in Vulkan

### Week 2 : Basic Rendering

- Learn about the basics of Vulkan
- Set up a basic Vulkan renderer
- Get a simple mesh rendering, with point lights
- High-level design of the voxel renderer, specifically the structure of voxels in memory, chunking, and how rendering will work conceptually
- Begin implementing the voxel renderer

### Week 3 : Basic Voxel Rendering (part 1)

- Creating a base layer for the project, including very important data structures like lists, buffers, and memory management, etc. Basically anything that will be used throughout the project
- Implement a baseline voxel renderer, without any lighting or more complicated optimization techniques. However, it should be using raymarching to render the voxels.

### Week 4 : Basic Voxel Rendering (part 2)

- Continue implementing the voxel renderer from last week, adding in a few basic types of lights (no bouncing our complex global illumination -- just Phong or something), and important optimizations like chunking
- Begin implementing a basic material system that allows for different types of materials (the same shader, but different parameters) to be used on different voxels
- Create a simple terrain generator using Perlin noise to showcase multiple materials

### Week 5 : Midterm Buffer Week

- Likely midterms week, so use this week as a buffer
- Plan out the material system for the next week, specifically how to handle different shaders, and what types of shaders will be used

### Week 6 : Material System

- Flesh out the material system to allow for different shaders to be used on different voxels
- Make some basic extensions to the shader language to allow for some-level of meta programming (mainly #include)
- Research how to implement Global Illumination in a raymarched voxel renderer, and begin implementing a system that can handle it

### Week 7 : Global Illumination

- Begin implementing a global illumination system that can handle complex lighting scenarios, like multiple bounces, and complex materials

### Week 8 : Global Illumination (part 2)

- Buffer week for global illumination, will likely take longer than a week

### Week 9 : More Materials & Demo!

- Add support for emissive materials, and create a suite of materials that can be used in the demo scene
- Begin creating a demo scene that shows off the capabilities of the engine

### Week 10 : Demo & Final Report

- Finish up any loose ends from the previous weeks
- Final Presentation and documentation of the project

---

## Spring 2025

Likely to be more focused on the game engine side of things, and less on the rendering engine. Things like the entity-component system, editor, and fun stuff like post-processing effects and particles.

