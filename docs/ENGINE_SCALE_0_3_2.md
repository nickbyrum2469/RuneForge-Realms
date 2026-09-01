# RuneForge Realms 0.3.2 — Runtime Scale Pass

0.3.2 turns the ownership boundaries introduced in 0.3.1 into active runtime systems. The objective is to preserve the playable Frontier loop while making world growth, remeshing, GPU ownership and persistence scale independently.

## World streaming

`world/chunks/ChunkManager` keeps a synchronous resident safety ring around the player and asynchronously prefetches farther chunks through `core/jobs/JobSystem`. The prefetch backlog is bounded, deterministic chunk generation is pure, and old worker jobs are drained when a world session is reset.

Player edits live separately from chunk residency. Unloaded chunks can be regenerated from seed and have their stored edits reapplied when they return.

## Neighbor-aware chunk meshes

`GreedyMesher` now accepts immutable `ChunkMeshingSnapshot` objects containing a center chunk and its four horizontal neighbors. Hidden faces between adjacent chunks are removed instead of both chunks rendering their shared wall. Loading, unloading and boundary edits invalidate adjacent chunk revisions so edge geometry stays coherent.

Snapshots own their data so worker threads never read live mutable world storage.

## Asynchronous meshing

Dirty chunks are scheduled through a dedicated mesh worker pool. Scheduling and GPU uploads both have explicit per-frame/backlog budgets. A chunk revision is captured with every mesh job; results are discarded if the world changed before the worker finished.

Only the nearby 3x3 neighborhood is meshed/uploaded synchronously during startup. Farther loaded chunks enter the normal asynchronous path so startup does not synchronously upload the entire streamed world.

## Vulkan chunk ownership

Every rendered chunk owns independent device-local vertex and index buffers. A block edit can replace one chunk mesh instead of rebuilding one world-sized GPU allocation. Unloaded chunks retire their own resources after the frame fence is safe.

`render/scene/ChunkCulling` applies distance and conservative rear-facing rejection before draw submission. The window diagnostics expose loaded, pending and visible chunk counts for field testing.

## Persistence

Frontier metadata is schema 2. World edits are partitioned into 32x32-chunk `.rfr` region files under the save directory. Individual region replacements are atomic, stale region files are removed during full saves, and schema-1 saves from 0.3.x remain readable and migrate on their next save.

A future persistence pass should add a journal/manifest generation for full multi-region crash recovery; 0.3.2 establishes the region ownership boundary first.

## Release-gate tests

Tests are separated by responsibility instead of accumulating in one file:

- `TestRegistries.cpp` — versions, hub model, block/material registries;
- `TestJobs.cpp` — queued and typed worker jobs;
- `TestWorld.cpp` — deterministic generation, collision, streaming, edit residency, neighbor-aware meshing;
- `TestPersistence.cpp` — region partitioning, stale cleanup and schema migration;
- `TestCulling.cpp` — conservative chunk visibility decisions.

## Explicit next engine targets

- non-blocking Vulkan transfer ring / upload staging pool;
- configurable render distance and streaming budgets;
- save journal + region manifest integrity;
- instrumentation for frame, generation, meshing and upload timings;
- then the 0.4 survival systems: items, drops, hotbar, inventory, mining, tools and crafting.
