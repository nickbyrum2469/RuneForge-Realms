# RuneForge Realms 0.3.1 — Engine Scale Foundation

This pass changes the native Frontier runtime from a fixed demo region into a streamed-world architecture while preserving the 0.3.0 gameplay contract.

## Responsibility boundaries

### `core/jobs/`
Owns generic worker threads and job scheduling. It knows nothing about voxels, rendering or saves.

### `world/chunks/`
Owns chunk coordinates, residency, prefetch, unload policy, revisions and dirty-mesh state. Chunk generation is delegated rather than embedded here.

### `world/generation/`
Owns deterministic generation functions. Generators accept seed + coordinates and return data without touching the renderer, player or platform layer. This makes them safe to run on worker threads.

### `world/blocks/`
Owns stable gameplay metadata for block IDs. Hardness, stack limits and material assignments live here rather than spreading switch statements through mining, inventory and rendering code.

### `render/materials/`
Owns render-material metadata. This is deliberately separate from BlockRegistry because multiple blocks may share physical render materials and one block may eventually use several surfaces/overlays.

### `render/scene/`
Owns scene-level visibility policy such as chunk culling. Vulkan resource management remains in `render/vulkan/`.

### `save/world/`
Owns scalable world persistence. `world.rfsv` is metadata; region files partition world edits spatially so one autosave does not require one ever-growing monolithic edit list on disk.

## Runtime streaming policy

- resident radius: 2 chunks (5x5 guaranteed synchronously around the player)
- prefetch radius: 5 chunks (generated on background worker threads)
- unload radius: 7 chunks
- prefetch budget: 10 new jobs per streaming update

These are engineering defaults, not final graphics settings. They will become configurable after runtime profiling.

## Mesh lifecycle

1. Chunk loads or changes.
2. Chunk revision increments and mesh state becomes dirty.
3. Renderer copies the 4096-cell chunk snapshot.
4. Greedy meshing runs on a mesh worker thread.
5. Completed result is accepted only if its revision still matches the live chunk.
6. Vertex/index data is uploaded to a device-local Vulkan buffer pair for that chunk.
7. Only that chunk's previous GPU allocation is replaced.
8. Unloaded chunks retire their own GPU resources without rebuilding the rest of the world.

This intentionally removes the 0.3.0 behavior where any block edit rebuilt one combined world-sized GPU mesh.

## Persistence migration

Schema 1 (0.3.0): metadata + every edit in `world.rfsv`.

Schema 2 (0.3.1): metadata in `world.rfsv`, edits in `regions/r.X.Z.rfr`.

Schema-1 files remain readable. The next successful save writes schema 2. Region replacement uses temporary files and stale region files are deleted only after the new complete edit set is written.

## Next engine work after this pass

The immediate follow-on is not more world ownership refactoring. It is the content/runtime layer that benefits from this foundation:

- proper texture arrays and GPU material resources;
- neighbor-aware chunk meshing to remove internal boundary faces;
- non-blocking transfer/upload ring instead of queue-wait uploads;
- configurable render distance and stronger frustum/occlusion culling;
- inventory/items/mining/crafting survival systems;
- biome/climate/cave/ore generators as independent generation stages.

The file-size rules in `ARCHITECTURE.md` remain mandatory: features are split by responsibility instead of allowing `FrontierWorld`, `VulkanRenderer`, or `NativeWindow` to become god objects.
