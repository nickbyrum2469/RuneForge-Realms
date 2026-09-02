# RuneForge Realms

RuneForge Realms is a native C++23 Windows voxel survival/RPG project built around **Frontier Realms**. Its long-term progression increases the player's scale of agency from individual blocks to shapes, structures, settlements, landscapes and eventually bounded realm rules.

## Status — 0.5.0 Frontier Foundation Pass

RuneForge is a native Win32/Vulkan application, not an Electron/browser wrapper. GitHub Actions builds the Windows release, and the existing `RuneForgeBootstrap.exe` update channel installs newer published runtimes while preserving the previous runtime for rollback.

0.5.0 is the first broad commercial-quality foundation pass. It is still an early playable build, but Frontier now has a much deeper interaction, simulation, presentation and platform spine than the original renderer lab.

### Playable foundation

- Frontier-focused native main menu with **New World** and **Continue**.
- Deterministic streamed voxel terrain with Grass, Dirt, Stone, Wood, Leaves and Water identities.
- First-person WASD movement, mouse look, gravity, voxel/micro collision, jump, sprint and crouch.
- Persistent world seed, player state, inventory, mining damage, drops, block edits and micro-voxel edits.
- Automatic periodic saving plus F5 manual save.
- Nine-slot hotbar/inventory state, physical world drops, block placement and multiple mining modes.
- Chunk culling, asynchronous mesh jobs and distance-based detail tiers.

### Physical interaction and feedback

Mining no longer applies damage directly from a long camera ray. The view ray only acquires a nearby intended target; an animated first-person fist sweep decides where contact actually occurs.

- Fist reach is physically bounded near the player.
- One swing locks its front target, so breaking that block cannot make the same punch continue through and damage the block behind it.
- The visible first-person arm consumes the same `SwingPose` used by contact collision.
- Structural block damage persists independently of whether the player keeps looking at the block.
- Micro mining can carve an 8×8×8 physical occupancy state inside promoted blocks.
- Material-aware particles and physical item drops provide hit/break feedback.
- Mining and placement emit semantic material-aware audio events rather than sound filenames.

The Windows audio backend uses XAudio2 and an original procedural interaction bank with per-event variation and distance attenuation. Authored sample banks can replace the procedural bank later without changing gameplay systems.

### Water and world simulation

Water has explicit render/fluid/collision semantics, generated lowland geography, and a dedicated transparent Vulkan pass.

Generated macro water can transition into a sparse active-cell simulation around disturbed areas. Stable water sleeps; only local active cells are scheduled. The current solver uses discrete conserved volume, a fixed work budget, gravity-first movement and limited lateral equalization.

This active-region architecture is intended to generalize to future sand, fire, erosion, vegetation and structural simulation instead of brute-forcing every world cell every frame.

### Rendering and visual foundation

The renderer now separates major visual responsibilities:

- procedural sky/cloud pipeline;
- opaque voxel terrain/material pipeline;
- dedicated character material pipeline;
- transparent stylized-water pipeline;
- in-render HUD/hotbar pipeline.

Material identities include lush turf, rooted soil, rich dirt, fractured stone, oak bark/end-grain, foliage, flowers, water, character skin, blue cloth, leather and dark steel. Vegetation is thinner/sparser than the old blocky grass pass, and foliage supports alpha-cut detail.

The first-person character foundation uses separate skin/cloth/leather/steel materials instead of borrowing terrain textures. This is the base for the later full hero model, armor/equipment, dyes, emissive gear and NPC rendering.

## Controls

- **W / A / S / D** — move
- **Mouse** — look
- **Space** — jump
- **Shift** — sprint
- **Ctrl** — crouch
- **Hold Left Mouse** — swing / mine
- **Right Mouse** — place selected block
- **1–9** — hotbar selection
- **M** — cycle mining mode
- **Tab / I** — inventory
- **F5** — save now
- **Esc** — pause

## Run the Windows release

1. For the first install, download `RuneForgeRealms-Windows-x64.zip` from the repository's Releases page.
2. Extract the complete `RuneForgeRealms` folder.
3. Run `RuneForgeBootstrap.exe`.

After that, continue launching through the same bootstrapper. It checks the latest published GitHub Release, stages a newer runtime if available, preserves the previous runtime for rollback, and launches the current version.

## Native technology

- C++23
- CMake
- Win32 application shell
- Vulkan 1.3
- `volk` dynamic Vulkan loader
- HLSL → SPIR-V with Microsoft DXC
- XAudio2 Windows playback backend
- Direct2D/DirectWrite for native shell/menu surfaces
- modular chunk/world/gameplay/save/audio/render/UI subsystems

A current Vulkan-capable graphics driver is required.

## Build locally on Windows

```powershell
pwsh -File tools/setup_windows_deps.ps1
cmake -S . -B build -A x64 -DRF_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix staging/RuneForgeRealms
```

## Architecture

Responsibilities are intentionally separated:

```text
src/
├─ app/                  application/game-state orchestration
├─ audio/windows/        Windows playback backend
├─ core/                 versioning, jobs, settings
├─ game/
│  ├─ audio/             semantic audio events
│  ├─ drops/
│  ├─ interaction/       physical swing/contact logic
│  ├─ inventory/
│  ├─ mining/
│  └─ particles/
├─ platform/windows/     native window + OS input
├─ render/
│  ├─ materials/
│  ├─ scene/
│  └─ vulkan/            rendering only
├─ save/                 persistence
├─ ui/                   hub, HUD/menu/inventory presentation
├─ updater/              bootstrap/update code
└─ world/
   ├─ blocks/
   ├─ chunks/
   ├─ fluid/             active water simulation
   ├─ generation/
   ├─ growth/
   ├─ meshing/
   └─ micro/
```

Rendering consumes world/game data and draws it. It does not own inventory, crafting, drops, world-generation rules, physical interaction rules or simulation state.

## Versioning rule

Every user-facing pass merged to `main` increments the root `VERSION` file and the CMake project version. The release workflow publishes `v<version>` and uploads the Windows package. Never reuse a published version for different code.

## Roadmap

0.5.0 is a foundation milestone, not the finished visual or survival target. The next production passes build on it with authored material assets, stronger lighting/world composition, tools/crafting/equipment, the full hero character, broader fluid/environment simulation, caves/biomes/ecology, roads/settlements and larger-scale world interaction.

The feature authority remains `docs/FEATURE_MASTER_PLAN.md`; migration/engine sequencing remains in `docs/MIGRATION_ROADMAP.md`. Historical browser prototypes are reference material only and are not production dependencies.
