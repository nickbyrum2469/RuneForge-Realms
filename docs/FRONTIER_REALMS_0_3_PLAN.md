# Frontier Realms 0.3.0 — playable survival foundation

## Product decision

RuneForge Realms is one flagship game first. Frontier Realms is the consumer-facing survival experience. The other hub cards are not separate finished games and should be removed from the primary launch flow until the core game is strong. Engine Lab remains developer/QA-only.

## Goal

Replace the current `ENTER VOXEL LAB` proof-of-renderer path with the first real Frontier Realms gameplay loop.

The 0.3.0 player should be able to:

1. Launch RuneForge through the existing bootstrapper/update channel.
2. Choose Frontier Realms / New Game from a simplified main menu.
3. Spawn into a deterministic procedural voxel world.
4. Move with WASD, look with the mouse, jump, sprint and collide with terrain.
5. See multiple streamed terrain chunks instead of the fixed voxel diorama.
6. Aim at blocks with a center-screen target ray.
7. Break a block and see the chunk remesh.
8. Place a block on an exposed face and collide with it.
9. Use a minimal numbered hotbar/material selection.
10. Press Escape to open a pause overlay and return to the main menu without closing the application.

## 0.3 implementation order

### A. Product/menu cleanup

- Retire the fake multi-mode marketplace presentation from the default launch flow.
- Make Frontier Realms the primary game identity.
- Replace `ENTER VOXEL LAB` with `NEW GAME` / `CONTINUE` foundations.
- Keep Engine Lab accessible only through a development/diagnostics path.
- Preserve build/update metadata unobtrusively.

### B. Game-state boundary

Add explicit application states rather than letting the renderer itself act as the game:

- MainMenu
- LoadingWorld
- Playing
- Paused
- EngineLab (developer only)

Platform/window code owns transitions; gameplay does not own Win32/Vulkan UI details.

### C. World core

Introduce dedicated modules under `src/world/`:

- `BlockId` / block registry with stable namespaced IDs
- chunk coordinates and chunk storage
- deterministic seeded terrain generator
- chunk neighborhood access
- dirty flags for mesh/collision changes

Initial benchmark: 16x16 horizontal chunks with a practical vertical range, subject to profiling.

Initial materials:

- air
- grass
- dirt
- stone
- wood
- leaves

### D. Meshing + renderer bridge

Replace the hard-coded voxel diorama vertex data with chunk-generated mesh data.

- exposed-face meshing first
- greedy meshing immediately after correctness
- GPU vertex/index buffers per visible chunk/section
- dirty-chunk remesh after edits
- render-distance-limited visible chunk set

The renderer receives meshes; it does not own procedural world rules.

### E. First-person controller

Add gameplay-owned player state:

- position / velocity
- yaw / pitch
- grounded state
- walk / sprint speed
- jump impulse
- gravity
- AABB/capsule-style voxel collision
- step/slope behavior foundation
- mouse capture/release

### F. Block interaction

- center raycast
- highlight current block target
- left-click break
- right-click place
- placement collision safety
- selected hotbar material
- chunk dirty/remesh propagation to neighbors when editing borders

### G. Minimal gameplay HUD

Only what is needed for the first playable build:

- crosshair
- selected block/material
- compact hotbar
- FPS/frame diagnostic toggle in development builds
- pause overlay

Do not build the final inventory/crafting UI yet.

## Explicitly not in 0.3.0

- final PBR grass/stone/water quality
- full inventory/crafting
- enemies/combat
- hunger/temperature
- settlements
- multiplayer
- other consumer game modes
- full save format
- final worldgen biome suite

Those come after the core movement/edit loop is stable.

## Acceptance test

A successful 0.3.0 build is not judged by screenshots alone.

From a clean launch, the tester must be able to enter Frontier Realms, walk at least several chunks away from spawn without hitting a fixed-scene boundary, jump onto terrain, look freely with the mouse, break terrain, place terrain, collide with the changed world, pause, return to menu, close the app normally, relaunch through the bootstrapper, and retain updater functionality.

## Follow-on milestone

0.4.0 should turn the functional voxel world into an actual survival slice: persistence, physical drops, starter inventory/hotbar, timed mining/tool effectiveness, crafting, first shelter loop, day/night and the first serious material/lighting pass.
