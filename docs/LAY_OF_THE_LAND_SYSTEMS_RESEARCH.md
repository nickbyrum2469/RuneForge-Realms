# Lay of the Land systems research -> RuneForge Realms

This document is competitive/technical research, not a request to copy another game's assets, code, content, names, or exact implementation. The goal is to identify proven design patterns that fit RuneForge Realms and implement original systems in our native C++/Vulkan architecture.

## Why this reference matters

Lay of the Land is valuable to study because it treats the voxel world as a simulation substrate rather than a static block grid. Public developer material describes a custom C++ voxel system, GPU-compute accelerated voxel data generation, physically simulated/destructible environments, flowing water, collapsing sand, fire propagation, procedural terrain/location generation, procedural building tools, seamless LOD transitions, and world-space physical crafting/interaction.

RuneForge should target the same *class* of systemic depth while retaining its own visual identity, progression, lore, combat, rendering stack, and user experience.

## Research takeaways and RuneForge mapping

### 1. One simulation substrate, many materials

Observed idea:
- Water flows.
- Sand collapses and can bury/damage actors.
- Fire propagates with material-specific burn behavior.
- Loose voxel structures can become physically simulated objects.

RuneForge direction:
- Add `world/simulation/` as a shared scheduler rather than making separate ad-hoc per-frame loops.
- Active simulation cells/chunks use dirty queues and budgets.
- Simulation is deterministic from world state plus scheduled ticks where practical.
- Materials expose simulation traits: fluid, granular, combustible, structural, buoyant, heat capacity, ignition threshold, erosion resistance.
- Only active regions near players or relevant events receive high-frequency simulation.
- Distant regions use low-frequency analytical/coarse updates or sleep completely.

Planned architecture:
```
src/world/simulation/
    SimulationScheduler.*
    ActiveRegionSet.*
    SimulationBudget.*
    CellChangeQueue.*
    MaterialSimulationTraits.*

src/world/fluid/
    FluidField.*
    FluidSolver.*
    FluidSurfaceBuilder.*

src/world/granular/
    GranularSolver.*

src/world/fire/
    FireSystem.*
    HeatField.*

src/world/structure/
    StructuralConnectivity.*
    DynamicVoxelBody.*
```

### 2. GPU compute is a later accelerator, not the initial source of truth

Observed idea:
- Public dev material states GPU compute was used to accelerate voxel data generation, improving mesh generation, voxel physics, fire and water simulation.

RuneForge direction:
- First establish portable CPU reference implementations with tests.
- Profile them.
- Move embarrassingly parallel work to Vulkan compute when the CPU versions are correct:
  - density/material field evaluation
  - water surface velocity/normal field
  - erosion masks
  - vegetation placement candidate generation
  - visibility/compaction where useful
  - particle simulation
  - selected simulation kernels
- Never require GPU readback every frame for gameplay truth. GPU simulation outputs should be consumed asynchronously or mirrored through compact state changes.

### 3. Hydrology should shape geography before runtime fluid visuals

Observed idea:
- Lay of the Land describes layered procedural world simulation where water carves channels through valleys and roads connect locations organically.

RuneForge direction:
- Separate macro hydrology generation from runtime local fluid simulation.
- World generation stages should become:
  1. continental/regional elevation field
  2. ridge/valley shaping
  3. rainfall/moisture field
  4. downhill flow accumulation
  5. river graph extraction
  6. lake basin detection/filling
  7. erosion/channel carving
  8. biome assignment using elevation + moisture + temperature
  9. POI placement
  10. road graph generation between compatible POIs
  11. terrain-aware road routing
  12. bridge/ford placement at water crossings
  13. local detail/vegetation/decor pass
- Runtime water handles edits, dams, spills, waterfalls, local flow, player interaction and destructible terrain changes.

### 4. RuneForge water should use a hybrid field, not thousands of independent rigid particles

The visual goal is fluid, granular-looking water, but thousands of fully independent physics particles would be expensive and unstable for an open world.

RuneForge plan:
- Authoritative fluid state: sparse per-cell volume + horizontal velocity + source/sink flags.
- Solver: gravity/downward fill first, then lateral equalization/pressure transfer, bounded iterations per active region.
- Surface rendering: generate a continuous top surface from fluid levels with smoothed height transitions.
- Visual micro-particles: GPU particles for spray, droplets, waterfall mist, shoreline splash and turbulent flow. These sell 'thousands of particles' visually without making every droplet authoritative.
- Flow vectors feed shader normal advection, foam placement, surface distortion and sound intensity.
- Distance tiers:
  - near: active fluid solve + animated surface + spray
  - mid: cached/coarse flow field + animated shader
  - far: static water body proxy/LOD

### 5. Structural connectivity creates emergent destruction

Observed idea:
- The game emphasizes collapsing cave roofs, falling trees and loose voxel structures.
- Community documentation describes anchored vs unanchored voxel groups.

RuneForge direction:
- Add structural connectivity graphs for supported voxel groups.
- Terrain and player buildings begin anchored.
- Destruction invalidates local support connectivity.
- Small disconnected islands can become dynamic voxel bodies.
- Large disconnected regions are split/approximated under a strict physics budget.
- Trees become a special case: trunk connectivity determines felling, then the tree converts to a dynamic body with impact damage.
- This should be introduced after basic mining/building is stable, not before.

### 6. Physical interaction beats menu-only interaction when it improves comprehension

Observed idea:
- Lay of the Land moved campfires/anvils and crafting interactions toward physical world interactions rather than generic UI.

RuneForge direction:
- Keep inventory/crafting UI for discoverability and accessibility.
- Add context-sensitive physical interactions where they create tactile gameplay:
  - hit an anvil with a hammer to work/repair metal
  - place food on/in a cooking station
  - pour molten metal into molds
  - physically feed fuel into some stations
  - use tools directly on terrain/objects
- Crosshair/context UI should describe *what the world interaction will do* rather than opening a modal by default.
- Avoid needless item-placement friction; physicality should reduce abstraction, not create busywork.

### 7. Building must graduate beyond block-by-block placement

Observed idea:
- Procedural cylinder/cone tools, terrain sculpting, click-drag building, paths, prefab/blueprint capture and transform gizmos.

RuneForge direction:
- Preserve precise block placement but add construction tools in progression tiers:
  - line/wall drag
  - floor/rectangle fill
  - cylinder/ring
  - arch
  - roof slope/cone
  - stairs
  - road/path brush
  - raise/lower/smooth terrain
  - material replace/paint
- Construction preview must be non-destructive until confirmed and show resource cost.
- Blueprint system captures voxel + prefab + metadata volumes with rotation/mirroring/alignment.
- Later settlements can use the same blueprint format for NPC construction and procedural ruins.

### 8. Seamless LOD transitions are a requirement for the visual target

Observed idea:
- Lay of the Land explicitly added smooth LOD transitions to remove traversal pop-in.

RuneForge direction:
- Current hero/standard/distant detail tiers become the foundation of a proper continuous transition system.
- Terrain chunk LOD should support:
  - distance + screen-space error selection
  - hysteresis to prevent flicker
  - transition skirts/stitching or morph regions between adjacent LODs
  - asynchronous mesh generation
  - old mesh remains valid until replacement is ready
  - foliage density fades rather than pops
  - shadow/material complexity independently scale by distance
- Water surface LOD must follow the same philosophy.

### 9. World generation should produce networks, not isolated random features

Observed idea:
- Roads connect locations, bridges occur at crossings, water follows valleys, biomes contain characteristic caves/locations.

RuneForge direction:
- Treat rivers, roads, trails and settlement connections as network features.
- POIs are placed first based on biome/elevation/resource constraints.
- A regional graph connects important POIs.
- Route cost includes slope, water, cliff, biome resistance and scenic/defensive preferences.
- Roads deform/blend terrain locally rather than floating on top.
- Water crossings choose ford/bridge/ferry candidates based on width, depth and route importance.

### 10. Biomes need gameplay identity below and above ground

Observed idea:
- Distinct cave variants tied to surface biomes, with unique vegetation, water, sand and crystal features.

RuneForge direction:
- Each biome package should define:
  - macro terrain signature
  - ground materials
  - tree families
  - vegetation palette
  - water behavior/color
  - weather probabilities
  - ambient audio
  - surface POIs
  - cave style
  - ores/resources
  - wildlife/enemy ecology
  - rare events
- Cave generation should inherit regional geology but have its own sub-biome state.

### 11. Emergent ecology is more valuable than static spawn tables

Observed idea:
- Recent updates include species attacking other species and summoned AI targeting relevant enemies.

RuneForge direction:
- Introduce faction/ecology relationships instead of only `enemy -> player` behavior.
- Wildlife can flee, hunt, defend territory, scavenge and react to fire/water/weather.
- AI queries dynamic voxel obstacles through a navigation abstraction that can be updated locally after destruction.
- Do not rebuild the whole nav world after each edited block.

### 12. Performance has to be visible in the feature design

Observed lessons from public patches:
- world creation optimization
- reduced loading stutter
- voxel simulation/render update improvements
- CPU/GPU memory reduction
- physics-query optimization
- shadow performance work
- UI optimization
- AI navigation around physics objects

RuneForge policy:
- Every large simulation system gets a budget, profiler counters and sleep rules from day one.
- No feature may assume the entire world is active.
- Prefer chunk/local invalidation over global rebuilds.
- Use structure-of-arrays data for large homogeneous simulation sets when profiling supports it.
- Cache immutable generation inputs.
- Separate authoritative state from render-only detail.
- Expose performance-impact labels in graphics settings once the setting actually exists.

## Features we should intentionally improve beyond the reference

1. **Hybrid water**: macro hydrology + local editable fluid volumes + GPU visual particles, rather than relying on a single simulation representation.
2. **Physical mining contact**: visible arm/tool collision sweep controls damage instead of a pure click ray.
3. **Damage persistence**: per-block/micro-voxel structural state remains in world data and saves.
4. **RuneForge material pipeline**: strong stylized PBR identity with material-specific microdetail, damage, particles and audio.
5. **System interoperability**: water extinguishes fire, erodes/softens selected materials, moves light debris; fire changes vegetation/material state; falling structures damage actors and terrain.
6. **Simulation debugging**: developer overlays for active cells, flow vectors, support islands, tick budgets, LOD tiers and dirty queues.
7. **Deterministic tests**: CPU reference solvers for fluids, fire, granular materials and support connectivity before GPU acceleration.

## Roadmap insertion

### 0.5.x - Foundation and feel
- physical fist/tool swing contact
- first-person body presence
- persistent damage/mining cadence
- material/foliage/water visual foundation
- functioning inventory/pause/settings/hotbar
- simulation scheduler skeleton + profiler counters

### 0.6.x - Hydrology and world networks
- macro river/lake/coast graph generation
- active-region local fluid solver
- continuous water surfaces, flow vectors and spray particles
- road/POI regional graph foundation
- terrain-aware path carving and first bridges
- seamless terrain/foliage LOD transition foundation

### 0.7.x - Physical world simulation
- granular sand/gravel solver
- fire/heat/ash propagation
- structural support connectivity
- falling trees and small dynamic voxel bodies
- local AI navigation invalidation around destruction
- simulation budget/settings/debug tools

### 0.8.x - Construction and physical crafting
- line/wall/floor/cylinder/roof/path construction tools
- terrain sculpting brushes
- blueprint/prefab capture, rotate, mirror, align and resource-cost preview
- physical station interaction layer for forge/anvil/cooking
- improved crafting recipe UX/search/grouping

### 0.9.x - Living world
- biome-specific cave packages
- ecology/faction relationships
- weather + simulation interactions
- farming/compost/fertilizer/growth-space rules
- advanced ambient audio occlusion and world-state-driven sound
- mounts/traversal if core movement and terrain streaming are ready

## Primary public research references

- Steam: https://store.steampowered.com/app/2776090/Lay_of_the_Land/
- Tooley1998 devlog index: https://www.gamesinprogress.com/indie-game-developers/tooley1998
- `Making my Voxel Engine Really Fast` (GPU compute voxel-data acceleration)
- `Simulating Thousands of Voxels in my Voxel Game`
- `Improving my Voxel Game's Physics Engine`
- `Playing with the new Flowing Water in my Voxel Game`
- `Adding Seamless LOD Transitions to my Voxel Game`
- `Improving the Interaction System in my Voxel Game`
- `I've Added Procedural Caves to my Voxel Game`
- Lay of the Land Steam patch notes / SteamDB update history (2026)
