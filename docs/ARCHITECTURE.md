# Native architecture — RuneForge Realms

## Goal

Rebuild the project from scratch without carrying forward the old single-file architecture.

The legacy browser prototype proved ideas. The native version must make those ideas maintainable for years.

> **A subsystem owns its data, exposes a narrow interface, has tests at its boundary, and does not reach through another subsystem to mutate internal state.**

## Source-size rules

- Normal `.cpp` target: **150–350 lines**.
- Soft review threshold: **400 lines**.
- Strong split threshold: **500 lines**.
- Anything above **800 lines** requires a written reason and should normally be generated data, third-party code, a shader table, or serialization schema.
- Headers should normally stay below **250 lines**.
- One primary responsibility per source file.
- Large lookup/data tables belong in data files, not implementation files.
- No `Utility.cpp` dumping ground.
- No `Game.cpp` or `World.cpp` owning unrelated systems.
- No backend-specific Vulkan header outside `engine/render/backend/vulkan/`.
- No gameplay file calls SDL, Vulkan, Jolt or RmlUi directly.

These are review rules, not compiler rules. CI can add a line-count warning later.

## Repository layout

```text
RuneForge-Realms/
  CMakeLists.txt
  CMakePresets.json
  vcpkg.json
  README.md

  cmake/
  config/
  assets/
    ui/ fonts/ textures/ materials/ meshes/ audio/ localization/
  shaders/
    common/ voxel/ lighting/ water/ atmosphere/ ui/ post/

  engine/
    core/ platform/ jobs/ math/ render/ physics/ audio/ ui/ assets/ diagnostics/

  world/
    voxel/ blocks/ chunks/ generation/ streaming/ meshing/
    fluids/ ecology/ weather/ landmarks/ persistence/

  game/
    app/ player/ interaction/ inventory/ crafting/ progression/
    building/ structures/ settlements/ combat/ exploration/ objectives/ modes/

  launcher/
  updater/
  network/
  modding/
  tools/
  tests/
  docs/
```

## Build-target layout

Use CMake library targets to enforce boundaries:

```text
rfr_core
rfr_platform
rfr_jobs
rfr_render
rfr_render_vulkan
rfr_assets
rfr_physics
rfr_audio
rfr_ui
rfr_world_voxel
rfr_world_gen
rfr_world_streaming
rfr_world_sim
rfr_persistence
rfr_gameplay
rfr_structures
rfr_settlements
rfr_progression
rfr_launcher
rfr_updater
rfr_game
```

`rfr_game` composes targets. It must not become the implementation location for everything.

## Dependency direction

Backend/platform dependencies flow downward. Game modules depend on stable RuneForge interfaces, not vendor libraries.

```text
launcher/app
    |
 gameplay ---- progression ---- structures ---- settlements
    |                \             |              /
    +----------------- world ---------------------+
                         |
               physics / rendering
                         |
                        core
```

# 1. Core — `engine/core/`

Responsibilities: common types, IDs/GUIDs, error/result types, logging, time, filesystem paths, configuration, deterministic hashing, version information and feature flags.

```text
AppVersion.h/.cpp
Guid.h/.cpp
Result.h
Log.h/.cpp
Paths.h/.cpp
Config.h/.cpp
Hash.h/.cpp
```

No gameplay concepts here.

# 2. Platform — `engine/platform/`

SDL3 lives here. Responsibilities: application lifecycle, window/high DPI, input, cursor/relative mouse, clipboard, OS integration and crash hooks.

```text
Platform.h
SdlPlatform.cpp
Window.h
SdlWindow.cpp
InputDevice.h
SdlInput.cpp
```

# 3. Job system — `engine/jobs/`

World generation, meshing, compression and large-edit analysis cannot stall the frame thread.

```text
JobSystem.h/.cpp
JobHandle.h
JobQueue.h/.cpp
```

Requirements: worker pool, priorities, cancellation, dependencies, main-thread completion queue, timings and a deterministic one-worker test mode. Work stealing and specialized IO/render queues can be added after profiling.

# 4. Rendering — `engine/render/`

Backend-independent front end:

```text
RenderDevice.h
RenderResource.h
Buffer.h
Texture.h
Sampler.h
Pipeline.h
CommandList.h
RenderGraph.h/.cpp
FrameContext.h
Camera.h
Material.h
MaterialRegistry.cpp
RenderStats.h
```

Vulkan backend:

```text
backend/vulkan/
  VulkanDevice.cpp
  VulkanSwapchain.cpp
  VulkanBuffer.cpp
  VulkanTexture.cpp
  VulkanPipeline.cpp
  VulkanDescriptors.cpp
  VulkanCommands.cpp
  VulkanSync.cpp
  VulkanQueries.cpp
  VulkanDebug.cpp
```

A feature such as voxel rendering does not belong in `VulkanDevice.cpp`.

### Render graph

Start early enough to prevent another monolithic `draw()` function.

Initial passes:

```text
DepthPrepass
ShadowPass
VoxelOpaque
DynamicOpaque
Water
Transparent
WorldEffects
Ui
ToneMap
Present
```

Later passes can add HiZ, GPU culling, GTAO, contact shadows, volumetric fog, SSR, radiance probes, TAA, upscaling and bloom.

Each pass declares inputs/outputs, receives `FrameContext`, records commands, has CPU/GPU timestamps and can be disabled by graphics tier.

# 5. Voxel world

Voxel data remains independent from renderer meshes.

## Block registry — `world/blocks/`

```text
BlockId.h
BlockDefinition.h
BlockRegistry.h/.cpp
BlockLoader.cpp
```

Production IDs are stable namespaced strings mapped to compact runtime integers:

```text
core:air
core:grass
core:dirt
core:granite
core:oak_log
rune:amberstone
rune:echo_crystal
```

Saves store stable identity/mapping information; they never assume integer `7` means the same block forever.

## Chunk storage — `world/chunks/`

```text
ChunkCoord.h
ChunkSection.h/.cpp
Chunk.h/.cpp
ChunkState.h
ChunkStore.h/.cpp
```

Starting model: 16×16 horizontal chunks, vertical sections, palette compression where useful, separate light/fluid/state channels and dirty flags for mesh/collision/save/simulation.

# 6. World generation — `world/generation/`

```text
WorldGenerator.h
GeneratorVersion.h
TerrainNoise.cpp
ClimateMap.cpp
BiomeSelector.cpp
TerrainShape.cpp
CaveGenerator.cpp
OreGenerator.cpp
FloraGenerator.cpp
LandmarkGenerator.cpp
SpawnFinder.cpp
```

Do not rebuild the old compressed script as one `WorldGenerator.cpp`.

Every stage receives world seed, generator version and coordinates. No global mutable PRNG. Golden tests record semantic/hashes for selected seeds.

# 7. Meshing — `world/meshing/`

```text
VoxelMesher.h
FaceVisibility.cpp
GreedyMesher.cpp
MeshBuilder.cpp
VoxelAo.cpp
MicroDetailBuilder.cpp
WaterMesher.cpp
CollisionMesher.cpp
MeshUploadQueue.cpp
```

Pipeline:

```text
Chunk Section
 -> face/material classification
 -> greedy macro mesh
 -> AO/material attributes
 -> optional micro-detail instances
 -> CPU mesh package
 -> bounded GPU upload
```

Meshing happens off the frame thread.

# 8. Streaming — `world/streaming/`

```text
StreamPriority.h
ChunkRequest.h
ChunkStreamer.cpp
RegionCache.cpp
StreamBudget.cpp
```

Priority: player collision safety -> visible terrain -> near simulation -> preload -> far generation.

Suggested state machine:

```text
Unloaded -> Requested -> Loading/Generating -> ReadyData
 -> Meshing -> ReadyRender -> Active -> Sleeping -> Evicting
```

Never `if missing then synchronously generate` in gameplay.

# 9. Fluids — `world/fluids/`

```text
FluidCell.h
FluidChunkState.h
FluidSolver.cpp
FluidSleep.cpp
FlowField.cpp
```

Stable water sleeps. Use bounded updates. Later add pressure classes, aquifers, floodgates, river routing, lava and temperature interaction.

# 10. Ecology/weather

```text
world/ecology/
  EcologyState.h
  FloraRules.cpp
  Succession.cpp
  MigrationEvents.cpp

world/weather/
  WeatherState.h
  WeatherSystem.cpp
  Precipitation.cpp
  StormEvents.cpp
```

Near-player simulation can be detailed; distant simulation uses coarse scheduled events. Never tick every plant in the world every frame.

# 11. Player and interaction

```text
game/player/
  PlayerState.h
  PlayerController.cpp
  PlayerMovement.cpp
  PlayerCamera.cpp
  PlayerVitals.cpp

game/interaction/
  RaycastTarget.cpp
  BlockBreakAction.cpp
  BlockPlaceAction.cpp
  InteractionReach.cpp
  PickupSystem.cpp
```

This replaces the old `physicsStep()` style where movement, skill gain, discovery, audio and world updates were mixed together.

# 12. Inventory and crafting

```text
game/inventory/
  ItemId.h
  ItemStack.h
  Inventory.h/.cpp
  Hotbar.cpp
  Equipment.cpp
  StorageContainer.cpp
  StorageNetwork.cpp

game/crafting/
  Recipe.h
  RecipeRegistry.cpp
  CraftingService.cpp
  Workbench.cpp
  CraftingQueue.cpp
```

UI reads view-model state; it never owns inventory truth.

# 13. Progression

```text
game/progression/
  Resonance.h
  ResonanceSystem.cpp
  ProgressionEvent.h/.cpp
  ScaleDefinition.h
  ScaleSystem.cpp
  SkillSystem.cpp
  WorldMemory.cpp
  EpochSystem.cpp
  ProgressionAntiExploit.cpp
```

Rules: repeated identical actions diminish progression yield; discoveries beat grind; Scales unlock verbs; numerical upgrades support verbs.

# 14. Building tools

```text
game/building/
  BuildSelection.h
  BuildPreview.cpp
  BuildCost.cpp
  EditOperation.h
  EditTransaction.cpp
  EditJournal.cpp
  tools/
    DirectTool.cpp
    LineTool.cpp
    WallTool.cpp
    PlaneTool.cpp
    BoxTool.cpp
    SphereTool.cpp
    CylinderTool.cpp
    ReplaceTool.cpp
    RampTool.cpp
```

Every macro tool follows one transaction path:

```text
input -> selection -> proposed edit -> validate -> cost
 -> preview -> commit -> dirty affected systems -> journal
```

No tool directly mutates huge ranges behind the transaction system.

# 15. Structures

```text
game/structures/
  StructureId.h
  StructureBounds.cpp
  StructureRecognition.cpp
  StructureEntity.cpp
  StructureVoxelGrid.cpp
  StructureTransform.cpp
  StructureSockets.cpp
  Blueprint.h
  BlueprintSerializer.cpp
  BlueprintLibrary.cpp
  MobileAssembly.cpp
```

Requirements: capture, name, rotate, mirror, move, material substitution, blueprint library, sockets and eventually detached mobile local voxel grids.

# 16. Settlements

```text
game/settlements/
  SettlementId.h
  SettlementBoundary.cpp
  RoomType.h
  RoomRecognition.cpp
  RoadGraph.cpp
  NavigationHierarchy.cpp
  SettlerState.cpp
  NeedsSystem.cpp
  ScheduleSystem.cpp
  JobDefinition.h
  JobTeaching.cpp
  TaskPlanner.cpp
  ReservationSystem.cpp
  TradeRoute.cpp
  DistrictPolicy.cpp
```

Intended flow:

```text
player performs work
 -> game recognizes the process
 -> player can teach/delegate it
 -> settlement performs routine versions
```

# 17. Persistence — `world/persistence/`

```text
SaveVersion.h
SaveManifest.cpp
RegionFile.cpp
RegionCodec.cpp
ChunkDeltaCodec.cpp
EntityStore.cpp
PlayerStore.cpp
BlueprintStore.cpp
SaveJournal.cpp
BackupManager.cpp
SaveMigration.cpp
SaveValidator.cpp
WorldExport.cpp
WorldImport.cpp
```

Suggested world directory:

```text
Saves/MyWorld/
  world.json
  players/
  regions/
  entities/
  structures/
  blueprints/
  journal/
  backups/
```

Invariants: procedural base is not serialized wholesale; modifications are deltas; region generator version is recorded; migration backs up first; journal supports crash recovery; import validates checksums/IDs; unknown mod content becomes placeholder data, not deletion.

# 18. UI

```text
engine/ui/
  UiSystem.cpp
  UiRendererVulkan.cpp
  UiTextureCache.cpp
  UiAudio.cpp
  UiFocus.cpp
  UiDataModel.cpp

assets/ui/
  common/
  main_menu/
  inventory/
  crafting/
  storage/
  settings/
  hud/
```

Player-facing screens are RmlUi data/style assets plus small view-model/controllers, not 2,000-line menu classes.

```text
game/ui/inventory/
  InventoryViewModel.cpp
  InventoryActions.cpp
assets/ui/inventory/
  inventory.rml
  inventory.rcss
```

This markup is parsed by native RmlUi; it is not Electron.

# 19. Launcher

Initially RuneForge is one flagship game. Legacy alternate modes become presets, challenge realms or later experiences rather than 180 fake finished games.

```text
launcher/
  LauncherApp.cpp
  HubModel.cpp
  WorldBrowserModel.cpp
  ReleaseStatus.cpp
```

# 20. Native updater

A separate executable owns updates because the running game should never replace its own files.

```text
updater/
  UpdaterMain.cpp
  UpdateManifest.cpp
  ReleaseClient.cpp
  DownloadJob.cpp
  HashVerifier.cpp
  SignatureVerifier.cpp
  InstallPlan.cpp
  AtomicSwap.cpp
  Rollback.cpp
  UpdateLog.cpp
```

Updater downloads into a new version folder, verifies, atomically changes the active-version pointer, launches a health check and rolls back on failure. Saves live outside version folders.

# 21. Networking

Designed now, implemented later: authoritative server, chunk/region interest management, operation-level macro-edit replication, entity snapshots/deltas, permissions, attribution and rollback.

Do not send one network message per changed voxel for a 100,000-block operation. Replicate validated semantic operations plus result hashes/deltas.

# 22. Modding

Stable namespaced IDs. Content packs can add blocks, items, recipes, materials, biomes, landmarks, structures, creatures, progression definitions and settlement room types. A scripting layer can be added only after the core APIs stabilize.

# 23. Diagnostics

Keep and improve prototype telemetry: FPS, GPU, draw/triangle counts, chunk counts, mesh/worldgen timings, dirty/fluid queues, save stats, player state and progression.

Add CPU frame graph, GPU pass timings, VRAM allocation, streaming latency, job utilization, worst hitch reason, save IO time and compression ratios.

# 24. Testing architecture

Required deterministic tests: hashing/noise, selected worldgen seeds, generator-version compatibility, block mapping, chunk delta codec, corruption handling, save migration, blueprint transforms, macro cost/undo, resource conservation, fluid sleep/wake, room recognition and updater version ordering.

Stress tests: chunk-border sprinting, teleporting, 100k-block previews/commits, mass water disturbance, hundreds of drops, thousands of entities and rapid save/load/update cycles.

# 25. Golden rule

We do **not** port the old JavaScript function-by-function into a 10,000-line `RuneForgeGame.cpp`.

We port behavior into the subsystem that should own it.

That is the total conversion.
