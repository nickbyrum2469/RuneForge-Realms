# RuneForge Realms — master feature plan

## Product definition

**RuneForge Realms** is the native successor to the WORLDWEAVE prototype: a high-detail voxel survival/RPG sandbox whose defining progression is **scale of agency**.

The player begins by manipulating individual pieces of the physical world and eventually learns to manipulate patterns, shapes, structures, settlements, landscapes and bounded realm rules. The world remains persistent while that power curve unfolds.

This document is the **feature authority** for the native rebuild. Historical WORLDWEAVE concepts remain in `docs/legacy/`, but names, thresholds and implementation details may be redesigned when doing so makes RuneForge better.

---

# 1. Product boundaries

## Must be

- native desktop game, Windows first;
- no Electron/Chromium runtime;
- real `.exe` build and installer/updater path;
- versioned Git/release workflow;
- one flagship survival/RPG experience developed deeply before proliferating shallow modes;
- highly modular codebase;
- persistent procedural world;
- beautiful, tactile voxel materials;
- polished fantasy UI based on the supplied RuneForge reference images;
- playable offline;
- architecture prepared for co-op, modding and additional realms later.

## Must not become

- a browser game wrapped as a desktop app;
- “Minecraft but with more blocks”;
- a conveyor/factory game whose main fantasy is throughput;
- a menu-first incremental game;
- a collection of dozens of recolored fake game modes;
- destructive prestige that wipes the world for multipliers;
- an architecture where every terrain block is an ECS entity;
- a monolithic `Game.cpp` replacing the monolithic HTML file.

---

# 2. Core fantasy and design laws

RuneForge progression changes what counts as one meaningful action:

```text
block
 -> connected cluster/material family
 -> shape/volume
 -> structure
 -> settlement/district
 -> landscape/ecology
 -> realm/world rule
```

The design laws are:

1. Every major progression tier adds a **new verb**. Stats support verbs but never replace them.
2. Old work gains new uses rather than becoming obsolete.
3. Complexity appears shortly before the old interaction would become repetitive.
4. Exploration feeds construction and construction changes exploration.
5. Macro power always has preview, cost, precision and undo/history.
6. Automation is earned delegation: the player demonstrates work before helpers repeat it.
7. Offline progress comes from real established places and activity.
8. The main world accumulates history; optional challenge realms can reset.
9. Epoch advancement transforms the world instead of deleting it.
10. Procedural size only matters if regions have identity, landmarks, resources and reasons to revisit.
11. Performance architecture is gameplay architecture because the endgame depends on large operations.

---

# 3. Flagship survival/RPG loop

## Immediate loop

Explore -> physically gather -> craft/use -> solve a local problem -> build/modify -> earn Resonance/skill growth -> unlock a better way to act on the world.

## Session loop

Choose a project -> explore for materials/knowledge -> improve tools/abilities -> complete the project -> the completed thing gains utility -> use that utility to push into a new region/problem.

## World loop

Develop one home region -> connect outposts -> establish settlements -> build infrastructure -> revisit old places with stronger verbs -> transform terrain/ecology -> advance the Epoch while keeping the world's history.

## Long-term loop

The player should eventually have a world where the oldest hut, roads, abandoned quarries, transformed rivers, city districts and challenge scars all tell the history of the playthrough.

---

# 4. Progression scales

The exact names/thresholds can evolve, but the native design should use seven legible public scales rather than exposing a noisy list of minor ranks.

## Scale 1 — HAND

- individual block break/place;
- starter resources;
- physical item drops;
- simple inventory/hotbar;
- basic crafting;
- first shelter;
- health, stamina and survival fundamentals;
- early exploration and biome discovery.

## Scale 2 — TOOL

- tool tiers and durability/maintenance where it improves decisions;
- connected-vein trace;
- cluster/Pulse mining;
- line placement;
- material eyedropper;
- quick build palette;
- pattern repair;
- stronger traversal tools.

## Scale 3 — SHAPE

- wall;
- floor/ceiling plane;
- line;
- hollow box;
- circle/cylinder;
- sphere;
- smart stair/ramp generation;
- scaffold mode;
- connected material replace;
- direct/smart/volume selection;
- live ghost preview;
- exact block/material estimate;
- transactional undo/redo.

## Scale 4 — STRUCTURE

- recognize a built collection as a structure while retaining voxel editability;
- name structures;
- blueprint capture;
- blueprint library;
- move/rotate/mirror finished builds;
- material substitution palettes;
- structural sockets: doors, windows, storage, functional modules;
- copy construction with real survival material cost;
- movable/local voxel grids;
- eventual ships/platforms/vehicles if structural rules are met.

## Scale 5 — SETTLEMENT

- room semantics: home, kitchen, storage, workshop, tavern, garden, clinic, school, etc.;
- settlement boundaries/districts;
- road/path hierarchy;
- autonomous inhabitants;
- job teaching instead of spreadsheet assignment;
- NPC blueprint construction using supplied materials;
- maintenance/repair;
- requested projects;
- trade routes;
- town growth preference based on roads and viable spaces;
- logistics/storage reservations;
- history and World Memory from actual use.

## Scale 6 — LANDSCAPE

- terrain smoothing/terracing;
- raise/lower region while respecting material rules;
- large excavation plans;
- river-channel preview and redirection;
- floodgate/aquifer infrastructure;
- soil restoration;
- forest succession/replanting tools;
- biome boundary stabilization;
- environmental repair;
- settlement-delegated megaprojects.

## Scale 7 — REALMWEAVE

“WORLDWEAVE” becomes historical terminology; final lore naming can evolve. Capabilities may include:

- portal topology;
- bounded local gravity rules;
- climate tendencies;
- protected ecological reserves;
- floating-island regions;
- deep-world and sky strata;
- distant settlement infrastructure links;
- challenge realms derived from the main world's history/state;
- expensive local rule changes with consequences.

---

# 5. Progression resources

## Physical materials

Construction continues to consume actual matter. Macro tools do not magically create blocks in survival. Later material networks can draw from nearby/linked storage and reserve requirements for projects.

## Resonance

Earned through meaningful interaction: first discoveries, mining/gathering natural resources, building, recognized structure completion, solving local problems, exploration, difficult projects and relics/landmarks.

Anti-grind: repeated identical edits in one small area quickly reduce Resonance yield.

## World Memory

Slow persistent progression generated because a place actually mattered: inhabitants use a home, traffic uses a road, a mine supports a town, a landmark is revisited, a structure survives across time or a restored biome stabilizes.

World Memory drives Epoch progression.

## Skills/mastery

Preserve useful prototype foundations: Athletics, Mining, Building and Exploration. Later combat/crafting/survival specializations are added only if they create decisions instead of number soup.

---

# 6. World Anchors

Anchors mark important areas and bridge active play with long-term simulation.

They can track chunk activity, structures, paths/traffic, extraction history, ecology, settlement use/population, discoveries, repairs/projects and long-term Memory.

Offline gains are small and capped, derived from actual activity. An Anchor is never a magic idle-income box.

Later uses can include settlement seeds, travel/waystone infrastructure, project/logistics coverage, regional simulation authority, historical protection and respawn/home functions.

---

# 7. Player movement and survival

Rebuild the responsive first-person fundamentals:

- WASD + controller movement;
- mouse/controller look;
- sprint;
- crouch;
- jump with coyote/buffer behavior;
- slope/step handling;
- swimming up/down;
- buoyancy/current influence;
- stamina;
- health;
- fall impact/damage;
- safe respawn;
- camera motion controls;
- movement skill growth;
- material-aware footsteps/landing feedback.

Future survival layers—temperature, food/rest/comfort, equipment protection and status effects—must add planning without meter babysitting.

---

# 8. Mining, gathering and physical items

- target/raycast interaction;
- material hardness;
- timed mining;
- tool categories/tier benefits;
- connected-vein and Pulse progression;
- ore/geology identity;
- physical drops with gravity/bounce;
- water carrying/floating drops;
- pickup radius;
- rare materials/relic cores;
- chunk-delta harvested state;
- large excavation as asynchronous transactional work.

Rare resources gate decisions, not arbitrary hours of repetition.

---

# 9. Inventory, equipment and storage

The supplied UI references raise this from a prototype list into a full survival/RPG inventory system.

## Inventory

- grid inventory;
- stacks;
- weight/capacity only if worthwhile;
- categories;
- search/filter/sort;
- drag/drop;
- controller navigation;
- quick transfer;
- compare tooltips;
- rarity borders/text;
- stable item IDs;
- hotbar;
- item durability/state;
- recipe/material reservations.

## Equipment

- live 3D character preview/paper doll;
- main/off hand;
- armor;
- accessories/trinkets;
- tool slots where useful;
- stat/resistance summary;
- equipment appearance reflected in world.

Do not add generic RPG attributes merely because the reference mockup contains them; every stat must serve RuneForge gameplay.

## Storage

- physical chests/containers;
- split-pane player/container UI;
- transfer one/stack/all;
- sorting/filtering;
- named containers;
- upgrades;
- nearby-storage auto-sort as later unlock;
- searchable storage network as later unlock;
- project reservations;
- settlement logistics integration.

Early survival remains physical; late progression removes storage chores.

---

# 10. Crafting

Baseline:

- starter inventory crafting;
- workbenches/stations;
- categories/search;
- exact requirements/current count;
- quantity control;
- output preview;
- item/tool stats;
- feedback;
- mastery where meaningful.

Where it improves the fantasy, some material processing becomes spatial/tactile—furnaces, forging or other placed processes—rather than being only menu timers. Settlers can eventually repeat routine proven crafting.

No mobile-game timers.

---

# 11. Building UX

Show target name/range, selected material, macro block count, availability/requirements, support warnings, water consequences, undo state and structure outline.

Selection modes:

1. Direct — one target.
2. Smart — connected material/pattern/room inference.
3. Volume — explicit bounds.

Every destructive macro operation gets a translucent ghost preview. Undo reverses the material transaction too.

---

# 12. Structures and mobile assemblies

Features:

- capture;
- name;
- recognition tags;
- blueprint serialization/library;
- rotation/mirroring;
- move preview;
- material substitution;
- attachment sockets;
- support/mass analysis where necessary;
- history/version metadata;
- library thumbnails;
- later sharing/export.

Mobile assembly path: detach a valid structure, preserve local voxel coordinates, transform as one entity, generate chunk/aggregate physics representation, attach modules/engines/controls, and eventually support boats/airships/platforms/vehicles appropriate to progression.

---

# 13. Settlements and autonomous life

Rooms are detected from valid spaces/furnishings, not only prefabs.

NPCs/creatures use identity-based entity simulation with needs/schedules at a sensible abstraction, navigation, tasks, relationships/roles, logistics reservations and home/work associations.

Earned delegation examples:

- demonstrate mining route -> delegate routine mining;
- establish/use roads -> delegate maintenance;
- approve blueprint -> settlement reproduces it;
- establish farms -> delegate harvest;
- repeatedly repair damage -> teach maintenance.

Automate chores, not the game.

---

# 14. Combat

Combat exists without hijacking the world premise.

Base requirements: responsive melee/ranged interaction, readable hit feedback, armor/equipment, biome/ruin enemies and stakes for exploration/base design.

RuneForge-specific combat verbs: raise cover, temporary walls/ramps, seal tunnels, collapse unstable material, redirect water/fire, shape defensive terrain, Anchor defenses, and enemies that burrow/climb/modify terrain.

Avoid a progression system dominated by weapon DPS inflation.

---

# 15. World generation

A region should create **projects**, not only scenery.

Each region needs strong silhouette, resource identity, traversal problem/opportunity, ecology identity, landmark class, rare formations and a later-Scale revisit reason.

Production vertical strata should greatly exceed the browser prototype: deep mantle/ancient caverns, underground waterways/caves, surface, highlands and later sky/floating geology.

Determinism/versioning:

- seed + generator version;
- chunk records generator version;
- old explored regions never silently rewrite beneath structures;
- generator upgrades may affect unexplored territory by explicit world rules;
- player edits remain deltas from procedural base.

---

# 16. Biomes, ecology and landmarks

Starting surface identities: ocean, coast/beach, meadow, old-growth forest, pine taiga, desert, alpine and marsh.

Underwater: riverbed, seagrass shelf, kelp forest, coral garden, cold shelf and deep blue.

Ecology can include biome-authored tree species, grass/flowers/reeds, fungi/glowcaps, cacti, kelp/seagrass/coral, spread/succession, creature migration, seasonal behavior, fire/storm damage/recovery and abandoned/weathered structures.

Landmark lineage to preserve/expand: Standing Stones, Broken Thread Ruins, Echo Cairns, Rune/Weave relics, Echo/Rune crystals, caves, monuments, dungeons and settlement ruins.

---

# 17. Water

Water is simulation + traversal + visual system:

- water cells/volumes;
- bounded/sleeping updates;
- sources/local flow;
- rivers/currents;
- swimming/buoyancy;
- moving drops;
- shoreline generation;
- underwater biomes;
- depth fog/absorption;
- animated surface;
- wetness;
- later boats/diving/pressure;
- floods/aquifers;
- player-redirection tools;
- storm waves at higher tiers.

---

# 18. Time, weather and seasons

- continuous day/night;
- calendar/day counter;
- sun/moon/stars;
- configurable cycle length;
- clear/rain/storm baseline;
- wind;
- roof exposure;
- lightning/thunder;
- fog/visibility response;
- wet materials;
- snow/cold;
- desert heat;
- seasons later;
- weather influences ecology/gameplay.

This is RuneForge's own native renderer/system and must not be mixed with the separate OmniForge project.

---

# 19. Epochs

Epoch is long-term prestige without deletion. Advancing can reveal resources, alter ecology, introduce world events, open vertical strata, change climate tendencies, expand enemies/ruins, settlement capability and Realmweave verbs.

It **does not delete homes, roads, structures or history**.

---

# 20. Challenge realms and old mode concepts

Do not present old prototype modes as finished separate games. Reuse the strongest as challenge realms, presets, expedition contracts, difficulty rulesets or future standalone experiences only after the core game is strong.

Concept pool retained: frontier survival, ascension, cave descent/extraction, exploration RPG, floating islands, drowned/ocean frontier, relic hunt, one-life hardcore, giant labyrinth, kingdom seed, mining ascension, nomad, underwater expedition, frost, desert, permanent night, storm, crystal rush, ancient forest, volcanic shores, builder survival, Epoch world, traversal/parkour, underground kingdom, cozy survival, world restoration, roguelite expedition cycle, rift descent, archipelago, World Memory experiment, Creative Infinite and Engine Lab.

---

# 21. Creative and developer modes

## Creative Infinite

Unlimited materials, all Shape/Structure tools, time/weather controls, blueprint access, no survival cost and visual-quality parity.

## Engine Lab

Developer/QA only: diagnostics, material gallery, all content, rendering/chunk/save/macro/physics/UI stress tools and profiling. Never promote Engine Lab as a consumer game.

---

# 22. Launcher and main menu

RuneForge is one main game/product, so simplify the old giant mode-marketplace hub.

## Native bootstrapper

Installed version, update availability, verify/download/apply, rollback, launch, repair/verify, safe-mode graphics and release notes. It never owns gameplay saves.

## Game main menu

Use the supplied RuneForge visual target: live rendered fantasy voxel background, logo, Continue, New Game, Multiplayer only when real, Settings, Quit, Credits, Mods only when real, world/save management and unobtrusive build metadata.

The background “breathes”: cloud/wind/water/lantern/fog movement and slow camera drift.

---

# 23. In-game UI

Ornate native RuneForge UI kit supports HUD, vitals, XP/Scale/Resonance, compass/objective, target/material/range, hotbar, inventory/equipment, crafting/workbench, storage/chests, character stats, progression, map/world info, settlement screens, blueprint library, project reservations, settings, pause, tooltips, notifications and later multiplayer permissions.

All player UI is data-bound to view models. Gameplay never hunts UI elements and mutates them directly.

---

# 24. Audio

Replace browser synthesis with authored/mixed native audio: material footsteps, mining/break/place, pickup, tool/weapon impacts, underwater filtering, rain/wind/storm/thunder, biome/wildlife/settlement ambience, UI feedback, music state system, spatial audio and volume/accessibility buses.

---

# 25. Save/persistence

Persistent worlds are a product feature.

Requirements: format signature, schema version, engine version, generator version per region/chunk, stable namespaced IDs, procedural base separate from deltas, region containers, entity records separate from terrain, progression/player records, blueprints, edit journal, crash recovery, automatic backups, backup before migration, tested migrations, checksums, export/import and missing-mod placeholders.

Suggested layout:

```text
WorldName/
  world.json
  players/
  regions/
  entities/
  blueprints/
  journal/
  backups/
```

Chunk payloads become compact binary data; metadata stays human-readable where useful.

---

# 26. Updater/release system

- GitHub repository is authoritative source;
- GitHub Actions builds Windows releases;
- bootstrapper checks release manifest;
- staged download;
- hash/signature verification;
- never patch live running files;
- install versioned directory;
- atomic active-version switch;
- launch health check;
- rollback on failure;
- saves/config outside version directories;
- optional stable/preview/dev channels.

---

# 27. Multiplayer/co-op architecture

Design now, ship later:

- world does not assume one player;
- personal vs world-level progression;
- authoritative host/server;
- permissions for direct edit/macro edit/structure move/district policy;
- operation log/rollback;
- deterministic IDs;
- network-friendly chunk deltas;
- replication interest regions;
- settlement authority;
- security before public servers.

---

# 28. Modding/content packs

Stable namespaced IDs from day one:

```text
core:granite
flora:pine_log
runeforge:echo_crystal
some_mod:machine_part
```

Content packs may add blocks/materials, items, recipes, biomes, structures, creatures, progression nodes, settlement room types, UI assets, quests/events and challenge rules.

Missing content never silently deletes world data.

---

# 29. Diagnostics/tools/performance

Developer builds expose frame graph, CPU/GPU timings, GPU capabilities, visible chunks, draw/triangle counts, generation/mesh timings, upload queue, memory, dirty/fluid/simulation queues, entity counts, save journal, migration state, job utilization and later network stats.

Dedicated tools: Material Lab, worldgen viewer, save inspector, blueprint viewer, UI gallery, item-icon capture, migration runner and benchmark scenes.

---

# 30. Performance model

- 16×16 horizontal chunks are a starting benchmark, not dogma;
- vertical sections;
- multithreaded generation/meshing;
- only dirty sections/neighbors remesh;
- camera-centered priority streaming;
- collision/visible > simulation > preload;
- no synchronous border-crossing burst;
- distant simulation via coarse events;
- stable fluids sleep;
- large edits calculate async, preview and commit per chunk/transaction;
- renderer supports GPU-driven submission/culling as scale demands;
- micro-detail geometry has aggressive LOD.

---

# 31. Visual target

The supplied references supersede the older simple-block target.

RuneForge uses **high-detail stylized voxel physicality**: macro gameplay voxels, micro-detail on exposed near surfaces, irregular silhouettes, chipped/bevelled stone, layered dirt/roots/pebbles, dense grass, detailed trees, physically plausible water/crystal transmission, PBR response, deep shadow/AO/contact depth, atmospheric distance and restrained cinematic post-processing.

Full technical strategy: `docs/VISUAL_RENDERING_PLAN.md`.

---

# 32. Accessibility and quality of life

Remappable keyboard/mouse/controller, controller UI navigation, FOV/sensitivity, head-bob/motion controls, scalable UI, readable text, color-independent state communication, subtitles, volume buses, hold/toggle options, difficulty/survival customization, camera-shake reduction, safe graphics reset and transparent autosave/manual backup/export.

---

# 33. First vertical slice

Do not start by implementing the 50-hour endgame.

First native vertical slice proves simultaneously:

- native executable;
- Vulkan renderer;
- one beautiful generated biome plus cave/water edges;
- grass/dirt/stone/water/wood/leaves/ore/crystal quality materials;
- one or two tree families;
- movement/collision/swimming;
- mining and placement;
- physical drops;
- inventory/hotbar;
- starter crafting/workbench;
- day/night + rain;
- versioned region save/load;
- RuneForge main menu;
- ornate inventory/crafting UI;
- one early Resonance unlock (cluster/Pulse);
- profiling and automated tests.

Only after this looks and feels like a real game do Shape/Structure/Settlement accelerate.

---

# 34. Historical preservation

Previous WORLDWEAVE source/research is kept under `legacy/` and `docs/legacy/` so ideas are not lost. Historical files are not production dependencies.

When a legacy behavior is replaced, migration coverage is recorded instead of deleting history and hoping we remember it.
