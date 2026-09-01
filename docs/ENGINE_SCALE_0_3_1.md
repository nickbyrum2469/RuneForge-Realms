# RuneForge Realms 0.3.1 — Engine Scale Foundation

This pass expands the 0.3.0 playable Frontier prototype into a world architecture that can grow beyond a fixed test region without coupling generation, streaming, rendering, saves and gameplay into one class.

## What changes

### Chunk lifecycle

`world/chunks/ChunkManager` owns loaded chunk records and their lifecycle. Chunks are requested nearest-first around a streaming center, retained with a hysteresis ring, and evicted after they move outside the retain radius. `FrontierWorld` remains the gameplay-facing facade rather than exposing storage details to the player controller or renderer.

The initial world still boots with a 7x7 area for fast startup. Once the player moves, the streaming window grows to a 9x9 load radius with an 11x11 retention envelope. The world can therefore continue generating beyond the original 0.3.0 boundaries instead of ending at a fixed wall.

### Deterministic per-chunk generation

`world/generation/TerrainGenerator` owns terrain and first-pass tree generation. A chunk can be regenerated independently from only the world seed and chunk coordinate. Tree generation samples a small halo so canopies crossing chunk boundaries remain deterministic regardless of chunk load order.

Player edits remain stored independently from loaded chunks. When a chunk unloads and is later regenerated, stored edits are reapplied before it becomes visible. This is a required property for future region files and multiplayer chunk ownership.

### Dirty chunk state

`ChunkManager` tracks Ready/Dirty state. Block edits dirty only their owning chunk. The current Vulkan path still combines loaded chunk meshes into one GPU allocation as a compatibility bridge, but the world now exposes per-chunk mesh and dirty-chunk APIs so the next renderer pass can move to independent GPU chunk meshes without redesigning world storage again.

### Worker foundation

`core/jobs/JobSystem` is a small native C++ worker pool with bounded worker count, queued jobs and an idle barrier. It is intentionally not allowed to mutate `FrontierWorld` directly in this pass. The next pass will use immutable chunk-generation/mesh jobs and apply completed results on the owning game/render thread.

### Registries

`world/blocks/BlockRegistry` gives blocks stable gameplay metadata such as hardness, preferred tool, stack size, transparency and material assignments.

`render/materials/MaterialRegistry` separates visual material identity from block identity. The first definitions already name the detail profiles required by the visual plan: dense grass tufts, root soil, fractured rock, bark chunks and leaf clusters. Actual PBR texture resources arrive in the visual pass; this registry prevents those properties from being hard-coded in shaders.

## Explicitly deferred to the next engine pass

- background chunk generation through `JobSystem`;
- asynchronous greedy meshing;
- render-thread mesh upload queue;
- one Vulkan vertex/index allocation per visible chunk;
- frustum/distance chunk culling;
- region-file persistence and save journal;
- configurable render distance UI.

Those systems now have clean ownership boundaries to build against instead of being bolted into `VulkanRenderer` or `FrontierWorld`.
