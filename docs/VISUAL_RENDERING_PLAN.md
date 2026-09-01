# RuneForge Realms — visual and rendering plan

## Objective

The supplied reference images establish a new visual bar for RuneForge Realms. The goal is **not** to put a high-resolution photo on a normal cube. The visible quality comes from layered geometry, irregular silhouettes, physically plausible materials, lighting depth, and strong art direction at several spatial scales.

The renderer therefore needs to support a world that is still readable as voxel construction while individual materials can look dramatically richer up close.

## Key decision: macro voxels + generated micro detail

A gameplay block remains a stable world unit. The initial target is a **1 meter macro voxel** because it keeps editing, collision, saves, blueprints, structure recognition and multiplayer operations tractable.

The block is *not* visually limited to one six-faced cube.

Each exposed macro face can produce a deterministic **micro-detail shell** based on material ID, world coordinate, face orientation, biome/environment state, damage/weather state and a stable material-detail seed.

This lets grass have tiny clumps and overhanging turf, stone have chipped plates, and dirt show roots/pebbles without storing thousands of tiny cells in the save.

### Why we should not store all visible detail as literal micro-voxels

A 16×16×16 visual subdivision is 4,096 micro-cells per macro block. A 16×16×16 macro chunk would imply more than sixteen million micro-cells before metadata, lighting, fluids or entities. That is the wrong representation for a persistent editable world.

Instead:

1. **Gameplay/state** stays at macro-block resolution.
2. **Near-camera visuals** generate extra geometry only for exposed faces.
3. **Mid-distance visuals** collapse detail into normal/height/roughness information.
4. **Far-distance visuals** use greedy macro meshes and mipmapped materials.

The player gets reference-image richness while the simulation manipulates sensible data.

---

# 1. Reference breakdown

## Grass / dirt block

The grass reference succeeds because the top is not a flat green tile. It has thousands of height variations, clustered colors, dark gaps, sparse flowers, turf hanging over the side, visible soil transition, embedded stones, roots and strong ambient occlusion.

### Implementation

Near LOD grass gets instanced micro-cube/clump geometry on exposed top faces, density masks, multiple reusable clump meshes, deterministic rotation/scale variation, edge-biased overhang pieces and biome decorations such as flowers, snow or burned variants.

Dirt faces get a macro PBR set, sparse embedded stones, root decals/geometry, dampness near water, appropriate height/parallax detail and crevice contact depth.

## Stone block

The reference reads as a cube assembled from fractured slabs: broken silhouette, multiple plate depths, bevelled/chipped edges, cracks with real shadow depth and controlled roughness/color variation.

### Implementation

Stone uses a **surface tile grammar**:

- material-specific plate templates;
- deterministic subdivision per exposed face;
- micro-displacement within a silhouette budget;
- chipped corner variants;
- real near-camera micro-bevels or bevel normals;
- crack masks shared with AO/roughness;
- geology parameters controlling slab size/fracture direction.

Granite, limestone, deepstone and masonry share the system but not the same grammar.

## Water / crystal block

The blue reference reads as translucent volume because it has refraction, depth color, bright transmitted edges, specular highlights and caustic-like internal streaks.

Water should remain continuous fluid rather than solid glass cubes, but the same language applies to crystal, ice, magical water cells and UI gems.

Water pipeline:

- depth-aware absorption/scattering;
- scene-color refraction;
- Fresnel reflection;
- normal wave layers plus flow direction;
- shoreline foam/wetness;
- underwater fog/extinction;
- approximate shallow-water sun caustics;
- reflection probes/SSR;
- optional ray-query correction later.

Crystal/ice:

- closed-volume transmission;
- IOR and absorption color;
- procedural fracture planes;
- emissive/sparkle masks;
- thickness/edge highlight;
- optional high-tier dispersion.

## Trees

The tree reference avoids “trunk + cube leaves” by using voxel language at a smaller scale to form organic mass.

Tree assets contain trunk/branch graph, modular bark clusters, foliage cluster volumes, roots, sockets for vines/fungi/snow/fruit/lanterns/nests, biome/growth parameters and destruction metadata.

LOD0 uses micro-voxel bark/foliage; LOD1 merges branches and clumps; LOD2 aggressively merges the canopy; far distance uses an impostor or simplified silhouette.

The tree remains cuttable without storing every leaf as an independent world cell.

---

# 2. Material system

Every production material should define:

```text
stable_id
family
tags
base_color_texture
normal_texture
roughness_texture
metallic_texture
ao_texture
height_texture
emissive_texture (optional)
transmission parameters (optional)
subsurface/translucency parameters (optional)
micro_detail_profile
edge_profile
weather_response
sound_profile
particle_profile
physics/material tags
```

Use glTF-style metallic/roughness PBR semantics for mesh assets and the same physical definitions for voxel materials.

Use KTX2/BasisU in shipped builds. Authoring textures can remain lossless; the runtime wants GPU-ready mip chains/compression.

### World-space variation

Do not repeat one obvious texture per block. Use coordinate-hashed variation, triplanar geology, macro maps spanning blocks, per-region hue/roughness parameters, detail normals, material-family arrays and deterministic stochastic texture selection.

Variation must be deterministic so reload does not visually scramble terrain.

---

# 3. Edge treatment

Perfect razor cubes look synthetic even with good textures. The references rely heavily on lit edges.

Three levels:

1. **Near:** real chamfer/micro geometry on exposed boundaries where budget allows.
2. **Mid:** bevel-normal reconstruction / edge-normal treatment.
3. **Far:** normal-map/mip representation only.

Material-dependent: stone chips, wood rounds/splinters, soil crumbles, metal has hard chamfers, crystal has sharp refractive edges. Never bevel hidden internal faces.

---

# 4. Voxel meshing pipeline

```text
Chunk block data
 -> visibility mask
 -> material face classification
 -> greedy macro surface extraction
 -> edge/seam classification
 -> near-LOD micro-detail request generation
 -> vertex/material packing
 -> collision update if needed
 -> GPU upload queue
```

Micro detail lives in separate cached instance/detail buffers so camera LOD changes do not force the base chunk mesh to regenerate.

Generation, base meshing, collision cooking, detail generation and asset decompression run as jobs. The render thread consumes completed immutable upload packets.

---

# 5. LOD strategy

## LOD0 — interaction distance

- full micro geometry;
- real edge silhouette detail;
- highest material maps;
- roots/pebbles/decals;
- contact shadows;
- animated vegetation;
- highest water detail.

## LOD1 — nearby world

- reduced micro instances;
- full macro mesh;
- parallax/height replaces smaller geometry;
- reduced vegetation density.

## LOD2 — middle distance

- greedy macro mesh;
- normal/roughness/AO;
- no tiny geometry;
- simplified trees.

## LOD3 / far terrain

- merged far-region representation or clipmap;
- low-frequency albedo/normal;
- silhouettes preserved;
- atmospheric perspective hides lost micro detail naturally.

Transitions should dither/crossfade rather than pop.

---

# 6. Lighting and depth

Initial production stack:

- linear HDR scene rendering;
- physically based BRDF;
- directional sun/moon;
- cascaded sun shadows;
- local point/spot lights;
- emissive materials + controlled bloom;
- voxel-neighbor AO baked into chunk vertices;
- GTAO/SSAO for dynamic contact depth;
- contact shadows close to camera;
- reflection probes;
- volumetric/height fog;
- physically inspired sky/atmosphere;
- eye adaptation/exposure;
- filmic tone mapping;
- TAA/temporal resolve;
- optional FSR quality modes.

Later only after the base renderer is stable: screen-space/probe GI, radiance/DDGI probes, ray-query reflection/contact correction and hardware RT quality tier.

Path tracing is **not** required to hit this aesthetic.

---

# 7. GPU-driven direction

Architecture should permit persistent GPU scene buffers, indirect draw lists, CPU culling first/GPU culling later, Hi-Z occlusion, instance compaction for grass/rocks/roots, multi-draw indirect, bindless/descriptor-indexed material access, optional mesh shaders and selected async compute.

Optional features always have fallback paths.

---

# 8. Main menu target

The supplied RuneForge menu image is a **style bible**, not a screenshot to hard-code.

The menu renders over a curated live RuneForge scene with fantasy voxel city/valley, moving cloud/fog, warm sunset or mode-specific lighting, water motion, smoke/particles, lantern flicker, foliage wind and very slow cinematic camera breathing/parallax.

The scene is a small curated `.rfrscene`, not a full simulation world. It uses the real renderer/materials/lighting so the menu advertises what the game can actually render.

---

# 9. Player-facing GUI target

Visual grammar from the references:

- charcoal/black hammered or stone panel interiors;
- aged bronze/gold framing;
- bevels/inset shadows;
- blue crystal accent points;
- occasional purple magic/rarity accent;
- warm ivory/gold labels;
- ornate corner caps/header plaques;
- grid cells reading as physical recesses;
- crisp voxel item icons;
- rarity-colored tooltip text;
- strong hover/focus/selected states.

Use RmlUi for native layout/data binding, rendered by RuneForge's Vulkan backend. **No Electron and no Chromium.**

Reusable widget kit:

```text
RfFrame
RfHeaderPlaque
RfGemDivider
RfButton
RfIconButton
RfItemCell
RfItemGrid
RfTooltip
RfScrollbar
RfTabBar
RfStatRow
RfEquipmentSlot
RfCharacterPreview
RfQuantityStepper
RfRecipeCard
RfHotbar
RfModal
RfToast
```

Ornate border art uses 9-slice sprites and modular corners so panels resize without stretching decoration.

### Inventory

- equipment paper doll/live 3D character preview;
- stats;
- armor/weapon/accessory slots;
- pageable/searchable grid;
- deep item tooltips;
- rarity borders;
- drag/drop and controller navigation;
- hotbar consistent with HUD.

### Crafting

- category list;
- recipe search/filter;
- recipe item previews;
- process/crafting grid where appropriate;
- quantity selector;
- exact material availability;
- output comparison tooltip.

### Storage

- split storage/player inventory;
- transfer one/stack/all;
- sort/filter/search;
- named containers;
- later storage-network view after progression unlocks it.

---

# 10. Icon and item art pipeline

Item icons should look like miniature high-quality voxel artifacts, not flat programmer icons.

Preferred pipeline: use the actual 3D item model -> render in an automated icon-capture scene -> output transparent atlas layers -> apply rarity/frame masks separately -> pack shipped icon textures to KTX2.

This keeps inventory art consistent with dropped/world items.

---

# 11. Art tooling

Provide tools for material preview, micro-detail profile preview, deterministic seed scrubber, block face/edge visualization, LOD comparison, tree assembly, screenshot tests, UI widget gallery, icon capture, PBR validation and texture-compression preview.

A dedicated `RuneForgeMaterialLab` executable can use the same renderer without loading the game.

---

# 12. Performance budgets and acceptance tests

Track visible macro sections, macro triangles, micro-detail instances, transparent pixels, shadow casters, texture residency, GPU time by pass, CPU generation/mesh jobs, uploads/frame and worst remesh pressure during a large edit.

Every hero material gets a fixed reference scene with LOD0/1/2 screenshots and GPU timing captures.

### First visual pillar scene

Before adding dozens of biomes, prove together:

- grass/dirt matching the supplied depth language;
- fractured stone;
- water;
- one ore/crystal;
- oak + pine trees;
- sunlight/shadows/AO/fog;
- rain/wet surfaces;
- day-to-sunset transition;
- small stone/wood shelter;
- final inventory/hotbar skin.

If this scene is not beautiful and performant, adding hundreds of block IDs is pointless.

---

# 13. Explicitly avoid

- flat texture atlas stretched across perfect cubes as the final look;
- literal stored micro-voxels for every speck of grass;
- full-detail trees at kilometer distances;
- post effects used to hide weak materials;
- path tracing as prerequisite for acceptable lighting;
- UI built from hard-coded pixel coordinates in C++;
- Electron/Chromium for player-facing screens;
- a gorgeous menu background the actual game renderer cannot reproduce.

---

# 14. Definition of success

At arm's length the world reads immediately as a voxel world. Up close, materials have enough **physical structure** that the player wants to inspect them. At long distance, atmospheric composition and silhouettes preserve beauty without rendering microscopic geometry.

Grass cliffs, stone ruins, giant trees, water and the ornate inventory should all belong to the same RuneForge Realms art language.
