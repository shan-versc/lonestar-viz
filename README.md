# Lonestar — 3D Causal Cartographer

A real-time 3D force-directed visualizer for dynamic simulation state.
Nodes represent simulation registers; edges encode causal dependencies.
The layout engine is a Verlet-integrated force-directed graph with Coulomb
repulsion and Hooke spring attraction. A lock-free SPSC queue decouples the
ingest thread from the render thread.

## Tech stack

| Layer       | Library                |
|-------------|------------------------|
| Language    | C++20                  |
| ECS         | EnTT v3.13             |
| Windowing   | GLFW 3.4               |
| OpenGL      | glad (GL 4.1 core)     |
| Math        | GLM 1.0.1              |
| GUI         | Dear ImGui v1.90.8     |
| JSON        | nlohmann/json v3.11.3  |
| Build       | CMake ≥ 3.24           |

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/aarf_cartographer
```

Requires: OpenGL 4.1 driver, X11/Wayland (Linux), or macOS 10.15+.

## Controls

| Input              | Action                         |
|--------------------|--------------------------------|
| LMB drag           | Arcball orbit                  |
| MMB drag           | Pan                            |
| Scroll             | Dolly zoom                     |
| `R`                | Reset layout & camera          |
| `SPACE`            | Pause / resume physics         |
| `ESC`              | Quit                           |

## Architecture

```
┌──────────────────┐    SPSC Queue    ┌───────────────────┐
│  Ingest Thread   │ ───────────────► │   Main Thread      │
│  (synthetic sim) │                  │  drain → physics   │
│  ~60 Hz          │                  │  → octree → render │
└──────────────────┘                  └───────────────────┘
```

- **Octree** partitions world-space for LOD culling (rebuilt every 10 frames).
- **PhysicsSystem** applies repulsion + spring forces, Verlet-integrates positions.
- **Renderer** uses instanced billboard quads; one draw call for all nodes.
- **IngestSystem** produces synthetic random-walk + cascade-burst data.

## Branches

- `main` — stable trunk
- `feature/octree-physics` — octree-accelerated O(n log n) physics
- `feature/renderer-imgui` — extended ImGui panels + history ribbon
- `feature/ingest-spsc` — ingest thread and SPSC queue (merged to main)
