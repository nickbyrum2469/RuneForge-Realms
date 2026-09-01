# RuneForge Realms — legacy migration coverage matrix

`LEGACY_CODE_AUDIT.md` contains the exhaustive function/helper inventory. This matrix tracks native ownership and completion at a subsystem level so migration work stays manageable.

Status values: `NOT STARTED`, `TEST DEFINED`, `IN PROGRESS`, `NATIVE COMPLETE`, `RETIRED`.

| Legacy behavior | Representative legacy functions | Native owner | Status |
|---|---|---|---|
| Global crash routing | `message`, `show` | `engine/diagnostics` | NOT STARTED |
| Hash/noise foundation | `hashInt`, `hash2`, `hash3`, `noise2`, `noise3`, `fbm2`, `fbm3` | `world/generation` | NOT STARTED |
| Legacy generator compatibility | `v1Hash*`, `v1Noise*`, `generateV1Chunk` | `tools/save_migrator` + importer | NOT STARTED |
| Terrain/biomes | `terrainInfo`, `slopeAt` | `world/generation` | NOT STARTED |
| Underwater classification/flow | `underwaterBiomeAt`, `flowVectorAt` | `world/generation` + `world/fluids` | NOT STARTED |
| Caves/ores | `caveAt`, `oreAt` | `world/generation` | NOT STARTED |
| Flora generation | `treeCandidate`, `addOak`, `addPine`, `addCactus`, `addFallenLog` | `world/generation` + `world/ecology` | NOT STARTED |
| Landmarks | `addStandingStones`, `addRuin`, `addEchoCairn`, `landmarkNear` | `world/landmarks` | NOT STARTED |
| Spawn selection | `findSpawn` | `world/generation` | NOT STARTED |
| Player collision | `bounds`, `intersects`, `clipAxis`, `canStand` | `game/player` + `engine/physics` | NOT STARTED |
| Player movement | `moveHorizontalWithStep`, physics `update` | `game/player` | NOT STARTED |
| Procedural prototype audio | `tone`, `noise`, `play` | `engine/audio` | RETIRED |
| Environmental audio behavior | `updateEnvironment` | `engine/audio` | NOT STARTED |
| Browser/WebGL context | `createWebGLContext` | `engine/render/backend/vulkan` | RETIRED |
| Settings persistence | `loadSettings`, `saveSettings` | `game/app/settings` | NOT STARTED |
| Block data access/mutation | `getBlock`, `setBlock`, `markDirty` | `world/voxel` | NOT STARTED |
| Dynamic water cells | `setFluidCell`, `processFluids`, `waterFlowVector` | `world/fluids` | NOT STARTED |
| WebGL shader/program plumbing | `shader`, `linkProgram`, `program` | `engine/render/backend/vulkan` | RETIRED |
| Chunk meshing | `pushFace`, `emitGreedy`, `greedyMasksToMesh`, `buildChunk` | `world/meshing` | NOT STARTED |
| Chunk streaming/cache | `ensureChunks`, `resetMeshes` | `world/streaming` | NOT STARTED |
| Raycast/reach | `raycast`, `reach` | `game/interaction` | NOT STARTED |
| Resonance/Scale | `gain`, `recalcRank`, `rankProgress`, `buyUpgrade` | `game/progression` | NOT STARTED |
| XP/skills | `xpNeeded`, `gainXP` | `game/progression` | NOT STARTED |
| Hotbar/inventory | `normalizeHotbar`, `addInventory`, `spendInventory` | `game/inventory` | NOT STARTED |
| World item drops | `spawnDrop`, `updateDrops` | `game/items` | NOT STARTED |
| Mining/tool speed | `breakBlockAt`, `toolSpeedFor`, `updateMining` | `game/interaction` | NOT STARTED |
| Pulse Mine | `pulseMine` | `game/progression` + `game/building` | NOT STARTED |
| Block placement | `placeSelected` | `game/interaction` | NOT STARTED |
| Shape tools | `shapeCells`, `builderClick`, `builderLimit` | `game/building` | NOT STARTED |
| Undo/redo | `undoBuild`, `redoBuild` | `game/building/EditJournal` | NOT STARTED |
| Structure capture | `captureStructure` | `game/structures` | NOT STARTED |
| Structure transform | `transformedBlueprint`, `rotateStructure`, `mirrorStructure` | `game/structures` | NOT STARTED |
| Blueprint paste | `pasteStructure` | `game/structures` + edit transactions | NOT STARTED |
| First-person survival update | `physicsStep`, `respawn`, `safeSpawn` | `game/player` | NOT STARTED |
| Day/night | `timeState`, `formatTime` | `world/weather` | NOT STARTED |
| Weather/lightning | `weatherStep`, `drawWeather` | `world/weather` + renderer | NOT STARTED |
| Legacy renderer `draw()` | `draw`, `bindWorldAttribs` | render graph/pass modules | RETIRED |
| HUD data | `currentObjective`, `updateHUD`, `renderHotbar` | game view models + RmlUi | NOT STARTED |
| Browser pointer lock | `requestMouseLock` | SDL relative-mouse input | RETIRED |
| Pause/menu behavior | `pause`, `resume`, `openMenu`, `showTab` | native UI controllers | NOT STARTED |
| Crafting | `hasCost`, `spendCost`, `craft` | `game/crafting` | NOT STARTED |
| Diagnostics/self-test | `runSelfTest`, `diagnosticsSnapshot` | `engine/diagnostics` + tests | NOT STARTED |
| Save packing | `packModified`, `packFluidLevels`, `packDrops`, `packToolState` | `world/persistence` | NOT STARTED |
| Save validation/migration | `migrate`, `sanitizeWorldData`, `sanitizePlayerData` | `world/persistence` | NOT STARTED |
| Autosave/backup | `saveLocal`, `restoreBackup` | `world/persistence` | NOT STARTED |
| Export/import | `exportWorld`, `loadObject` | `world/persistence` + tools | NOT STARTED |
| Offline progression | `applyOffline` | `game/progression` + world simulation | NOT STARTED |
| New world/mode config | `newWorld`, `applyModeConfig`, `launchMode` | `game/app` | NOT STARTED |
| Browser main tick | `tick` | `game/app/GameLoop` | RETIRED |
| Hub graphics probe | `probeGraphics`, `formatSystemCheck` | launcher/native diagnostics | NOT STARTED |
| Canvas mode art | `paintModeArt`, `drawRidge`, `drawTree` | real 3D menu scenes | RETIRED |
| Discover cards/favorites | `modeCard`, `toggleFavorite`, `renderDiscover` | simplified native world/challenge browser | REDESIGN |
| Saved-world browser | `renderWorlds` | native world browser | NOT STARTED |
| Custom seed creation | `createCustom` | New World flow | NOT STARTED |
| Hub startup | `bindHub`, `initializeHub` | native main menu/bootstrap flow | REDESIGN |

## Completion rule

A row reaches `NATIVE COMPLETE` only when it has:

1. a named native owner/file set;
2. automated test or explicit manual acceptance case;
3. native implementation merged;
4. save/migration implications documented;
5. legacy behavior confirmed covered or intentionally redesigned.
