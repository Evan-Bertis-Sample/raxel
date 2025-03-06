# Report Outline (Winter 2025)

```
Engine
├── Objectives of the Engine
│   └── Motivations, outside of it being a cool project
├── High Level Description of the Engine
│   └── (Diagram)
└── Notable Design Choices
    └── Python Tooling System
Rendering Systems
├── Core Renderer (Base Layer over Vulkan)
│   ├── Compute Shader-Driven Graphics Pipeline
│   │   ├── (Diagram)
│   │   ├── Why?
│   │   └── How does it work?
│   └── Abstractions of the Core Renderer
│       ├── Raxel Pipeline
│       ├── Raxel Render Passes
│       ├── Raxel Render Targets
│       ├── Raxel Push Constants
│       └── What the Abstractions Enable
└── Voxel Renderer
    ├── Objectives of the Voxel Renderer
    ├── Voxels in Memory
    │   ├── Individual Voxels
    │   └── Chunks of Voxels
    ├── Memory Mangagment
    ├── Accleration Structures (BVH)
    └── Rendering Voxels in Compute Shader
        ├── Explaining the Algorithms
        └── How the Accleration structures make it faster
Demonstration
├── Picture of the Demonstration
├── (Code for Demonstration, in Appendix, to show how engine is used)
└── Instructions Building and Running the Demo
    └── May not work on all computers -- but shows how the tooling works
Reflections & Further Work
├── Workflow of using my own Engine
└── Plans for the Spring
```

All of this is subject to change as I write the report, but this is the general outline I am going to follow. I'm imagigning that the report will be around ~8-10 pages long, but mainly photos.

I don't plan onto going into great detail on the `Engine` section, and mainly am going to focus on the `Rendering Systems` part. Seeing how a lot of my work has been continously building and improving tooling for myself as I work on the engine, it would feel wrong to completely dismiss that part of the project in the report. I also think that the `Voxel Renderer` is the most interesting part of the project, so I want to make sure that I have enough space to explain it in detail.
