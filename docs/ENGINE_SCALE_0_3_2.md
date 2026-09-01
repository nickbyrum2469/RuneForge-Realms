# RuneForge Realms 0.3.2 — Async Chunk Rendering

0.3.2 completes the second half of the engine-scale foundation: streamed terrain generation leaves the game thread, and the Vulkan renderer stops treating the visible world as one giant mesh.

## Background chunk generation

`world/streaming/ChunkStreamer` owns the asynchronous generation queue and uses the generic `core/jobs/JobSystem` introduced in 0.3.1. Worker threads receive only immutable seed + chunk-coordinate inputs and produce standalone `VoxelChunk` results.

Completed chunks are committed to `FrontierWorld` on the owning game thread. Stored player edits are reapplied during that commit, so worker threads never mutate gameplay state or the persistent edit map.

The first spawn region remains synchronously generated so a new game has solid terrain immediately. Chunks requested while traveling are generated in the background.

## Per-chunk Vulkan meshes

The renderer now owns one `GpuChunkMesh` per loaded world chunk instead of concatenating the entire streamed region into a single vertex/index allocation.

Consequences:

- editing one block only dirties and reuploads its owning chunk;
- newly completed chunks upload independently;
- evicted chunks release only their own GPU buffers;
- the whole Vulkan device no longer waits idle for every terrain edit;
- the architecture is ready for a dedicated batched upload queue without redesigning world ownership.

## Chunk culling

`render/culling/ChunkVisibility` provides a renderer-independent distance/FOV visibility test over chunk bounds. The Vulkan command recorder skips chunks outside the current view and records only visible chunk draws.

The window diagnostics expose visible / loaded / pending chunk counts so runtime streaming behavior can be inspected during testing.

## Continue-save correctness

A continued save now synchronously boots its initial chunk region around the saved player position rather than origin. This prevents a far-traveled player from briefly loading into empty space while the background streamer catches up.

## Release automation cleanup

The Windows Release workflow no longer describes every future build as the original Vulkan voxel-scene proof. Release notes now describe the native package/update behavior generically and confirm that the published package passed the merged-main Windows build/test gate.

## Still deliberately deferred

- batch Vulkan staging/upload queue instead of per-buffer one-shot copies;
- asynchronous greedy meshing after chunk generation or edits;
- true camera frustum planes and occlusion culling;
- region-file save storage/journaling;
- texture arrays and PBR material resources;
- runtime render-distance setting.

Those are the next engine-scale pieces before the full 0.4 survival slice expands gameplay systems.
