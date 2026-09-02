# RuneForge Realms — Master Development Playbook, Progress Ledger, and Roadmap

> **MANDATORY FOR AI/AGENT WORK:** Read this entire file before editing RuneForge Realms. Do not use the roadmap alone. This file defines how work is planned, where code belongs, what visual/interaction quality is required, how Git/release work must be performed, and what may be called “done.”

This document is the canonical handoff for continuing RuneForge Realms across chats, agents, Codex sessions, and human development sessions. It exists to prevent architectural drift, giant files, false completion claims, visual regressions, and “feature pile” development on top of broken foundations.

---

## 0. Required read order and authority

Before implementing anything, read in this order:

1. **`docs/RUNEFORGE_MASTER_DEVELOPMENT_PLAYBOOK.md`** — process, architecture, quality, current progress, release rules.
2. **`docs/FEATURE_MASTER_PLAN.md`** — product/feature authority. If another document conflicts with the intended product, this wins.
3. **`docs/FRONTIER_REALMS_0_6_PLUS_ROADMAP.md`** — dependency order for major milestones.
4. **`docs/LAY_OF_THE_LAND_SYSTEMS_RESEARCH.md`** — systems/optimization research to adapt, not copy.
5. **`docs/MIGRATION_ROADMAP.md`** when migration/history matters.
6. Relevant subsystem source/tests on the current branch. Never assume an older chat description still matches the repository.
7. If reference images are present in the current conversation, inspect them directly before visual work. Do not work from vague memory of them.

Historical notes and old chat summaries are context only. **Current repository state is source of truth for implementation.**

---

# 1. Product definition in one paragraph

RuneForge Realms is a native C++23/Win32/Vulkan voxel survival/RPG sandbox centered on **Frontier Realms**. Its identity is not “Minecraft with more blocks.” The world should be tactile, beautifully stylized, highly interactive, persistent, simulation-rich, and increasingly manipulable from individual block/micro detail through shapes, structures, settlements, landscapes, and eventually bounded realm-scale rules. The flagship experience is developed deeply before additional realms/modes.

The visual target is a premium fantasy voxel world with strong block/pixel construction, crisp micro-detail, roots/soil/rock/bark depth, dense but porous vegetation, beautiful stylized water, cinematic atmosphere, dramatic geography, clean commercial UI, and an embodied voxel hero character.

---

# 2. Current verified release state

## Latest verified public release

- **Version:** `0.5.0`
- **Tag:** `v0.5.0`
- **Main/release commit:** `047a88841bb86ed632bbdcbb267d2ee144e5a312`
- **Published package:** `RuneForgeRealms-Windows-x64.zip`
- **Published checksum:** `RuneForgeRealms-Windows-x64.sha256`
- Existing `RuneForgeBootstrap.exe` is the update entry point.

0.5.0 has a real native playable foundation, but the user’s real-hardware review shows it is **not yet at the desired presentation or interaction quality**. Do not confuse “released” with “polished.”

## 0.5.0 systems that are real foundations

- Native Win32 application and updater/release path.
- Vulkan renderer with separated sky, terrain, character, water and HUD responsibilities.
- Streamed deterministic voxel chunks and asynchronous meshing.
- Player movement, collision, gravity, jump, sprint, crouch and mouse look.
- Persistent saves including world/player/inventory/mining/micro edits/drops.
- Physical first-person swing/contact foundation.
- Persistent block damage and macro/micro/mixed mining concepts.
- 8×8×8 promoted micro-voxel occupancy for local carving.
- Inventory/hotbar data model and physical drops.
- Material-aware particles.
- Semantic audio events and Windows XAudio2 backend.
- Explicit Water identity, transparent render pass and sparse active-cell fluid foundation.
- Vegetation growth/detail foundation.
- Native menu/inventory/settings painter foundations.
- CI/release automation for Linux core + native Windows release.

These systems should be repaired/refined rather than casually replaced unless profiling or architecture proves a rewrite is necessary.

---

# 3. Current real-hardware problems — treat these as the immediate truth

The following problems were observed in the shipped 0.5.0 build and form the next repair/polish queue.

## P0 — interaction/body/UI blockers

### First-person body/swing

- The arm visibly stretches/telescopes during the swing.
- When the player rotates, the hand can appear locked to a point while wrist/arm geometry spins unnaturally around it.
- The fists are not behaving like children of a coherent player-body transform hierarchy.
- Downward punching/reach becomes awkward after removing nearby blocks.
- The player should be able to swing in empty air; animation must not require a valid target.
- Default unarmed strike should become a natural side/diagonal swipe with fixed limb lengths.
- Physical contact must remain the source of damage; camera ray may nominate intent but must not directly mine.
- One swing must not tunnel through a destroyed front block into rear blocks.

### Inventory / pause / settings

- Inventory is not reliably accessible in the real build.
- Pause/settings state can accept clicks while the actual menu is invisible.
- Escape can require multiple presses because hidden modal states remain active.
- Mouse capture/release must agree with visible UI state.
- The UI state machine and final compositing order require repair, not a fake placeholder workaround.

### Interaction readability

- Add a clear in-reach block/face highlight.
- Placement should preview the target face/ghost block.
- Out-of-range targets should not look valid.
- Micro/mixed mining may eventually preview the local affected region.

## P1 — material and vegetation quality blockers

### Grass

What is working:
- broad light/dark variation is less obviously tiled;
- blades are smaller than the earlier oversized pass.

What is wrong:
- surface looks blurry/low-resolution;
- material lacks crisp micro-definition;
- blade/turf coverage still feels sparse compared with the reference;
- desired reference language is a dense walkable turf layer with individually readable micro pieces, subtle cavities and occasional flowers.

Required direction:
- preserve macro anti-tiling variation;
- add authored/high-frequency albedo, normal, roughness, AO/cavity and height information;
- build a dense short turf layer plus controlled blade/clutter geometry;
- roots must visually connect grass into soil on exposed sides.

### Dirt / rooted soil

Current dirt has ugly directional/line artifacts and does not read as premium soil.

Target language:
- irregular soil clumps;
- dark cavities;
- small embedded stones/pebbles;
- moisture/color variation;
- root fibers and rooted turf transition;
- every micro/pixel region should have intentional depth/color definition without turning into noisy mush.

### Bark / end grain

- Current bark reads as repetitive vertical stripes rather than irregular bark plates.
- End-grain rings show an unwanted seam/line.

Target:
- interrupted vertical bark plates, cracks, knots and recessed channels;
- separate side-bark and cut-end coordinate systems;
- clean irregular rings with no seam through the center.

### Leaves / tree structure

- Canopies can behave like hollow shells: breaking one outer leaf reveals emptiness/inner block faces in an unnatural way.
- Tree silhouettes are too boxy/funky and not full enough.

Target generation:
- trunk → primary branches → secondary branches → foliage clusters;
- dense core, medium canopy and broken/porous edge layer;
- breaking leaves should reveal more foliage/branches/gaps, not a hollow cube.

## P2 — water blockers

Current 0.5 water is a foundation, not the target.

Observed problems:
- too perfectly transparent/clear at many depths;
- terrain beneath water can remain overly readable;
- underwater mining can produce flickering/glitching geometry;
- fluid/terrain boundaries can fight visually;
- water may pop into a newly opened cell instead of visibly flowing;
- user-observed cases exist where water does not fill a gap properly, or appears to fill instantly without fluid motion;
- active-cell telemetry may remain at zero when the user expects disturbed water to simulate.

Required architecture:
- authoritative sparse fluid cells store discrete volume/fill and flow hints;
- stable water sleeps;
- terrain changes wake only local fluid neighborhoods;
- gravity-first transfer, then lateral equalization under a fixed tick/work budget;
- renderer interpolates between previous/target fluid heights so simulation steps appear fluid instead of teleporting;
- internal water faces are culled;
- no coincident coplanar terrain/water surfaces;
- local terrain edits invalidate the local fluid surface mesh;
- depth absorption/fog increases with optical depth;
- falling water/waterfalls get a distinct render state;
- GPU/visual particles sell spray, droplets, foam and turbulence but are not authoritative bulk water.

## P3 — terrain, distance and presentation blockers

- World is too flat/samey over long travel.
- Regions do not yet create strong valleys, mountains, cliffs, watersheds or exploration silhouettes.
- Distance presentation looks pale/washed out and lacks atmospheric depth.
- Current scene is often overexposed and excessively lime/bright.

Required direction:
- regional/continental terrain fields;
- ridges, valleys, basins, cliffs and biome-dependent height character;
- hydrology should eventually help shape geography before decoration;
- atmospheric perspective and controlled distance haze;
- meaningful directional shadows/contact shading/AO;
- stable exposure/tone mapping and richer color grading;
- far terrain LOD that preserves silhouette instead of becoming flat fog-colored geometry.

---

# 4. Visual construction law — blocks must look MADE from material

This is a core art/engine requirement.

A premium RuneForge block should not look like a high-resolution JPEG pasted on a cube. It should read as a clean voxel material constructed from many smaller forms.

Use three visual scales:

1. **Macro:** normal world block / construction unit.
2. **Meso:** roots, soil clumps, stone plates, bark plates, moss patches, fractured pieces.
3. **Micro:** grains, fibers, pebble pieces, turf nodes, cavities, chips, micro-AO and color variation.

Near the player, selected materials may expose real micro-surface geometry/promotion. Mid distance should retain the appearance through authored normal/height/AO/material detail. Far distance should preserve macro identity and silhouette cheaply.

**Never make a block visibly downgrade when it becomes interactable or promoted for micro mining.** Representation transitions must preserve or improve apparent quality.

The game itself, including the hero character, should preserve the voxel/pixel construction language: armor plates, fingers, hair, cloth folds, belts, roots, foliage and stone should look assembled from deliberate small block forms rather than smooth generic primitives.

---

# 5. First-person body law

The player must be an embodied hierarchical voxel character, not floating world-space hands.

Required transform hierarchy direction:

```text
PlayerRoot
└─ Pelvis
   └─ Spine
      └─ Chest
         ├─ Neck → Head → Camera
         ├─ LeftShoulder → UpperArm → Elbow → Forearm → Wrist → Hand
         └─ RightShoulder → UpperArm → Elbow → Forearm → Wrist → Hand → ToolSocket
```

Rules:

- fixed limb segment lengths;
- local parent-relative transforms;
- player yaw rotates the whole body/root;
- pitch affects camera/head and controlled upper-body aiming, not a detached world-space wrist;
- swing animation is joint rotation/pose animation, not stretching a hand toward an impact point;
- visible arm and physical contact derive from the same animation state/pose;
- LMB always starts a swing if gameplay input is allowed, even when there is no target;
- during active strike frames, a swept sphere/capsule checks fist/tool contact;
- first valid solid contact locks the swing;
- raycast is for intention/highlight only, not authoritative damage;
- later tools reuse this system with different pose, reach envelope, timing, contact volume and effects.

Preferred unarmed default: a side/diagonal swipe with enough vertical envelope to comfortably hit waist-level/downward blocks without requiring awkward camera gymnastics.

---

# 6. UI/UX law

UI is not programmer debug chrome. Gameplay HUD remains minimal; modal menus may be richer and atmospheric.

Required top-level states should be explicit and observable, for example:

```text
Gameplay
Pause
Settings
Inventory
Crafting
```

Do not allow invisible modal states.

Input expectations:

- Gameplay + Escape → Pause.
- Pause + Escape → Gameplay.
- Settings + Escape → previous menu (normally Pause or Hub).
- Inventory + Tab/I/Escape → close inventory appropriately.
- Opening a modal releases/captures mouse exactly once and consistently.
- Gameplay input must not continue behind modal UI.

Presentation order should be deterministic:

```text
Sky/world
Terrain
Characters
Water/transparency
Particles/effects
Post/tonemap
HUD
Modal UI
```

If Direct2D/DirectWrite native menu surfaces are used beside Vulkan, their visibility/compositing/lifetime must be explicit; never rely on accidental WM_PAINT behavior while a continuously presenting Vulkan surface overwrites the same HWND.

Hotbar stays bottom-center and must display actual inventory state, icons/counts and selection. Inventory must open reliably and expose real inventory data, not fake slots.

---

# 7. Architecture and file-placement rules

## Non-negotiable rule: no feature dumping

Do not solve convenience by expanding `VulkanRenderer.cpp`, `PlayerController.cpp`, `NativeWindow.cpp`, or a new `Game.cpp` into a monolith.

Current responsibility layout:

```text
src/
├─ app/                  high-level application/game-state orchestration
├─ audio/windows/        Windows audio-device/playback backend only
├─ core/                 reusable infrastructure: jobs, settings, versioning, diagnostics
├─ game/
│  ├─ audio/             semantic gameplay audio events
│  ├─ drops/             world item drop behavior
│  ├─ interaction/       physical swing/contact/reach/interaction logic
│  ├─ inventory/         inventory/stack/slot data and operations
│  ├─ mining/            mining cadence, hardness, damage rules
│  ├─ particles/         gameplay/effect particle state/events
│  ├─ items/             item identities/definitions
│  ├─ tools/             tool definitions/effectiveness/physical profiles
│  ├─ equipment/         equipped item/armor state
│  ├─ crafting/          crafting logic
│  ├─ recipes/           recipe registry/data
│  └─ stations/          workbench/forge/station gameplay
├─ platform/windows/     Win32 window, OS messages, raw/platform input, native integration
├─ render/
│  ├─ materials/         render material definitions/resources
│  ├─ scene/             render-scene extraction/culling models
│  └─ vulkan/            Vulkan device/pass/resource/draw implementation ONLY
├─ save/                 persistence/version migration/serialization
├─ ui/
│  ├─ hud/               HUD view models/painters
│  ├─ inventory/         inventory presentation
│  ├─ menus/             pause/menu presentation
│  ├─ settings/          settings presentation
│  ├─ native/            shared Direct2D/DirectWrite UI helpers
│  └─ theme/             shared visual tokens/palette
├─ updater/              bootstrap/update code
└─ world/
   ├─ blocks/             block definitions/traits
   ├─ chunks/             storage/streaming/chunk state
   ├─ generation/         deterministic macro/local generation
   ├─ growth/             vegetation simulation/state
   ├─ meshing/            voxel/micro/detail mesh builders
   ├─ micro/              micro-voxel occupancy/edit state
   ├─ fluid/              water/fluid state and solver
   ├─ simulation/         shared active-region scheduler/budgets (as introduced)
   ├─ granular/           future sand/gravel simulation
   ├─ fire/               future heat/fire simulation
   └─ structure/          future structural connectivity/dynamic voxel groups
```

## Placement decision test

Before creating/editing code, ask:

- Is this **game rule/state**? → `game/` or `world/`, not renderer.
- Is this **world spatial/simulation state**? → `world/<subsystem>/`.
- Is this **drawing/GPU resource logic**? → `render/`.
- Is this **UI data behavior**? → UI model/subsystem, not painter-only state.
- Is this **OS input/device/window/audio integration**? → platform/backend layer.
- Is this **persistence**? → `save/`, with version/migration tests.
- Is this reusable infrastructure? → `core/` only when truly cross-domain.

If a feature needs more than one layer, define a clean boundary/model/event between layers rather than putting everything in the easiest file.

## Examples of forbidden ownership

- Inventory state in `VulkanRenderer`.
- Crafting recipes in `Inventory`.
- Block drop tables in a shader/renderer.
- Fluid rules inside water fragment shader.
- Audio filenames inside mining logic.
- Windows/XAudio device code inside `MiningSystem`.
- World-generation rules inside chunk draw code.
- Menu state solely encoded as hidden booleans in renderer.

---

# 8. Data-driven design rules

Systems expected to scale to many blocks/items/materials must be registry/data driven.

Block/material definitions should be capable of describing, where relevant:

- physical solidity/collision;
- render opacity/transparency;
- material/surface identity;
- hardness;
- preferred tool;
- minimum tool tier;
- strike interval and efficiency modifiers;
- drop table;
- hit/break sound family;
- particle/fracture profile;
- future simulation traits: fluid, granular, combustible, structural strength, buoyancy, heat, erosion resistance, etc.

Do not implement dozens of `if (block == ...)` branches across unrelated systems when a definition/registry belongs at the data boundary.

---

# 9. Performance rules

Visual ambition may not destroy open-world architecture.

Every expensive system needs:

- bounded work budget;
- active/sleep policy;
- distance/visibility policy;
- profiler/diagnostic counters;
- local invalidation instead of global rebuilds;
- testable CPU/reference behavior before optional GPU acceleration.

Preserve/use:

- greedy meshing where applicable;
- chunk/frustum culling;
- async generation/meshing;
- device-local GPU resources;
- distance detail tiers;
- texture arrays/atlases for scalable material binding;
- instancing/batching for repeated detail;
- sparse active fluid/simulation cells;
- GPU particles for visual abundance rather than making every visual particle authoritative.

Do not brute-force every terrain cell, plant or water cell every frame.

---

# 10. Visual-quality implementation rules

When visual work is requested:

1. Inspect supplied reference images directly if available.
2. Name the measurable differences: scale, density, silhouette, contrast, roughness, normal strength, edge treatment, transparency, atmosphere, etc.
3. Fix geometry/material/lighting causes rather than simply changing colors.
4. Preserve voxel construction language.
5. Prefer authored material resources plus procedural anti-tiling/macro variation; procedural shader noise alone is not the final art pipeline.
6. Check near, mid and far distance presentation.
7. Avoid overexposure. Preserve deep shadows, readable highlights and atmospheric depth.
8. Materials must remain distinct: dirt cannot resemble bark; cobble cannot resemble wood; foliage cannot read as glass or opaque cubes.
9. Visual LOD transitions must not visibly downgrade when a player interacts with a block.
10. Never call a material “finished” because a shader compiles. Real-machine screenshots are the acceptance source.

---

# 11. Immediate milestone sequence from current 0.5.0 feedback

The existing 0.6+ roadmap remains the major-version plan. These repair passes describe the dependency order immediately after the observed 0.5.0 real-hardware problems.

## 0.5.1 — interaction + UI recovery

Priority:

1. hierarchical first-person body/root transform;
2. eliminate stretchy/telescoping limbs;
3. side/diagonal air swing with fixed limb lengths;
4. physical contact/reach remains authoritative;
5. target/reach/placement highlight;
6. inventory opens reliably and displays real state;
7. pause/settings become actually visible;
8. modal/escape/mouse-capture state machine repaired;
9. regression tests for swing-with-no-target, rotation attachment, front-block lock and modal transitions.

Do not add large new gameplay content to this pass.

## 0.5.2 — material + vegetation reconstruction

1. material resource/texture-array foundation if not already in the 0.6 feature branch;
2. crisp grass/turf material while retaining macro anti-tiling variation;
3. roots and layered soil transition;
4. dirt clumps/pebbles/cavity rebuild;
5. irregular bark plates;
6. end-grain seam removal;
7. fractured natural stone + separate cobble identity;
8. fuller branch-driven tree canopies with dense interior and porous edges;
9. foliage alpha/lighting tuning;
10. real-hardware screenshot comparisons against reference material closeups.

## 0.5.3 / 0.6 water + world presentation work

Depending on branch/version scope, complete:

- sparse fluid correctness around terrain edits;
- visible fill interpolation;
- partial-height/connected fluid surface meshing;
- underwater fog/depth absorption;
- water boundary/z-fighting repair;
- waterfall/falling-water representation;
- spray/foam visual particles;
- terrain regional variation;
- stronger valleys/ridges/basins;
- exposure, fog, shadows/AO and distance depth.

Do not let semantic version labels become more important than dependency correctness. If a change is large enough to be the planned 0.6 architecture, put it on the 0.6 feature branch rather than abusing hotfix numbering.

---

# 12. Major roadmap summary

## 0.6 — survival slice + authored visual identity

- item/tool/equipment architecture;
- tool-driven physical swing/contact;
- crafting/recipes/stations;
- production inventory/crafting UI;
- authored material resource pipeline;
- grass/dirt/stone/cobble/wood/foliage production materials;
- stronger lighting, shadows, atmosphere and distance presentation;
- full starter survival acceptance loop.

Acceptance loop:

**spawn → physically gather → pickup → inventory → craft/equip tool → tool changes physical mining → gather/build shelter → save → quit → continue with world/inventory/equipment preserved.**

## 0.7 — hydrology, caves, biomes and living terrain

- macro watersheds/river networks/lakes/coasts;
- water 2.0 local solver + connected surfaces/currents;
- caves/geology/resources;
- biome transitions and species packages;
- environmental audio/weather hooks;
- seamless terrain/foliage/water LOD foundation.

## 0.8 — structural physics, destruction and advanced building

- support/connectivity graph;
- falling trees and bounded dynamic voxel groups;
- material-specific structural damage/collapse;
- shared active simulation scheduler expanded to sand/fire/heat;
- line/wall/floor/stair/ring/cylinder/shape building;
- transactional construction preview and undo/redo;
- blueprint/structure recognition foundation;
- physical crafting/station interactions where they improve tactile play.

## 0.9 — settlements, roads, ecology and civilization

- hydrology/terrain-aware roads and bridges;
- settlement anchors/use semantics;
- jobs/delegation/storage/logistics;
- NPC blueprint construction;
- ecology/faction relationships;
- world memory and regional simulation LOD.

## 1.0 candidate

Not “every idea ever.” A cohesive early/mid Frontier experience with:

- distinctive premium voxel art;
- polished embodied hero/equipment;
- satisfying physical survival/tool/build loop;
- stable persistence/updater;
- authored materials and strong atmosphere;
- recognizable geography/biomes/caves/hydrology;
- meaningful water/environment interaction;
- RuneForge-specific advanced construction;
- initial settlements/roads/world-memory loop;
- measured performance and shipping-quality settings/audio/input/accessibility/error handling.

---

# 13. Required engineering workflow for every AI/chat/agent

## Step A — inspect before changing

Before saying what is implemented or broken:

- fetch current `main`/target branch;
- read this playbook and relevant roadmap/feature docs;
- inspect the relevant source and tests;
- inspect recent PR/CI/release state if the task depends on it;
- do not rely solely on previous chat summaries.

## Step B — plan the smallest clean subsystem change

State:

- root cause or hypothesis;
- files/subsystems that should own the fix;
- tests/acceptance criteria;
- performance implications;
- whether it is a hotfix, feature branch or docs-only change.

Do not turn planning into an excuse not to implement when implementation is requested.

## Step C — branch cleanly

Never pile major work directly onto `main`.

Suggested names:

- `fix/<subsystem>-<issue>-0.5.1`
- `feat/frontier-survival-visuals-0.6`
- `feat/hydrology-worldgen-0.7`
- `docs/<topic>`

A user-facing hotfix may be a small focused branch/PR. A major milestone should get its own feature branch.

## Step D — implement with subsystem boundaries

- create files/subfolders when responsibility becomes distinct;
- keep headers/interfaces small;
- avoid circular ownership;
- renderers consume data; they do not become gameplay databases;
- use events/models/snapshots between layers;
- add comments for non-obvious invariants, not narration of obvious code.

## Step E — tests before release claims

Portable/core behavior should receive deterministic tests where possible, especially:

- mining cadence/contact/reach;
- front-block lock/no tunneling;
- body transform math;
- inventory operations;
- save/load/migration;
- world generation determinism;
- fluid conservation/activation/sleep;
- meshing boundaries;
- registry contracts;
- structure/simulation rules later.

Windows-specific render/audio integration must still go through the native CI build even when portable tests are green.

## Step F — CI gate

For a code milestone intended to merge:

1. create/update PR;
2. verify Linux/core configure + compile + tests;
3. verify native Windows configure + DXC shader compile + native build + tests;
4. fix the actual cause of failures; do not weaken tests merely to get green;
5. ensure the **exact final head SHA** is green. Older green runs do not prove a newer commit.

Documentation-only commits still move the head. If policy requires exact-head CI, wait for it.

## Step G — merge only the verified head

- keep PR description synchronized with actual scope;
- squash or otherwise merge according to existing repository practice;
- record/verify the resulting `main` merge commit;
- do not call the feature released at merge time.

## Step H — version/release gate

For a user-facing release:

- increment root `VERSION` and CMake/project version consistently;
- never reuse a published version for different code;
- release workflow must build **from merged `main`**, not merely reuse an unmerged PR artifact;
- verify Release configure/build/tests/staging/package/hash/publish steps;
- release notes must describe the actual release, not stale hard-coded historical copy.

## Step I — independent release verification — mandatory

**Never say “fully released,” “done,” or “available in the updater” merely because CI passed or a publish step returned success.**

After the release workflow completes, independently fetch/read the public GitHub release and confirm:

- expected `v<version>` tag exists;
- release is not draft/prerelease unless intentionally so;
- `target_commitish` or associated release commit matches the intended merged code;
- Windows ZIP asset exists and is uploaded;
- SHA256/checksum asset exists and is uploaded;
- package names are correct;
- release is the latest release when that is intended;
- bootstrap/update behavior is compatible with the release layout.

Only after this independent verification may the agent say the release is fully published.

If release verification cannot be performed, say exactly what has and has not been verified.

---

# 14. Definition of “done”

Use precise language.

### “Implemented on branch” means
Code is committed on a branch. It may not compile or be merged unless stated.

### “CI green” means
The exact referenced commit passed the stated jobs. It does not mean released.

### “Merged” means
The verified changes reached `main`. It does not mean packaged/released.

### “Published” means
The release workflow reported a successful publish step. It still requires independent verification.

### “Fully released” means
Merged main build passed required release tests, the GitHub Release was independently fetched/verified, required package/checksum assets exist, and updater compatibility has been checked.

### “Visually fixed” means
Not merely shader compilation. The user or a captured real-hardware screenshot demonstrates the issue is actually corrected. If real-hardware validation is still pending, say so.

---

# 15. Hotfix vs feature policy

Use a hotfix (e.g. 0.5.1) for focused defects in an already released foundation:

- broken inventory/menu visibility;
- body transform/swing bug;
- save corruption;
- crash/device loss;
- severe water boundary bug;
- shader/runtime regression;
- updater/audio/input blocker.

Use a feature milestone (0.6+) for significant new architecture/content:

- complete tool/equipment/crafting progression;
- authored texture resource pipeline;
- major lighting/shadow renderer;
- hydrology world generation;
- large water solver redesign;
- structural physics;
- settlement systems.

Do not hide an enormous feature expansion inside a “hotfix.”

---

# 16. Reporting requirements when handing work back to the user

A useful engineering report should state:

- branch/PR/version involved;
- what changed at the subsystem level;
- root cause for important bugs;
- what tests/CI actually passed and for what exact head;
- what remains incomplete;
- whether the user needs real-machine validation;
- whether code is branch-only, merged, published, or fully release-verified;
- the next dependency-ordered step.

Avoid vague completion summaries such as “fixed textures” or “improved water.” Explain the mechanism.

Never pretend to have tested gameplay you could not physically run/observe. CI can validate builds/tests; the user’s screenshots are critical for visual/feel validation.

---

# 17. AI/agent anti-chaos rules

1. **Read this file first every new session.**
2. Inspect current repo before claiming state.
3. Never write major feature code directly to `main`.
4. Keep responsibilities in appropriate files/subsystems.
5. Split a file when it starts owning unrelated responsibilities; do not split mechanically into meaningless fragments.
6. Preserve existing public interfaces unless a deliberate refactor is justified/tested.
7. Do not make renderer own gameplay state.
8. Do not make UI painter own inventory/crafting truth.
9. Do not bind simulation speed to framerate or raw input event frequency.
10. Do not brute-force all world cells/foliage/water every frame.
11. Do not add a visual-only fake toggle and call it a setting; settings must affect real systems when exposed.
12. Do not ship programmer/debug presentation as final UI.
13. Do not weaken regression tests just to get CI green.
14. Do not assume a successful PR build proves the merged release package.
15. Do not call a release finished before independent release + asset verification.
16. Do not reuse old release-note copy for new milestones.
17. Do not copy another game’s code/assets/names. Competitive research is for transferable design/architecture patterns only.
18. Preserve offline play and native desktop architecture.
19. Prefer deterministic reconstruction and sparse persisted edits/state instead of massive regenerated world snapshots where possible.
20. Performance architecture is part of feature design, not a cleanup task after content explodes.

---

# 18. Recommended new-chat kickoff procedure

A new AI/chat continuing development should do this before implementation:

1. State that it will read the canonical playbook and current source.
2. Fetch/read this file, `FEATURE_MASTER_PLAN.md`, current milestone roadmap and relevant subsystem docs.
3. Fetch `VERSION`, latest release and current branch/main state.
4. Inspect source/tests for the requested issue.
5. Create an appropriate branch.
6. Implement the highest-priority dependency first.
7. Add/update tests.
8. Run/trigger CI and fix exact-head failures.
9. Merge only when the exact final head is green.
10. If user-facing release is requested/appropriate, build from merged main and independently verify release/tag/assets before saying done.
11. End with a concrete status and next step.

---

# 19. Current recommended next action

Based on the real-machine 0.5.0 feedback, the immediate engineering work should be a focused repair branch such as:

`fix/frontier-interaction-ui-0.5.1`

Start with this dependency order:

1. diagnose/fix invisible pause/settings/inventory rendering/state routing;
2. make inventory reliably accessible;
3. replace world-space/stretchy arm behavior with hierarchical fixed-length body joints;
4. allow air swings and implement the side/diagonal unarmed swing;
5. preserve physical first-contact mining and test no tunneling;
6. add target/reach/placement highlight;
7. run Linux + Windows CI;
8. real-machine test the UI/body/contact fixes;
9. only then decide whether to publish 0.5.1 or continue a tightly scoped material hotfix.

In parallel planning—but not by destabilizing the hotfix—prepare the 0.6 authored-material pipeline needed for crisp grass/roots/dirt/bark/stone/cobble and the stronger world presentation target.

---

## Final principle

**RuneForge progress is not measured by the number of systems that technically exist. It is measured by whether each system is cleanly owned, testable, performant, visually coherent, tactile in real play, and safe to build the next layer on.**

A block should look worth inspecting. A fist should feel attached to a body. Water should visibly flow. A menu should always be visible when it owns input. A distant landscape should feel deep and worth exploring. And no agent should claim a release is finished until the repository, CI, packaged release, and public assets prove it.
