# Native technology decision — RuneForge Realms

## Decision

RuneForge Realms should be rebuilt as a **native C++23 game and custom engine layer**, with a **Vulkan-first renderer** and no Electron/Chromium runtime.

The rebuild should use mature native libraries for platform plumbing and specialized subsystems, while keeping the voxel world, renderer architecture, save format, gameplay systems, progression, structure logic, and simulation under our control.

### Chosen core stack

| Area | Choice | Why |
|---|---|---|
| Core language | C++23 | Highest practical control over CPU/GPU memory, threading, data layout, SIMD, native libraries, OS APIs, and game middleware. |
| Build system | CMake + presets | Cross-platform, IDE-friendly, CI-friendly, explicit targets/modules. |
| Dependency management | vcpkg manifest | Reproducible native dependencies without vendoring everything into the repo. |
| Window/input/platform | SDL3 | Native, lightweight platform layer. **Not** SDL's renderer and not a browser runtime. |
| Graphics API | Vulkan 1.3 baseline, 1.4/2026 features opportunistically | Explicit control, excellent modern-vendor support, modern compute/indirect rendering, cross-platform path. |
| GPU memory | Vulkan Memory Allocator (VMA) | Avoids error-prone raw allocation boilerplate while preserving Vulkan control. |
| Shader source | HLSL compiled with DXC | One shader language can produce SPIR-V for Vulkan and DXIL for a future D3D12 backend. |
| Physics | Jolt Physics | Production-grade multithreaded C++ physics/collision; suitable for dynamic structures later. |
| Dynamic entities | Flecs ECS | Relationships, prefabs, hierarchy, reflection and multithreading fit NPCs, creatures, items and settlements. |
| Voxel storage | Custom chunk/section data structures | Voxels are not ordinary ECS entities and need cache-efficient specialized storage. |
| Runtime UI | RmlUi with a custom Vulkan render backend | Native C++ UI layout; generates vertices/draw commands into our renderer. No Chromium/Electron. Excellent for the ornate inventory/crafting UI target. |
| Developer UI | Dear ImGui | Diagnostics, profiler panels, live material tools. Never used as the final player-facing skin. |
| Text/fonts | FreeType + HarfBuzz | High quality and Unicode-safe. |
| Assets | glTF 2.0 for meshes, KTX2/BasisU for textures | Standard PBR asset path and GPU-friendly compressed texture distribution. |
| Compression | Zstandard | Fast region/save compression. |
| Audio | SDL3 audio initially; dedicated mixer layer above it | Native audio, no web audio. Can later swap internals without touching gameplay. |
| Profiling | Tracy + internal counters | CPU/GPU timing, job stalls, chunk/mesh costs, large-edit hitch tracking. |
| Tests | Catch2/CTest + deterministic golden tests | Worldgen, serialization, migrations, geometry and macro edit behavior need regression tests. |

## Why C++ is the best fit for this project

The question is not “which language can make a game?” Almost all mainstream languages can.

The actual question is: which choice gives RuneForge Realms the fewest artificial ceilings while we build a custom voxel renderer, high-throughput chunk streaming, persistent simulation, large structural edits, native updater, and eventually networking/modding?

For this specific project, **C++ wins**.

### C++ advantages here

- Direct, first-class access to Vulkan and Direct3D 12.
- No mandatory garbage collector pauses.
- Full control over memory ownership, arenas, pools, packed chunk formats and cache layout.
- Mature physics, audio, compression, image, networking, profiling and platform libraries.
- Easier integration with GPU vendor SDKs.
- Hot data paths can use SIMD or specialized allocators without crossing a language boundary.
- Strong path to consoles later if the project ever reaches that stage.
- No runtime or browser framework dictating how the application is packaged or rendered.

Microsoft's current Windows graphics guidance describes Direct3D 12 as the recommended game graphics API and emphasizes explicit GPU-resource control and multithreaded command submission. Direct3D 12's supported programming language is C++. Vulkan provides the same class of low-level control cross-vendor and keeps a cross-platform renderer path open.

## Why not Electron

Electron would put the launcher/UI inside Chromium and Node. That is unnecessary for this game and would create exactly the boundary we do not want between UI/runtime and native rendering systems.

RmlUi is different. It is a native C++ UI library with its own lightweight layout engine. It turns markup/style into vertices, indices, textures and draw commands that **our Vulkan renderer** submits. There is no browser process, Chromium, Node, web security model, or browser graphics ceiling.

## Why not Python

Python is useful for build tools, asset preprocessing and one-off data conversion. It is not the right core runtime here.

The renderer, chunk mesher, physics integration, streaming, save pipeline and simulation are all latency-sensitive, highly parallel systems with large contiguous data sets. Putting those behind Python would force native extensions for the exact systems that define the project, leaving a split architecture and extra debugging complexity.

Python remains welcome under `tools/` for offline utilities.

## Why not Java

Java can ship large games, but it brings a managed runtime and garbage collector into a project where we specifically want deterministic control over frame-time-sensitive allocation, GPU resource lifetime, chunk memory and native library integration.

The old inspiration from Minecraft is not a reason to inherit Java Edition's implementation constraints.

## Why not C# as the primary runtime

Modern C# is excellent for games and tooling, but C++ remains the stronger fit for a custom low-level renderer and engine where direct D3D12/Vulkan integration, memory layout and middleware integration are central.

C# can still be considered for external tools if it proves useful.

## Why not Rust as the primary runtime

Rust is the strongest alternative.

`wgpu` is a safe native graphics layer targeting modern native graphics APIs, and Rust's memory-safety model is attractive. A direct Rust/Vulkan stack can reach essentially the same GPU capabilities as C++.

The reason not to choose it here is practical ecosystem friction, not performance. RuneForge Realms is expected to integrate substantial C/C++ middleware and vendor tooling. C++ keeps those integrations native and reduces FFI boundaries while retaining the broadest mature game-engine/debugging ecosystem.

If the project's primary goal were memory safety and WebGPU portability, Rust would be a serious contender. For **maximum native game-engine control plus ecosystem depth**, C++ is the better choice.

## Why not build directly on Unreal or Godot

Both engines can produce excellent games. However, RuneForge's signature systems are unusual:

- deterministic destructible voxel terrain;
- custom multi-scale meshing;
- micro-voxel visual detail;
- giant transactional edits;
- moving local voxel grids;
- semantic structures;
- persistent region/version migration;
- simulation LOD across long-lived worlds;
- settlement-scale automation.

Using a general-purpose engine would accelerate some systems but force the defining systems to live inside another engine's world/streaming/render assumptions. This rebuild is choosing to own those assumptions.

“From scratch” should mean **our engine architecture and game logic are ours**, not “rewrite font rasterization, compression and rigid-body physics instead of using mature libraries.”

## Vulkan strategy

### Baseline

- Vulkan 1.3 feature baseline for broad modern-PC compatibility.
- Feature detection at startup.
- Device capability table recorded to diagnostics.
- Validation layers automatically enabled in developer builds.
- VMA for memory allocation.
- HLSL -> SPIR-V using Microsoft's DXC.

### Opportunistic modern features

Use only when supported:

- Vulkan 1.4 capabilities;
- Vulkan Roadmap 2026 features;
- descriptor indexing / modern descriptor paths;
- mesh shaders;
- buffer device address;
- multi-draw indirect;
- async compute;
- hardware ray queries for optional high-end effects.

The game must have fallback paths. A machine without mesh shaders still plays the same world.

## Why HLSL + DXC

DXC officially compiles HLSL to both DXIL for Direct3D and SPIR-V for Vulkan. This gives us one shader source tree and avoids locking shader code permanently to one backend.

A future D3D12 renderer can reuse shader source instead of requiring a second GLSL implementation.

## Renderer abstraction boundary

Do **not** write a giant generic rendering abstraction before a frame exists.

Phase 1 implements Vulkan directly behind a deliberately small RHI boundary:

- Buffer
- Image
- Sampler
- Pipeline
- Descriptor/resource binding
- Command list
- Fence/semaphore
- Swapchain
- Timestamp query

Game code never receives raw `Vk*` handles. After the Vulkan backend is mature, a D3D12 backend can implement that same small interface.

## SDL3 boundary

SDL3 is used for application/window lifecycle, keyboard/mouse/controller input, clipboard/OS integration, and native audio device access.

SDL3's GPU abstraction is **not** the primary renderer. Direct Vulkan preserves the lowest-level control and access to high-end Vulkan paths.

## Physics boundary

Jolt handles player/capsule collision once integrated, rigid bodies, moving assemblies, constraints, ray/sweep queries and dynamic debris where appropriate.

Voxel terrain collision remains chunk-aware. Do not create one Jolt body per block. Static terrain generates collision representation per section/chunk and updates only dirty regions.

## ECS boundary

Flecs handles objects whose identity matters: player, NPCs, creatures, drops, projectiles, doors, containers, interactive props, settlement agents, named structures and moving assemblies.

Do **not** put every terrain block in Flecs. Dense voxel terrain stays in specialized `world/voxel` storage.

## Runtime UI strategy

The supplied visual references require a real skinning/layout system: dark stone/metal panels, aged bronze/gold borders, blue gem accents, ornate corners, icon grids, equipment paper-doll, deep tooltips, storage split panes, crafting browser, quick-access hotbar and animated menu background.

RmlUi is a strong fit because it provides native retained-mode layout, animation, controls, templating and data binding while letting our renderer draw every vertex.

The final skin should use 9-slice frames, atlas iconography, layered panel materials, masked ornaments, animated gem/emissive effects, subtle depth, controller focus states and scalable DPI/layout.

## Dependency rule

Third-party libraries are isolated behind RuneForge interfaces.

Gameplay calls `PhysicsWorld`, not Jolt; `UiSystem`, not RmlUi; `AudioSystem`, not SDL audio; `RenderDevice`, not Vulkan. This keeps dependencies replaceable.

## Primary/current sources checked — August 2026

- Microsoft Windows graphics / Direct3D 12 documentation: https://learn.microsoft.com/en-us/windows/apps/develop/graphics
- Khronos Vulkan Roadmap 2026: https://www.khronos.org/blog/vulkan-introduces-roadmap-2026-and-new-descriptor-heap-extension
- SDL3: https://wiki.libsdl.org/SDL3/FrontPage
- Microsoft DirectX Shader Compiler: https://github.com/microsoft/DirectXShaderCompiler
- Vulkan Memory Allocator: https://gpuopen.com/vulkan-memory-allocator/
- Jolt Physics: https://github.com/jrouwe/JoltPhysics
- Flecs: https://www.flecs.dev/
- RmlUi: https://github.com/mikke89/RmlUi
- Khronos glTF PBR: https://www.khronos.org/gltf/pbr
- KTX2: https://registry.khronos.org/KTX/specs/2.0/ktxspec.v2.html
