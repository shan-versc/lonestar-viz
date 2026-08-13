# Architecture

## Thread Model

```
┌──────────────────────────────┐     SpscQueue<4096>     ┌──────────────────────────────────┐
│       Ingest Thread          │ ─────────────────────►  │          Main Thread              │
│  IngestSystem::ingest_loop() │     NodeDelta events     │  drain → physics → octree → render│
│  ~60 Hz (16 ms tick)         │                          │  ~60 fps                          │
│  Owns: values[], rng         │                          │  Owns: ECS, GPU state, ImGui      │
└──────────────────────────────┘                          └──────────────────────────────────┘
```

No mutexes. The SPSC queue is the only shared state. Lock-free acquire/release ordering.

## ECS Layout (EnTT)

| Component         | Purpose                                              |
|-------------------|------------------------------------------------------|
| `NodeComponent`   | Position, velocity, force, value, name, active flag  |
| `EdgeComponent`   | src entity, dst entity, spring strength              |
| `HistoryComponent`| Rolling deque of 128 past values                     |
| `ClusterTag`      | Marks collapsed cluster representatives              |
| `SelectedTag`     | Set by Raycaster on left-click                       |

Entities are never destroyed at runtime — inactive nodes keep their ECS slot and stop receiving forces when `active == false`.

## Physics Pipeline (per frame)

1. **Repulsion** — O(n²) naive (base) or O(n log n) octree-culled (feature branch):
   Coulomb: `F = k_r / r²`, clamped to `max_force`, skipped beyond 60 units.
2. **Springs** — Hooke: `F = k_s * (len - rest_len * strength)` per edge.
3. **Value impulses** — when `|value - prev_value| > 0.05`, apply a deterministic
   pseudo-random impulse proportional to the delta magnitude.
4. **Integration** — `world.tick(dt)`: `pos += vel*dt`, `vel *= damping`, `force = 0`.

## Renderer Pipeline (per frame)

1. Clear depth + colour (dark background).
2. Draw edges as `GL_LINES` (batched into one VBO upload, one draw call).
3. Build `BillboardInstance` array from all `NodeComponent` entities.
4. Upload to instance VBO via `glBufferSubData` (streaming).
5. `glDrawArraysInstanced` — 6 verts × N instances, one draw call.
6. ImGui: StatsPanel, PhysicsPanel, NodeInspector (if node selected), HistoryRibbon.

## Octree

The `Octree` is a simple recursive `OctreeNode` tree. Subdivides when a leaf
exceeds `kMaxItems = 16` entities, down to `kMaxDepth = 6`. Rebuilt from scratch
every 5–10 frames (cheaper than incremental update at these node counts).

Used by:
- `OctreePhysicsSystem` — spatial repulsion culling
- `LodCuller` — distance-based full-detail vs. cluster classification

## SPSC Queue

`SpscQueue<N>` is a power-of-two ring buffer with separate `head_` (producer)
and `tail_` (consumer) atomics, each on their own cache line (`alignas(64)`).
No CAS, no mutex. `push()` / `try_pop()` are wait-free from each side.

`IngestConfig` controls tick rate, walk sigma, burst parameters, and back-pressure
retry count. `QueueMetrics` tracks push/pop/drop/retry counts atomically.
