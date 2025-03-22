rcdaxel
======

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

Building the engine was a selfish effort, and it's design is to cater to my (Evan's) workflow. This workflow involves minimum usage of GUI's, a reliance on the CLI, and creating the minim possible 

## High Level Description

...

## Notable Design Choices

...

# Rendering Systems

...

## Core Renderer

...

### Compute Shader-Driven Graphics Pipeline

...

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


