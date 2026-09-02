# Frontier Realms — production roadmap after 0.5

This document turns the master feature plan into an implementation sequence for the next major native milestones. `FEATURE_MASTER_PLAN.md` remains feature authority; this file answers **what we build next, in what dependency order, and what must be true before each milestone ships**.

The goal is not to chase a version number. Each milestone must make the flagship Frontier loop materially better while preserving the modular engine boundaries established through 0.5.

---

## Architecture rules that survive every milestone

1. **No monolithic game file.** New systems get their own subsystem and data model.
2. **No per-block ECS.** Dense voxel/chunk data remains the base world representation; sparse entities exist only when behavior/state justifies them.
3. **Rendering does not own gameplay rules.** Renderers consume snapshots/models and draw them.
4. **Simulation is active-region based.** Stable terrain/water/structures/ecology sleep; changed areas are scheduled under explicit work budgets.
5. **Macro and micro representations coexist.** Far/ordinary terrain remains cheap; hero interaction areas can promote to finer physical detail without requiring the entire world to run at that resolution.
6. **Persistence stores causes and meaningful state, not giant regenerated snapshots where deterministic reconstruction is cheaper.**
7. **Every expensive system has a budget, visibility/range policy and profiling counters before content scales up.**
8. **Player-facing polish and engine architecture advance together.** A technically impressive subsystem that does not improve the core loop is not sufficient to ship a milestone.

---

# 0.5.x — stabilize the new physical foundation

0.5.0 establishes physical swing/contact mining, persistent damage, micro carving, streamed chunks, dedicated terrain/character/water/HUD rendering paths, sparse flowing water, semantic audio and the first visible first-person body.

Use 0.5.x only for problems discovered on real hardware:

- crash/device-loss/shader failures;
- bad mouse/contact alignment;
- mining cadence/reach bugs;
- water scheduler runaway or boundary issues;
- save corruption/regression;
- first-person clipping severe enough to block play;
- audio-device/runtime problems;
- serious streaming/remesh stalls.

Do **not** turn 0.5.x into another feature branch.

Exit gate: the shipped 0.5 build can be played, saved, reloaded, resized and updated repeatedly on the target Windows machine without a blocker.

---

# 0.6 — Survival Slice + authored visual identity

## Product goal

Turn the physical editable world into a real early-game survival loop while making the world stop reading as development/procedural placeholder art.

The acceptance loop is:

**spawn → physically gather wood/stone → drops enter inventory → craft/equip a starter tool → tool changes physical mining behavior → gather faster/stronger material → build a small shelter → save/quit → continue with world + inventory + equipment intact.**

## 0.6A — item/tool/equipment architecture

Create clean subsystems rather than expanding `PlayerController` or `VulkanRenderer`:

```text
src/game/items/
src/game/tools/
src/game/equipment/
src/game/crafting/
src/game/stations/
src/game/recipes/
```

Required:

- stable item definitions separate from block IDs;
- stack rules, categories, rarity only where useful;
- tool class, tier, mining efficiency, reach/contact envelope and durability/maintenance policy;
- hand → crude tool → first specialized tool progression;
- block drop tables;
- equipment slots and equipped-item state;
- crafting recipe registry and deterministic recipe resolution;
- player crafting plus first physical workbench/station;
- persistence tests for inventory, equipment, recipes-in-progress if any, and durability/state.

Physical interaction law: tools alter the same swing/contact system rather than replacing it with invisible ray damage. A pickaxe gets a different pose, contact volume, timing, impact strength and sound/particle response.

## 0.6B — real inventory and crafting presentation

The native UI should move toward the supplied RuneForge visual language:

- grid inventory rather than a debug list;
- 9-slot hotbar with item icons/counts and equipped tool state;
- drag/drop, stack merge/split and quick transfer foundations;
- item tooltip panel with material/tool information;
- crafting panel with ingredient ownership/reservation feedback;
- equipment/paper-doll data model even if the first 3D preview remains simple;
- controller-navigation model prepared even if mouse/keyboard ships first.

UI data models stay separate from Direct2D/Vulkan painters.

## 0.6C — authored material pipeline

0.5 proves procedural material identities. 0.6 begins the production asset path needed to approach the high-quality block references.

Add a material-resource layer capable of authored channels such as:

- base color/albedo;
- normal/detail normal;
- roughness;
- height/relief or parallax-driving data where appropriate;
- ambient-occlusion/cavity information;
- alpha/mask for foliage;
- emissive where justified.

Use texture arrays/atlases or another batching-friendly representation; do not create one descriptor/material object per block face.

The visual target remains voxel-first: geometry is clearly made from blocks/pixels/squares, but surfaces have clean high-frequency detail, readable face identity and physically coherent response to light.

Priority authored materials:

1. grass top + thin rooted side transition;
2. dirt/soil distinct from bark;
3. layered/fractured stone and cobblestone family;
4. bark + cut wood/end grain;
5. leaves/foliage clusters;
6. water surface/edge/foam support textures;
7. character skin/cloth/leather/steel.

Procedural functions remain useful as macro variation, masks and anti-tiling—not as a substitute for the whole asset pipeline.

## 0.6D — lighting and atmosphere pass

- directional sun with a real shadow solution appropriate to streamed voxel terrain;
- stable exposure/tone mapping;
- ambient/sky contribution;
- distance fog/haze tied to world scale;
- improved sky/cloud composition;
- contact/cavity response that reinforces block depth without dirty outlining;
- water receives/refers to the same lighting environment;
- graphics-quality settings expose meaningful cost tiers.

Exit gate: screenshots from ordinary gameplay clearly read as one coherent RuneForge art direction rather than procedural demo materials, and the starter survival loop works without developer commands.

---

# 0.7 — Living terrain, hydrology and exploration world

## Product goal

Make world generation and environmental behavior create places worth exploring instead of merely producing endless height-map terrain.

## 0.7A — hydrology-first macro geography

Terrain generation should reason about water before decorating the surface.

Pipeline direction:

```text
continental/region shape
→ elevation fields
→ drainage / flow accumulation
→ river and basin decisions
→ erosion-inspired valley shaping
→ lake/wetland placement
→ biome/climate fields
→ geology/caves
→ vegetation/ecology
→ landmarks/resources
```

Required outcomes:

- rivers originate from plausible catchments rather than painted random lines;
- tributaries merge instead of crossing arbitrarily;
- lakes occupy basins;
- riverbeds have width/depth/shore structure appropriate to flow scale;
- waterfalls occur where elevation actually drops;
- settlements/roads later have hydrological context to reason about.

Generation does not need offline scientific simulation. It needs deterministic approximations that create believable geography at game scale.

## 0.7B — water 2.0

Extend the 0.5 active-cell solver without converting entire oceans into particles.

- chunk-aware fluid activation boundaries;
- persistent/reconstructable fluid disturbances;
- source/sink policy where world design requires it;
- variable visible surface height from discrete volume levels;
- connected surface meshing that removes the full-cube-water look;
- shore intersection/foam cues;
- waterfalls and falling sheets/columns;
- currents exposed as gameplay vectors;
- swimming, buoyancy and current influence;
- floating/carried item drops;
- splash/ripple events and audio hooks;
- profiling: active cells, queue depth, work used, mesh updates and worst-frame time.

Particles are used for spray, splash and foam accents—not authoritative bulk volume.

## 0.7C — caves, geology and resource identity

- cave networks with entrance logic tied to terrain/geology;
- chambers, shafts, underground water and vertical traversal;
- rock/material strata;
- ore/resource distributions based on geological regions rather than uniform random speckle;
- surface clues that reward exploration;
- early relic/ruin hooks;
- deep-region lighting/atmosphere rules.

## 0.7D — biomes and ecology foundation

- temperature/moisture/altitude fields;
- biome boundaries with transitions instead of hard paint lines;
- tree/plant species profiles;
- terrain-aware placement;
- local succession/regrowth using the active simulation scheduler;
- weather hooks and wetness response;
- environment audio layers driven by biome/weather state.

Exit gate: a several-kilometre exploration session produces recognizable valleys, watersheds, caves, resource regions and biome changes with reasons to revisit locations.

---

# 0.8 — Structure physics, destruction and advanced building

## Product goal

Make RuneForge construction/destruction feel fundamentally more physical than ordinary block placement while unlocking the **SHAPE → STRUCTURE** progression promised by the master plan.

## 0.8A — structural graph

Do not run expensive rigidity analysis for every terrain block continuously.

When player-built or disturbed connected material becomes structurally relevant:

- identify structural islands/components;
- classify support/anchor contacts;
- propagate approximate load/support values under a fixed work budget;
- put stable structures to sleep;
- wake only affected components when support/material changes;
- provide deterministic collapse decisions;
- convert collapsing fragments to bounded dynamic bodies/fracture groups rather than one rigid body per voxel.

Start with readable game rules, not an impossible full finite-element solver.

## 0.8B — destruction language

- material-specific fracture thresholds;
- progressive structural damage;
- support removal and delayed collapse cues;
- debris groups/chunks;
- dust/chip/impact effects;
- sound events based on material and collapse scale;
- interaction with water and terrain where affordable;
- save result after the event settles rather than persisting thousands of transient debris entities.

## 0.8C — fire, sand and other active materials

Use the same scheduler philosophy as water:

- fire cells wake nearby fuel/heat state;
- spread and burnout under bounded budgets;
- smoke/embers rendered as effects, not world-state particles;
- sand/gravel wakes when support changes and settles under gravity;
- wetness/water can suppress fire;
- collapsed terrain can redirect local water.

No feature ships without a sleeping/stability rule.

## 0.8D — shape building tools

Unlock the first serious Scale 3 verbs:

- line;
- wall;
- floor/plane;
- hollow box;
- stairs/ramp;
- circle/cylinder foundations;
- direct/smart/volume selection;
- live ghost preview;
- exact material estimate;
- transactional apply;
- undo/redo history.

Macro building consumes real survival materials. A preview cannot mutate the world, and a failed transaction cannot leave half a structure.

## 0.8E — structure recognition and blueprint foundation

- detect completed connected builds as candidate structures;
- player can name/accept a structure;
- blueprint capture retains voxel editability and material palette;
- rotate/mirror preview;
- material substitution rules;
- structure metadata remains sparse and does not replace chunk voxel storage.

Exit gate: the player can physically undermine/collapse a supported build, use shape tools to construct a significantly larger project without block-by-block tedium, save/reload it, and capture it as a structure/blueprint without world corruption.

---

# 0.9 — Settlements, roads and the living world

## Product goal

Move from a player-only sandbox into a world where construction creates social/logistical utility.

- room/use semantics;
- settlement anchors/boundaries;
- inhabitant needs and schedules kept legible rather than spreadsheet-heavy;
- teachable jobs/delegation;
- NPC construction from player blueprints using real supplied materials;
- storage reservations/logistics;
- roads/path hierarchy driven by actual destinations and terrain cost;
- trade routes;
- town growth biased toward viable serviced space;
- maintenance/repair;
- World Memory generated from real use of roads, homes, workshops, mines and landmarks;
- region-level simulation LOD so distant settlements do not run full nearby AI.

Hydrology and terrain matter here: bridges, river crossings, valley roads, resource access and defensible terrain should naturally influence settlement shape.

Exit gate: a settlement can survive multiple play sessions, use structures/storage/roads the player actually built, perform delegated work and continue at a cheaper simulation tier when distant.

---

# 1.0 candidate — cohesive Frontier release

1.0 is not “everything in the master plan.” Realm-scale progression can continue after launch. 1.0 means the early-to-mid Frontier experience is coherent, beautiful and technically trustworthy.

Minimum release characteristics:

- distinctive authored voxel art direction;
- polished hero character/equipment presentation;
- satisfying physical gather/tool/build loop;
- meaningful crafting and storage progression;
- recognizable biomes/geology/caves/hydrology;
- water and major environment interactions are stable and performant;
- advanced building/blueprint tools provide a clear RuneForge identity;
- structures can meaningfully fail/change;
- initial settlements/roads/world-memory loop exists;
- persistence/update path has migration/version tests;
- settings, controller/input, audio, accessibility and error handling receive shipping passes;
- real-hardware performance budgets are defined and measured.

Later Scale 5–7 expansion—larger settlement systems, landscape operations and Realmweave—builds on these foundations rather than delaying a coherent first release indefinitely.

---

# Cross-milestone performance program

The world-quality goals are only viable if performance work stays continuous.

Track at minimum:

- CPU frame time by gameplay/world/render preparation subsystem;
- GPU frame time by sky/terrain/character/water/HUD/effects pass;
- loaded/resident/prefetch chunk counts;
- pending mesh jobs and upload bytes/frame;
- visible/cull counts;
- promoted micro-block count;
- active water/simulation cells and queue depth;
- dynamic debris/body count;
- audio voices/events;
- save delta sizes and serialization time;
- memory usage by chunk data, meshes, textures and dynamic simulation.

LOD policy should eventually include:

1. **hero interaction detail** — physical micro detail near edits/contact;
2. **normal voxel chunk mesh** — ordinary gameplay range;
3. **simplified distant terrain** — reduced topology/material frequency;
4. **regional macro representation** — very distant landscape silhouette/atmosphere where needed.

LOD transitions must preserve the voxel silhouette and avoid obvious terrain popping/cracks.

---

# Immediate branch after 0.5 ships

Create a fresh branch such as:

`feat/frontier-survival-visuals-0.6`

Start in this dependency order:

1. item/tool/equipment definitions + portable tests;
2. tool-driven physical swing/contact behavior;
3. crafting/recipe data model + persistence;
4. inventory/crafting UI model;
5. authored material resource/texture binding path;
6. grass/dirt/stone/wood authored material set;
7. shadow/lighting atmosphere foundation;
8. starter workbench + survival acceptance loop;
9. full Windows integration and real-machine visual tuning.

Do not begin 0.7 hydrology or 0.8 structural physics until the 0.6 survival/material acceptance loop is real and CI remains green.
