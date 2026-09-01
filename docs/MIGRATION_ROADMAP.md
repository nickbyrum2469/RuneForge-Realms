# RuneForge Realms — total conversion roadmap

## Conversion philosophy

This is a **behavioral rewrite**, not a syntax translation.

The frozen browser prototype remains a reference. Every useful behavior gets an acceptance criterion and a native owner. JavaScript is never copied wholesale into one C++ file.

The rebuild must prove correctness and performance in layers while always leaving the repo runnable/testable.

---

# Phase 0 — freeze, audit and architecture

Deliverables:

- freeze final WORLDWEAVE 0.10.1 behavior/source reference;
- preserve historical design/research;
- inventory every legacy function/UI surface;
- lock language/toolchain decision;
- define module/source-size rules;
- define save identity rules;
- define visual target/LOD strategy;
- create migration coverage matrix;
- CI skeleton for formatting/build/tests once source exists.

Exit: every legacy subsystem has a target native module; no production feature starts in a monolithic file; native save IDs are stable/namespaced.

---

# Phase 1 — native application skeleton

## Goal

Open a real RuneForge window and render a frame with final architectural boundaries in place.

### Build/toolchain

- C++23;
- CMake presets;
- vcpkg manifest;
- MSVC and Clang-cl developer presets;
- warnings-as-errors in owned code;
- sanitizer debug configs where supported;
- Catch2/CTest;
- Tracy integration stub.

### Platform

- SDL3 lifecycle;
- high-DPI window;
- mouse/keyboard/controller abstraction;
- fullscreen/windowed/borderless;
- logging/crash directories.

### Renderer

- Vulkan instance/device selection;
- capability report;
- swapchain;
- command pools/lists;
- VMA;
- small render graph;
- HLSL + DXC shader build;
- test triangle;
- GPU timestamps;
- device-lost reporting.

### UI

- RmlUi context rendered through Vulkan;
- one native RuneForge panel;
- DPI/controller focus.

### Exit criteria

- native executable launches with no browser runtime;
- resize/fullscreen stable;
- validation clean in smoke test;
- player UI renders in same swapchain;
- gameplay includes no Vulkan/SDL/RmlUi headers.

---

# Phase 2 — serious voxel core

## Block registry

- stable namespaced IDs;
- legacy numeric-ID import mapping;
- data-driven definitions;
- render/physics/sound/gameplay tags separated;
- material families.

## Chunk data

- benchmark section dimensions;
- palette compression where useful;
- lifecycle state machine;
- dirty flags by concern;
- efficient neighbor access.

## World generation

Behavioral reimplementation of seeded noise, terrain height, biomes, caves, ores, flora, water, landmarks, safe spawn and generator versions.

No global mutable PRNG. Golden tests verify selected seeds.

## Meshing

- exposed masks;
- greedy merge;
- packed material vertices;
- AO/light fields;
- asynchronous jobs;
- bounded GPU upload;
- dirty section + affected neighbors only.

## Streaming

- camera-centered priority queue;
- load/generate/mesh/upload/unload states;
- collision/visible work highest priority;
- no synchronous chunk burst at borders.

Exit: deterministic native world supports long traversal without unbounded memory growth or frame-blocking chunk builds.

---

# Phase 3 — visual pillar

## Goal

Prove the supplied block/tree/water visual target before broad content production.

Hero materials: grass/turf, dirt/soil, fractured stone, wood/bark, leaves, water, one ore and one Rune/Echo crystal.

Work:

- PBR definitions;
- KTX2 pipeline;
- world-space variation;
- exposed-edge treatment;
- deterministic micro-detail generator;
- near/mid/far LOD;
- high-detail oak/pine;
- HDR;
- sun/cascaded shadows;
- voxel AO + screen AO;
- contact depth;
- sky/fog;
- water absorption/refraction/Fresnel;
- wet surfaces;
- temporal stability;
- tone map/bloom;
- optional FSR after native rendering is correct.

Benchmark scenes: material cubes, forest edge, stone ruin, shallow/deep water, sunset, rain/wetness and dense-detail stress.

Exit: close materials have real depth; LOD transitions are stable; test scene meets frame budgets; screenshots/tests exist; no world data stores per-speck microvoxels.

---

# Phase 4 — player + survival vertical slice

Player: first-person movement/collision, sprint/crouch/jump, swimming/current influence, health/stamina, fall damage, camera accessibility.

Interaction: targeting, timed mining, tool effectiveness, placement, collision rules, physical drops, pickup and material/audio/particle feedback.

Inventory/crafting: item registry, stacks, hotbar, inventory grid, equipment foundation, workbench and starter recipes.

Progression: XP/skills, Resonance, first meaningful TOOL Scale unlock, Pulse/connected mining and anti-grind hooks.

Exit: New Game -> spawn -> explore -> gather -> craft tool -> build shelter -> save/quit -> relaunch/continue -> unlock/use first macro verb.

---

# Phase 5 — production persistence

This overlaps Phase 4; saves cannot be bolted on later.

- world metadata;
- region containers;
- chunk delta encoding;
- zstd;
- entities/players/progression/blueprints;
- checksums;
- transaction journal;
- crash recovery;
- backup before migration;
- migration chain;
- export/import archive;
- save inspector;
- invalid-input testing.

## Legacy import

Dedicated importer reads supported WORLDWEAVE JSON exports and maps seed/generator, player state where sensible, modified blocks, inventory/hotbar, tool tiers, XP/skills, Resonance/upgrades, discoveries and convertible blueprints.

The game does not read browser `localStorage` directly.

Exit: forced crash during save recovers; migration fixtures pass; large edit sets round-trip; backups restore; saves live outside install versions.

---

# Phase 6 — RuneForge UI + live main menu

Reusable UI kit: frame/header/gem, buttons, item cells/grids, rarity states, tooltips, tabs, equipment slots, character preview, recipes, quantity controls, hotbar, modal/toast, settings/keybind widgets.

Screens: main menu, world select/create, pause, settings, inventory/equipment, workbench/crafting, storage chest, character/progression, save/export/backup and credits. Mods/Multiplayer entries only become active when real.

Live menu scene: curated RuneForge city/valley, animated foliage/cloud/fog/water/lanterns, slow camera drift, real renderer/materials.

Exit: no Chromium process; UI scales across DPI/aspect ratios; controller navigates primary screens; menu background remains responsive.

---

# Phase 7 — Shape Scale

- direct/smart/volume selection;
- line/wall/floor/box/cylinder/sphere;
- stairs/ramp/scaffold helpers;
- material substitution;
- ghost preview;
- support/water warnings;
- material reservation;
- async edit calculation;
- per-chunk transaction commit;
- edit journal undo/redo;
- stable operation IDs.

Exit: large walls/floors can be previewed/cancelled/reversed with correct resource accounting and no single-frame stall.

---

# Phase 8 — Structure Scale

- recognition foundation;
- explicit capture;
- local voxel coordinates;
- blueprint format/library/thumbnail;
- rotate/mirror/move;
- material replacement;
- sockets;
- support/mass metadata;
- mobile assembly prototype;
- Jolt aggregate collision for detached structures.

Exit: build a house, capture it, move it, rotate it, undo it; survival copies consume correct materials; structure remains editable; moving assembly is not one physics body per block.

---

# Phase 9 — Settlement Scale

- navigation hierarchy;
- room semantics;
- inhabitants;
- schedules/needs at chosen abstraction;
- task graph;
- demonstrated-job recording;
- delegated mining/farming/building/repair;
- logistics reservations;
- road usage;
- districts;
- projects/requests;
- trade;
- World Memory from actual use.

Exit: NPC uses player-built valid room, player teaches/delegates a routine, roads influence movement/growth, distant simulation uses coarse events.

---

# Phase 10 — Landscape + Epoch

- terrain project system;
- terrace/smooth/raise/lower;
- large excavation;
- water redirection;
- floodgate/aquifer;
- ecology succession;
- soil/biome restoration;
- delegated regional projects;
- World Memory thresholds;
- first Epoch transition;
- new resources/rules without world wipe.

Exit: valley-scale operation is safe and streamed; old structures survive Epoch transition and gain new utility.

---

# Phase 11 — combat/content depth

Combat can begin earlier in simple form. Deepen with enemy/creature families, ecology integration, ruins/dungeons, constrained equipment modifiers, world-shaping combat verbs, terrain-affecting enemies, settlement defense and major threats/lore.

---

# Phase 12 — co-op/networking

- authoritative host/server;
- player replication;
- chunk/delta interest management;
- entity replication;
- operation replication;
- edit permissions;
- host rollback/log;
- structure/settlement authority;
- join-in-progress streaming;
- security/validation.

Network semantic game/world state and operations, not renderer state.

---

# Phase 13 — modding/content packs

- stable public registries;
- content manifest;
- dependency/version rules;
- missing-content placeholders;
- security model for scripts if exposed;
- load order;
- UI hooks;
- tools/docs;
- save dependency recording.

---

# Phase 14 — updater/release hardening

A simple bootstrapper can exist earlier. Harden with stable/preview channels, manifests, signed/checksummed artifacts, staged/resumable download, atomic switch, rollback/last-known-good, repair/verify, release notes, crash-loop detection and updater self-update strategy.

---

# Intentionally retired browser implementation details

- browser `localStorage` persistence;
- WebGL-specific compatibility code;
- browser pointer-lock quirks;
- DOM mutation from gameplay;
- canvas-generated mode-card art;
- 180 generated launcher entries;
- numeric IDs as persistent identity;
- single-file globals.

Their useful **user-facing purpose** is preserved where still valuable.

---

# Migration coverage process

For every item in `LEGACY_CODE_AUDIT.md`:

```text
legacy behavior/function
 -> owner module
 -> native acceptance test
 -> implementation PR
 -> migration status
 -> retirement note
```

A function is not ported because similarly named C++ exists. It is ported when behavior is tested and works natively.

---

# First shippable internal build

The first build worth handing to a player should include native packaging/bootstrapper, main menu, New Game/Continue, one excellent biome with caves/water, reference-quality hero materials, movement/mining/building, inventory/crafting, day/night/weather, first progression verb, persistent save/load, settings, diagnostics and stable frame pacing.

That becomes the foundation—not another disposable proof of concept.
