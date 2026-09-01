# RuneForge Realms 0.4.0 — Frontier Visual Survival

## Purpose

0.4.0 turns Frontier Realms from an editable voxel foundation into the first visual-survival slice while preserving the modular engine boundaries established in 0.3.x.

## Two-scale voxel rule

A normal world voxel remains the terrain/storage unit. Its physical high-resolution state is an 8×8×8 microvoxel field (512 cells). Pristine blocks do not allocate 512 objects; only blocks whose physical shape deviates from pristine persist a compact 512-bit occupancy mask.

Visual quality is never promoted by interaction. Pristine nearby blocks already use RuneForge's high-detail material and surface language. Mining changes physical occupancy, not graphics resolution.

## Mining modes

- **Block** — repeated hardness-based structural damage; the logical block remains whole until its damage threshold is reached, then breaks into a collectible block drop.
- **Micro** — precise local chipping removes real microvoxels. Collision and raycasts respect the removed cells. Removed micro-material is accumulated by material; 512 harvested cells convert to one collectible block equivalent.
- **Mixed** — default survival behavior. Each strike applies structural damage and a small physical chip. At structural failure the remaining block collapses into a collectible block drop.

Mining logic lives in `game/mining/`, not the renderer.

## Surface detail and LOD

`MicroDetailBuilder` provides dense pristine silhouette detail such as grass nodes, stone plates and leaf clumps. `MicroVoxelMesher` provides actual physical damaged geometry.

Surface detail tiers are selected by camera distance only:

- Hero: nearest chunks, dense grass/flowers/stone plates/foliage.
- Standard: reduced decorative density while preserving the same shader/material language.
- Distant: no expensive decorative microgeometry; coarse geometry still uses the same procedural material breakup.

Physical microvoxel damage remains real at every tier.

## Living grass

Grass exposes an 8×8 surface growth field. Nodes are deterministically derived from world seed, block coordinate, node coordinate and persistent world age. Nodes vary in presence, growth stage, dimensions and flower type. Missing top microcells no longer support their corresponding grass nodes.

## Survival inventory loop

Block failure produces `WorldDrop` entities. Drops have gravity, bounce, drag, pickup delay and pickup radius. Picked-up items enter a 9-slot hotbar plus 27-slot backpack. Right-click placement consumes the selected inventory stack.

World drops are rendered through a dedicated dynamic geometry path rather than remeshing terrain.

## Persistence schema 3

Schema 3 persists:

- world seed and age;
- player position/orientation;
- normal region edits;
- microvoxel region occupancy;
- selected mining mode;
- partial block mining damage;
- fractional micro-harvest material mass;
- inventory and selected hotbar;
- live collectible world drops.

Legacy schema 1/2 saves remain loadable and migrate forward on save.

## UI identity

The native hub and inventory move toward the supplied RuneForge reference language: deep fantasy scene depth, forged charcoal panels, bronze/gold framing, blue gem accents, recessed item slots, equipment framing and a coherent in-game HUD.

## Ownership boundaries

- `world/micro/` — physical microvoxel state and serialized edits.
- `world/meshing/` — coarse, physical-micro and decorative surface meshing.
- `world/growth/` — living grass/flower simulation.
- `game/mining/` — mining modes and structural damage.
- `game/drops/` — collectible entity simulation.
- `game/inventory/` — stacks, hotbar and backpack data.
- `render/scene/` — dynamic scene mesh construction/culling.
- `render/vulkan/` — GPU ownership/drawing only.
- `ui/inventory/` — inventory presentation.
- `save/world/` — region and micro-region persistence.

No gameplay feature should be moved into Vulkan code merely because Vulkan displays it.

## Release gate

0.4.0 is releaseable only after both Windows native/Vulkan/shader CI and Linux portable-core tests pass, followed by a successful rebuild of the exact merged `main` commit and publication of the Windows x64 release package.
