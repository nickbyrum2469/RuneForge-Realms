# WORLDWEAVE — Full Game Plan

> Historical design document preserved for RuneForge Realms. `docs/FEATURE_MASTER_PLAN.md` is the current authority.

## 1. One-sentence pitch

**WORLDWEAVE is an incremental voxel sandbox where the reward for mastering the world is gaining the ability to manipulate larger and larger units of it—from one block, to an entire structure, to a settlement, to landscapes, and eventually to the rules of the world itself.**

This is the core differentiator. It is not “Minecraft with more crafting” and it is not “Satisfactory in a voxel world.”

---

# 2. The fantasy

At minute one, you are physically small relative to the task in front of you. You punch, dig, place, climb and improvise. Building a ten-meter wall is work.

Hours later, you can identify that wall as a single object, extend it, recolor it, mirror it, move it or teach a settlement how to reproduce it.

Later still, you can raise a road across a valley, redirect water, restore a forest, lift a whole tower onto a moving platform, or select an exhausted quarry and convert it into a subterranean district.

The player’s power curve is not primarily “damage 8 -> damage 9.” It is:

- **I manipulate blocks.**
- **I manipulate patterns of blocks.**
- **I manipulate recognized structures.**
- **I manipulate systems made of structures.**
- **I manipulate terrain and ecology.**
- **I manipulate world behavior.**

That is the incremental game.

---

# 3. Design pillars

## Pillar A — Progress is a new verb

Every major milestone must unlock something the player could not previously do. Stat improvements are secondary.

Examples:

- trace an ore vein
- mine a connected cluster
- replace a material in a selected region
- place a line/plane/volume
- recognize a room
- blueprint a structure
- rotate/move a structure
- convert structure into a vehicle or platform
- teach NPC builders a blueprint
- establish a road network
- terraform a slope with a brush
- redirect a river
- stabilize a biome
- define a local world rule

## Pillar B — Your old work matters

The game should actively reinterpret things you built earlier.

A crude starter shelter can become:

- a recognized Home once room semantics unlock
- a blueprint once structure capture unlocks
- an inn or workshop once settlers arrive
- a historical site once the town grows around it
- a fast-travel node once Waystones unlock
- a “memory source” that contributes to Epoch progression because it has existed and been used for a long time

The emotional result: the save file develops archaeology.

## Pillar C — Complexity appears only when the player earns the need for it

The first ten minutes should be cleaner than Minecraft, not more intimidating.

No giant tech tree at spawn. No thirty crafting tabs. New layers appear immediately before the old layer would become repetitive.

## Pillar D — The world becomes active

The terrain and settlements should eventually act without the player:

- plants spread within ecological rules
- rivers erode soft material slowly
- paths become worn roads
- settlers use recognized rooms and routes
- abandoned structures weather
- creatures migrate
- towns expand inside player-approved district boundaries

The player creates conditions; the world responds.

## Pillar E — Macro power without loss of authorship

High-level tools need preview, cost, undo and precision. “Powerful” must not mean “sloppy.”

---

# 4. Core loop

## Immediate loop — seconds

Explore -> interact with blocks/materials -> receive resources + Resonance -> build/modify -> encounter a small problem -> solve it.

## Session loop — minutes

Choose a project -> gather/discover -> improve a tool or unlock a shaping ability -> complete project -> the project starts producing utility / attracting use / changing the local chunk.

## World loop — hours

Develop regions -> connect them -> establish settlements -> unlock higher-scale manipulation -> revisit old regions with new abilities -> transform earlier limitations into new infrastructure.

## Epoch loop — long-term incremental layer

Accumulate enough World Memory through exploration, construction, discoveries, settlements and stabilized regions -> advance the world into a new Epoch -> new materials, ecological behavior, vertical layers and world-scale verbs become available without deleting the old world.

Epoch advancement is the WORLDWEAVE replacement for destructive prestige.

---

# 5. Progression: the Seven Scales

## Scale 1 — HAND

Player operates on individual voxels.

Unlocks available:
- basic break/place
- five foundational materials
- simple inventory
- basic shelter recognition
- starter crafting directly from inventory

Purpose: make the physical world understandable.

Target duration before first scale-up: 10–20 minutes for a first-time player.

## Scale 2 — TOOL

Player begins acting on related groups.

New verbs:
- vein trace
- 3x3 / connected-cluster mining
- line placement
- eyedropper material selection
- quick-swap palette
- repair damaged pattern

Important: these abilities consume stamina/tool integrity or Resonance early so they feel earned, then become cheap later.

## Scale 3 — SHAPE

The engine understands geometric intent.

New verbs:
- wall tool
- floor/ceiling plane
- hollow box
- circle/cylinder/sphere guides
- replace material in connected selection
- smart stairs/ramp generation
- scaffold mode
- selection preview + undo stack

This is where building stops being click-spam.

## Scale 4 — STRUCTURE

The engine recognizes collections of blocks as persistent entities while still preserving voxel editability.

New verbs:
- capture blueprint
- name structure
- move/rotate/mirror structure
- material substitution palette
- structural sockets for doors, windows, power, storage
- turn a structure into a mobile assembly if it meets rules
- save blueprints to a personal library

This scale delivers one of the major “why can’t Minecraft just do this?” fantasies: build a house, realize it is three blocks too far left, and **move the damn house**.

## Scale 5 — SETTLEMENT

NPCs understand player-built spaces and networks.

Core systems:
- room semantics: home, kitchen, storage, workshop, tavern, garden, clinic, school, etc.
- roads/path hierarchy
- settlement boundary
- job teaching instead of spreadsheet labor assignment
- blueprint construction using supplied materials
- maintenance/repair behavior
- requested projects and settlement goals
- trade between settlements

Crucial constraint: NPCs do not immediately replace the player. The player first performs or demonstrates an activity; delegation unlocks when the activity has become routine.

## Scale 6 — LANDSCAPE

The player shapes terrain/ecology as a material.

New verbs:
- smooth/terrace terrain
- raise/lower region with material conservation rules
- river channel preview
- controlled floodgate / aquifer tools
- forest succession tools
- soil restoration
- biome boundary stabilization
- large excavation jobs delegated to settlements

This makes terraforming a legitimate endgame craft rather than cheat-mode creative tools.

## Scale 7 — WORLDWEAVE

The player affects rules and connections.

Potential verbs:
- establish portal topology
- change local gravity in bounded regions
- set climate tendencies
- create protected ecological reserves
- define a floating-island region
- bind distant settlements through shared infrastructure rules
- open deep-world / sky-world strata
- author challenge realms generated from the main world’s history

This is not unrestricted console-command god mode. World-scale powers need expensive persistent infrastructure and create new consequences.

---

# 6. Core incremental currencies

The game should use very few currencies and make each one physically meaningful.

## Resonance

Earned by *meaningful interaction with the world*: first discoveries, building, mining, completing recognized structures, exploration, solving local problems.

Spent on permanent manipulation abilities and quality-of-life mastery.

Anti-grind rule: repeatedly breaking/placing the same blocks in a tiny region rapidly decays Resonance yield.

## World Memory

Slow long-term progression produced by established, used places.

A building gets Memory because inhabitants use it, a road gets Memory because traffic crosses it, a mine gets Memory because it supported a settlement, a landmark gets Memory because it was discovered and revisited.

World Memory advances Epochs. It cannot be efficiently farmed by spam-building.

## Materials

Normal physical resources remain the real cost of construction. Macro tools do not create free matter in survival mode; they pull from inventory/storage networks and show a material estimate before execution.

---

# 7. World Anchors

Anchors are the bridge between active voxel play and idle/incremental progression.

An Anchor marks an area the player considers important. It does NOT simply generate coins.

An Anchor tracks:
- chunk activity
- buildings in radius
- paths used
- resource extraction history
- nearby ecology
- settlement population/use
- discoveries

While the player is away, Anchors generate a *small capped amount* of Resonance/Memory based on actual established activity.

Result: offline progress says “your world continued to matter,” not “a magic box printed currency.”

---

# 8. Things people constantly want from Minecraft — converted into progression

## Move a finished build

Unlocked at Structure Scale. Select -> preview bounding volume -> rotate/move -> confirm material/energy cost.

## Copy builds without creative-mode cheating

Blueprint library. Survival copy consumes stored materials and construction labor/time.

## Build walls/floors without thousands of clicks

Shape tools unlock progressively.

## Make villages use player builds intelligently

Room semantics + settlement AI.

## Make roads matter

Path usage generates wear, speed bonuses, trade routing and settlement growth preference.

## Have real seasons/ecology

Biome simulation is chunk-budgeted and event-driven rather than simulating every plant every frame.

## Have bigger doors, moving structures and ships

Structure entities support local voxel coordinates and transform matrices. A structure can detach from terrain into a mobile grid if it satisfies mass/support/engine requirements.

## Dig huge projects without becoming bored

Excavation starts manual, becomes cluster mining, then shape mining, then a delegated settlement project. The exact same concept grows with the player.

## Have a base that grows instead of becoming obsolete

Room functions, Memory, settlement use, blueprint history, district bonuses and travel links continually add value to old places.

## Better inventory without losing survival feel

Auto-sort nearby storage, searchable material network, quick palettes and project reservations unlock after the player has manually managed storage enough to understand why the feature matters.

---

# 9. Building UX

This is a make-or-break area.

## Always-visible affordances
- center target name + range
- selected material
- exact predicted block count for macro action
- material availability
- undo availability
- structure recognition outline

## Selection model

Three modes:
1. Direct — individual interaction
2. Smart — infer connected/material/room target
3. Volume — explicit two-corner selection

## Ghost previews

Every destructive macro action gets a translucent preview with:
- voxels removed
- voxels added
- unsupported sections warning
- water changes warning
- required materials

## Undo

Survival-friendly undo stores operations, not full world snapshots. A short rolling edit journal can reverse accidental macro edits. Undo cannot duplicate resources; it reverses the material transaction too.

---

# 10. World generation

## Generation goals

A good WORLDWEAVE seed should create projects, not only scenery.

Each region needs:
- recognizable silhouette
- resource identity
- traversal problem
- ecological identity
- at least one unusual formation/landmark class
- reason to revisit at a later Scale

## Vertical world

Proposed full-world height should be much larger than the prototype and divided into semantic strata:
- deep mantle / ancient caverns
- underground waterways
- surface
- highlands
- sky layer / floating geology in later Epochs

## Determinism

Base terrain is generated from seed + generator version. Player modifications are deltas.

Generator upgrades need an explicit compatibility policy:
- old explored chunks retain old base generation
- unexplored chunks can use newer generator version if world rules permit
- chunk metadata records generator version

This avoids updates silently rewriting terrain beneath old builds.

---

# 11. Save architecture

The save system is being designed before content because persistent worlds are the product.

## Save principles

- explicit schema version
- explicit engine version
- generator version per generated region/chunk
- player modifications separate from procedural base
- stable string/block GUID registry for production, not fragile numeric IDs alone
- entity records separated from terrain storage
- migration chain with tests
- transaction/journal layer for crash safety
- backups before migration

## Production world layout

```text
WorldName/
  world.json
  player/
  regions/
    r.0.0.wwr
    r.0.1.wwr
  entities/
  blueprints/
  journal/
  backups/
```

Region containers can group many chunks to reduce tiny-file overhead.

## Export/import

Export packages world metadata, chunk deltas, entities, progression and blueprints into a versioned archive. Import validates format signature, schema support, registry mappings, checksums and required mods/content packs, then migrates before play.

The prototype uses readable JSON for diagnosis. Production should switch chunk payloads to compact binary while keeping human-readable metadata.

---

# 12. Engine architecture

## World module
- deterministic terrain generator
- chunk address system
- block registry
- chunk lifecycle state machine
- modification/delta store

## Rendering module
- chunk mesh cache
- face visibility
- greedy meshing
- material atlas / texture arrays
- directional + ambient lighting
- fog
- frustum culling
- distance LOD / far terrain representation

## Simulation module
- fixed-step simulation clock
- active chunk budget
- scheduled events for inactive chunks
- ecology state
- fluids
- weather

## Structure module
- structure recognition
- local-coordinate voxel grids
- transform / move / rotate
- attachment graph
- blueprint serialization

## Settlement module
- navigation hierarchy
- room semantics
- job/task graph
- needs and schedules
- logistics reservation

## Progression module
- Resonance event pipeline
- anti-exploit diminishing returns
- Scale unlocks
- World Memory
- Epoch progression

## Persistence module
- region streaming
- schema migration
- edit journal
- backups
- export/import

---

# 13. Performance plan

A voxel game dies from bad architecture long before it dies from lack of content.

## Chunking

Production target starts with 16x16 horizontal chunks and vertical sections. Section sizing should be benchmarked, not religiously copied from Minecraft.

## Meshing

Prototype: emit only exposed faces.

Production:
1. face culling
2. greedy merge coplanar faces by material/light state
3. build meshes off the render thread
4. mesh only dirty sections and affected neighbors
5. upload completed mesh buffers in bounded batches

## Streaming

Use a camera-centered ring/priority queue:
- high priority: collision + visible chunks
- medium: near simulation
- low: far generation / preload

Never generate twenty chunks synchronously because the player crossed a border.

## Simulation LOD

Full simulation only near active players. Distant settlements/ecology advance through coarse scheduled events. An inactive farm can store `lastSimulatedTime` and evaluate growth when relevant instead of ticking every crop every frame.

## Lighting

Do not start with path-traced ambitions. Use scalable sun/sky, block light propagation, ambient occlusion and optional high settings for shadow cascades/screen effects.

## Fluids

Use bounded cellular updates and sleeping regions. Stable water does not need to tick.

## Large edits

Macro operations calculate asynchronously, preview, commit in chunk-sized transactions and remesh over multiple frames if necessary. A landscape-scale power should never cause a single-frame death spiral.

---

# 14. Visual direction

Original target: **clean stylized physicality** with readable materials, consistent texture scale, atmospheric depth, restrained post processing, weighted animation, elegant holographic macro previews and consistent human scale.

The current RuneForge visual plan raises this target substantially; see `docs/VISUAL_RENDERING_PLAN.md`.

---

# 15. Combat

Combat should exist but not hijack the premise. The interesting question is how world-shaping becomes combat:

- rapidly raise cover
- collapse unstable terrain
- seal a tunnel
- redirect water/fire
- build temporary ramps
- anchor a defensive district
- enemies that burrow or alter blocks

Avoid conventional sword-DPS-ladder dominance.

---

# 16. Co-op

Designed into architecture even if implemented later.

Progression is partly personal, partly world-level:
- personal mastery: controls, recipes, cosmetic/loadout preferences
- world mastery: Epoch, discovered materials, settlement capabilities
- permissions: direct edit / macro edit / structure move / district policy
- operation log so giant edits have attribution and host/admin reversal

The world file must not assume one player record.

---

# 17. Modding

Production registries use stable namespaced IDs such as `core:granite`, `flora:pine_log`, `modname:machine_part`.

Content packs may add blocks/materials, items, recipes, biomes, structures, creatures, progression nodes and settlement room types.

Save files record missing-content placeholders rather than deleting unknown data.

---

# 18. Content pacing

## First 15 minutes
- readable spawn
- gather 3–4 materials
- tiny shelter/project
- first Resonance unlock
- first scale-up ability

## First 2 hours
- two biomes
- cave/resource reason to travel
- starter base becomes semantically recognized
- first Anchor
- first project benefiting from cluster/shape tools

## 5–15 hours
- Structure Scale
- blueprint library
- first movable structure experiment
- settlement seed
- NPC uses something player built

## 20–50 hours
- multiple settlements/regions
- roads/logistics
- landscape tools
- major ecological project
- first Epoch

## 50+ hours
- structure vehicles
- large terrain transformations
- world-scale projects
- challenge realms / deep strata / sky systems
- historical world becomes primary reward

---

# 19. Anti-grind rules

1. Resonance from repetitive identical edits decays locally.
2. Major unlocks require diverse accomplishments, not only currency.
3. Macro tools remove repetition once repetition is understood.
4. Rare resources gate decisions, not arbitrary hours.
5. Offline gains are capped and tied to world activity.
6. No premium-style timers, energy bars or manipulative daily-streak dependence.

---

# 20. Prototype scope

The browser proof intentionally tested deterministic terrain, chunk rendering, movement/collision, block editing, Resonance, Scale milestones, Pulse Mine, Anchors, permanent upgrades, offline progress hook, autosave and versioned export/import.

It never represented the entire design as completed.

---

# 21. Historical development roadmap

- Milestone 0 — browser engine proof: feel/persistence.
- Milestone 1 — serious voxel core: native renderer decision, greedy meshing, threaded generation/meshing, registry, textures, region files, migration tests, undo journal.
- Milestone 2 — Shape Scale: shape tools, previews, reservations, inventory/storage, history.
- Milestone 3 — Structure Scale: recognition, blueprints, transforms, local grids, movable assemblies.
- Milestone 4 — Settlement Scale: navigation, rooms, autonomous settlers, demonstrated/delegated jobs, roads.
- Milestone 5 — Landscape/Epoch: ecology, water strategy, regional operations, World Memory, first Epoch.
- Milestone 6 — multiplayer/modding/content: authoritative server, permissions, content packs, larger world library.

The current native roadmap supersedes this schedule while preserving the feature order.

---

# 22. Testing metrics

Engine:
- chunk generation ms
- mesh generation ms
- draw calls
- triangles
- RAM per loaded chunk
- save size per 10k edits
- migration success
- hitch time during large edits

Design:
- time to first meaningful unlock
- repeated identical actions before macro unlock
- percentage of old base still used after 10 hours
- number of named/recognized player structures
- distance traveled vs meaningful discoveries
- whether players understand why Resonance was earned
- whether players voluntarily revisit old regions

Signature qualitative test:

> “Did the player repeatedly experience a moment where something that used to be tedious became a powerful new creative verb?”

If yes, the design is doing something genuinely different.

---

# 0.7 implementation checkpoint

By 0.7 the browser engine had progressed beyond the initial proof in greedy meshing, stronger WorldGen V2, rivers, procedural audio, robust collision, Shape transactions and early Structure capture/rotate/mirror/paste. Structure semantics, settlements, landscape operations and world-rule systems remained future work and must not be represented as completed.
