# Legacy code audit — WORLDWEAVE 0.10.1 → RuneForge Realms

> **Status:** Frozen behavior reference. This documents the final browser prototype so the native rebuild preserves useful behavior without copying the monolithic architecture.

## Source snapshot

- Frozen source reference: `WORLDWEAVE_PORTABLE_0.10.1.html`
- Source size: **235,501 characters / 1,166 lines**
- Embedded JavaScript blocks: **6**
- Embedded stylesheet blocks: **1**
- Player/launcher DOM element IDs: **117**
- Named function/arrow-function occurrences: **309**; **308 unique within script scopes**

The native code must never recreate a 235 KB single source file.

## Script-by-script function inventory

### Script 0 — global crash/error guard
Native destination: `engine/diagnostics` + app crash handling.

`message`, `show`.

### Script 1 — world generation / biomes / landmarks
Native destination: `world/generation` + `world/landmarks`.

`floorDiv`, `hashInt`, `hash2`, `hash3`, `smooth`, `noise2`, `noise3`, `n`, `fbm2`, `fbm3`, `v1Hash2`, `v1Hash3`, `v1Noise2`, `v1Noise3`, `n#2`, `v1HeightAt`, `getBaseV1Raw`, `generateV1Chunk`, `getBaseV1`, `terrainInfoV1`, `terrainInfo`, `slopeAt`, `underwaterBiomeAt`, `flowVectorAt`, `caveAt`, `oreAt`, `idx`, `setLocal`, `treeCandidate`, `addOak`, `addPine`, `addCactus`, `addFallenLog`, `addStandingStones`, `addRuin`, `addEchoCairn`, `addLandmarks`, `generateChunk`, `getBase`, `configure`, `clear`, `dropFar`, `findSpawn`, `info`, `landmarkNear`.

### Script 2 — player collision/movement physics
Native destination: `game/player` + `engine/physics`.

`approach`, `overlaps`, `bounds`, `eachSolidInBox`, `intersects`, `clipAxis`, `inFluid`, `canStand`, `isSupported`, `moveHorizontalWithStep`, `update`.

### Script 3 — procedural audio
Native destination: `engine/audio`.

`ensure`, `setVolume`, `resume`, `tone`, `noise`, `play`, `updateEnvironment`.

### Script 4 — 3D runtime / WebGL renderer / gameplay / persistence
Native destination: split across renderer, world, gameplay, diagnostics and persistence.

`WWCreateGame`, `createWebGLContext`, `onCreationError`, `freshWorld`, `freshPlayer`, `storageGet`, `storageSet`, `safeModeId`, `slotKey`, `legacySlotKey`, `backupKey`, `setLastMode`, `getSaveIndex`, `hasModeSave`, `loadSettings`, `saveSettings`, `clamp`, `floorDiv`, `mod`, `key3`, `ckey`, `isSolid`, `isFluid`, `isOpaque`, `isWaterLike`, `waterLevelAt`, `terrainAt`, `getBase`, `getBlock`, `markDirty`, `setBlock`, `queueFluid`, `queueFluidAround`, `setFluidCell`, `clearFluidCell`, `processFluids`, `waterFlowVector`, `waterSurfaceHeight`, `shader`, `linkProgram`, `program`, `dot`, `persp`, `look`, `mul`, `dir`, `faceMaterial`, `pushFace`, `pushWaterFace`, `pushPlant`, `blockShadeBucket`, `emitQuad`, `emitGreedy`, `greedyMasksToMesh`, `buildChunk`, `ensureChunks`, `resetMeshes`, `raycast`, `reach`, `bodyIntersectsBlock`, `gain`, `recalcRank`, `xpNeeded`, `gainXP`, `normalizeHotbar`, `hotbarId`, `autoSlot`, `inventoryCount`, `addInventory`, `spendInventory`, `assignHotbar`, `spawnDrop`, `updateDrops`, `drawDrops`, `activeAnchorCapacity`, `activeAnchors`, `breakBlockAt`, `toolSpeedFor`, `updateMining`, `pulseMine`, `placeSelected`, `builderAllowed`, `toggleBuilder`, `cycleBuilder`, `updateBuildHud`, `shapeCells`, `builderLimit`, `builderClick`, `undoBuild`, `redoBuild`, `structureAllowed`, `toggleStructure`, `updateStructureHud`, `structureClick`, `captureStructure`, `transformedBlueprint`, `rotateStructure`, `mirrorStructure`, `clearStructure`, `pasteStructure`, `eyeHeight`, `physicsStep`, `respawn`, `safeSpawn`, `timeState`, `weatherStep`, `fogColor`, `formatTime`, `resize`, `chunkInView`, `rainExposure`, `projectRel`, `resetRainParticle`, `drawWeather`, `draw`, `bindWorldAttribs`, `drawBoxOutline`, `drawOutline`, `rankProgress`, `currentObjective`, `updateHUD`, `renderHotbar`, `setSelected`, `toast`, `flashDamage`, `sound`, `isMenu`, `setPauseHint`, `requestMouseLock`, `pause`, `resume`, `openMenu`, `closeMenu`, `showTab`, `inventoryBlockList`, `renderMenu`, `recipeCard`, `toggleSetting`, `settingRow`, `bindInventoryUI`, `bindSettings`, `bindWorldButtons`, `buyUpgrade`, `hasCost`, `spendCost`, `craft`, `runSelfTest`, `t`, `diagnosticsSnapshot`, `packModified`, `unpackModified`, `packFluidLevels`, `unpackFluidLevels`, `packDrops`, `unpackDrops`, `packToolState`, `unpackToolState`, `saveObject`, `saveLocal`, `migrate`, `recountAnchors`, `sanitizeWorldData`, `n`, `sanitizePlayerData`, `finite`, `validChunkKey`, `loadObject`, `applyOffline`, `exportWorld`, `restoreBackup`, `newWorld`, `hashSeed`, `initSave`, `applyModeConfig`, `launchMode`, `returnToHub`, `startGame`, `updateStartSummary`, `clearKeys`, `tick`.

### Script 5 — launcher / Discover hub / mode catalog
Native destination: `launcher` + `engine/ui`.

`$`, `C`, `slug`, `readArray`, `storageGet`, `storageSet`, `modeForSave`, `gameAPI`, `fallbackSaveIndex`, `saveIndex`, `saved`, `saveMeta`, `isGameActive`, `probeGraphics`, `onErr`, `formatSystemCheck`, `runSystemCheck`, `openSystemCheck`, `closeSystemCheck`, `toggleCompatRenderer`, `showEngineFailure`, `ensureGame`, `formatPlayTime`, `formatDate`, `artPalette`, `h32`, `randGen`, `paintModeArt`, `sun`, `stars`, `water`, `crystals`, `mix`, `hex`, `drawRidge`, `drawTree`, `drawBlockStructure`, `rhole`, `modeCard`, `escapeHtml`, `schedulePaint`, `run`, `queuePaint`, `drainPaint`, `fillRow`, `renderDiscover`, `setHero`, `openDetails`, `closeDetails`, `toggleFavorite`, `rememberRecent`, `showHubError`, `freshModeConfig`, `startNewMode`, `launch`, `renderBrowse`, `titleCase`, `renderChips`, `renderWorlds`, `renderLibrary`, `showView`, `createCustom`, `bindHub`, `initializeHub`.

## Runtime data tables to migrate

### Block/material registry

The legacy build uses numeric IDs. Native RuneForge maps them to stable namespaced IDs while maintaining an import translation table.

| Old ID | Material |
|---:|---|
|0|Air|
|1|Grass|
|2|Dirt|
|3|Stone|
|4|Coal Ore|
|5|Copper Ore|
|6|Iron Ore|
|7|Amberstone|
|8|Sand|
|9|Water|
|10|Oak Log|
|11|Oak Leaves|
|12|Pine Log|
|13|Pine Needles|
|14|Pale Brick|
|15|World Anchor|
|16|Tall Grass|
|17|Wildflower|
|18|Snow|
|19|Deepstone|
|20|Echo Crystal|
|21|Cactus|
|22|Oak Planks|
|23|Reed|
|24|Glowcap|
|27|Weave Relic|
|28|Silt|
|29|Gravel|
|30|Kelp|
|31|Seagrass|
|32|Coral|

### Surface biomes

Ocean; Sunlit Coast; Meadow; Oldgrowth Forest; Pine Taiga; Amber Desert; High Alpine; Reed Marsh.

### Underwater biomes

Riverbed; Seagrass Shelf; Kelp Forest; Coral Garden; Cold Shelf; Deep Blue.

### Legacy Scale ranks

- HAND @ 0 — individual block interaction.
- REACH @ 20 — reach/basic shaping.
- PULSE @ 60 — coherent cluster excavation.
- THREAD @ 150 — veins/landmarks/pattern reading.
- ANCHOR @ 350 — persistent anchors.
- WEAVER @ 800 — deliberate world manipulation toolset.
- SHAPE @ 1800 — lines/walls/floors/boxes/spheres.
- STRUCTURE @ 4200 — blueprint scale.
- SETTLEMENT @ 10000 — rooms/roads/jobs/districts.
- LANDSCAPE @ 25000 — terrain/ecology control.
- WORLDWEAVE @ 60000 — world-rule/Epoch shaping.

These thresholds are historical behavior. Native RuneForge uses a cleaner public progression model while preserving useful abilities.

### Legacy permanent upgrades

- `reach` / Longer Gesture — +1.5 m interaction reach per level; base 18; max 5.
- `mine` / Material Rhythm — +14% hand mining speed; base 26; max 5.
- `pulse` / Pulse Control — unlock/widen Pulse; base 48; max 3.
- `lungs` / Deep Breath — water movement/stamina recovery; base 55; max 4.
- `anchorYield` / Anchor Network — Anchor capacity/passive yield; base 95; max 5.
- `buildCap` / Weave Capacity — shape-operation cap; base 160; max 5.

### Runtime settings

`sensitivity=.0022`, `fov=74`, `renderDistance=4`, `resolution=1.35`, `autoStep=false`, `headBob=true`, `volume=.30`, `cycleMinutes=20`, `hudScale=1`, `weatherDensity=1`, `waterMotion=true`, `fallDamage=true`, `pickupRadius=1.65`.

## Legacy UI surface inventory

117 IDs were present. The native RmlUi documents replace them while preserving required behavior.

`gl`, `damageFlash`, `underwater`, `weatherCanvas`, `weatherFx`, `vignette`, `hud`, `topHud`, `brand`, `worldChip`, `biomeName`, `worldState`, `stats`, `compass`, `compassText`, `crosshair`, `target`, `toast`, `statusHud`, `healthFill`, `healthText`, `staminaFill`, `staminaText`, `xpFill`, `xpText`, `mediumState`, `objectiveCard`, `rankCard`, `buildHud`, `structureHud`, `hotbar`, `controls`, `debug`, `pause`, `pauseWorldName`, `resumeBtn`, `pauseInventoryBtn`, `pauseMenuBtn`, `returnHubBtn`, `pauseHint`, `menu`, `closeMenu`, `tab-evolution`, `tab-inventory`, `tab-settings`, `tab-world`, `tab-help`, `start`, `hubSearchInput`, `hubBootError`, `hubMain`, `hubView-discover`, `hubHero`, `heroCanvas`, `heroEyebrow`, `heroTitle`, `heroDesc`, `heroTags`, `playBtn`, `heroPlayLabel`, `heroNewBtn`, `heroInfoBtn`, `heroModeType`, `heroSaveState`, `continueSection`, `continueRow`, `featuredRow`, `survivalRow`, `buildRow`, `experimentalRow`, `hubView-browse`, `browseBack`, `browseEyebrow`, `browseTitle`, `browseCount`, `categoryChips`, `modeGrid`, `hubView-worlds`, `worldsGrid`, `hubView-library`, `libraryGrid`, `hubView-create`, `createWorldBtn`, `labBtn`, `hubImportBtn`, `systemCheckBtn`, `hubModeCount`, `modeDetails`, `detailsCanvas`, `detailsEyebrow`, `detailsTitle`, `detailsDesc`, `detailsTags`, `detailsStats`, `detailsPlayBtn`, `detailsNewBtn`, `detailsFavoriteBtn`, `systemCheck`, `systemCheckStatus`, `systemCheckDetails`, `runSystemCheckBtn`, `safeRendererBtn`, `loading`, `loadingText`, `importInput`, `engineCrash`, `engineCrashText`, `engineCrashDetails`, `engineCrashBack`, `exportBtn`, `importBtn`, `backupBtn`, `selfTestBtn`, `labToggle`, `newBtn`, `selfTestResult`.

## Behavioral systems proven by the prototype

- deterministic seeded terrain and V1/V2 compatibility;
- chunk generation/cache and greedy opaque meshing;
- first-person movement/collision/crouch/sprint/jump/water/fall damage;
- raycast/timed mining/material hardness/tools/physical drops;
- inventory/nine-slot hotbar/crafting;
- Resonance/XP/skill progression;
- Pulse Mine;
- Shape line/wall/floor/box/sphere/cylinder;
- transactional undo/redo with resource refunds;
- Structure capture/rotate/mirror/paste;
- local dynamic water and flow vectors;
- day/night/rain/storm/lightning;
- procedural interaction audio;
- HUD/pause/menu/settings;
- autosave/backup/export/import/migration;
- engine self-test/diagnostics;
- Discover/My Worlds/Library/Create, favorites, recents, search/system check/details.

## What must not be ported literally

- `localStorage` persistence;
- WebGL context/browser pointer-lock handling;
- canvas-generated launcher art;
- direct DOM mutation from gameplay;
- global mutable singleton state across one script;
- numeric-only persistent IDs;
- mixed rendering/gameplay/persistence/UI/launcher compilation unit;
- synthesized browser audio as production audio.

## Conversion rule

For every legacy function above: **identify behavior -> write acceptance test -> implement in owning native subsystem -> mark coverage**. No line-by-line JavaScript-to-C++ transcription.

`MIGRATION_COVERAGE_MATRIX.md` tracks one row per identified legacy function/helper.
