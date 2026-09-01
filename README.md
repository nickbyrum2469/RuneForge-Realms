# RuneForge Realms

RuneForge Realms is a native C++ voxel survival/RPG sandbox whose defining progression increases the player's **scale of agency** from individual blocks to shapes, structures, settlements, landscapes and eventually bounded realm rules.

## Status — 0.3.0 Frontier Playable Foundation

RuneForge is a real native C++23 Windows application, not an Electron/browser wrapper. Releases are built by GitHub Actions and installed through the existing `RuneForgeBootstrap.exe` update channel.

The 0.3.0 milestone retires the consumer-facing renderer lab and begins the actual flagship game: **Frontier Realms**.

Current player-facing foundation:

- Frontier-focused native main menu with a richer layered fantasy voxel scene;
- **New World** and **Continue** flow instead of six fake standalone modes;
- real deterministic multi-chunk voxel terrain;
- Grass, Dirt, Stone, Wood and Leaves block identities;
- first-person WASD movement and mouse look;
- gravity, voxel collision, jumping, sprinting and crouching;
- left-click block breaking and right-click block placement;
- number keys 1–5 select placeable block type;
- material-aware greedy meshing;
- real vertex/index geometry uploaded to device-local GPU buffers;
- Vulkan indexed rendering from actual world data rather than shader-generated demo cubes;
- local persistent Frontier save containing seed, player state and block edits;
- automatic periodic saving plus F5 manual save;
- Escape pauses and releases the mouse; while paused, H saves and returns to the main menu;
- Continue becomes available after a Frontier save exists.

The 0.3 world is intentionally finite and the materials are intentionally still development-grade. This build exists to prove the complete interaction spine on real hardware:

**menu → create/load world → walk → collide → jump → break → place → save → return → continue**.

Once that path is stable, the next survival pass adds physical drops, inventory/hotbar ownership, timed mining, starter tools/crafting and deeper persistence. The visual pillar then replaces development materials with the high-depth RuneForge grass, stone, wood, foliage and water target documented in `docs/VISUAL_RENDERING_PLAN.md`.

## Run the Windows release

1. Open the repository's **Releases** page.
2. Download `RuneForgeRealms-Windows-x64.zip` only for the first install.
3. Extract the entire `RuneForgeRealms` folder.
4. Run `RuneForgeBootstrap.exe`.

After that, keep launching RuneForge through the same bootstrapper. It checks the repository's latest GitHub Release, stages a newer runtime, preserves the previous runtime for rollback, and launches the current version.

## Frontier 0.3 controls

- **W / A / S / D** — move
- **Mouse** — look
- **Space** — jump
- **Shift** — sprint
- **Ctrl** — crouch
- **Left mouse** — break targeted block
- **Right mouse** — place selected block
- **1–5** — Grass / Dirt / Stone / Wood / Leaves
- **F5** — save now
- **Esc** — pause / resume
- **H while paused** — save and return to main menu

The title bar currently exposes selected material and coordinates as temporary development HUD information. A proper in-render HUD/hotbar replaces this in the survival UI pass.

## Native technology

- C++23
- CMake
- Win32 application shell
- Vulkan 1.3 renderer
- `volk` dynamic Vulkan loader
- HLSL compiled to SPIR-V with Microsoft DXC
- Direct2D/DirectWrite for the transitional native main-menu painter
- dedicated chunk/world/player/save modules rather than a monolithic game source file

A current Vulkan-capable graphics driver is required.

## Build locally on Windows

```powershell
pwsh -File tools/setup_windows_deps.ps1
cmake -S . -B build -A x64 -DRF_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix staging/RuneForgeRealms
```

## Versioning rule

Every user-facing pass merged to `main` increments the root `VERSION` file and `project(... VERSION ...)` value. The release workflow creates `v<version>` and uploads the Windows package. Never reuse a published version for different code.

## Architecture and roadmap

Read `docs/README.md`, `docs/FRONTIER_REALMS_0_3_PLAN.md`, `docs/ARCHITECTURE.md`, `docs/MIGRATION_ROADMAP.md`, and `docs/FEATURE_MASTER_PLAN.md`.

The historical browser prototype is reference material only and is not a production dependency.
