# RuneForge Realms

RuneForge Realms is the native successor to the WORLDWEAVE browser prototype: a persistent voxel survival sandbox whose core progression increases the **scale of agency** from individual blocks to shapes, structures, settlements, landscapes and eventually world rules.

## Status — 0.2.0 Vulkan Foundation

RuneForge is a real native C++23 Windows application rather than an Electron/browser wrapper. The 0.1.0 release established the hub, updater, CI and GitHub Release channel. The 0.2.0 pass begins the actual game renderer.

Current foundation:

- native Windows executable and C++23/CMake project;
- Direct2D/DirectWrite hub shell using the RuneForge hub composition;
- **Vulkan 1.3 game-renderer path** launched by the hub's PLAY button;
- dynamic Vulkan loader through pinned `volk` + pinned Khronos Vulkan headers, with no machine-wide Vulkan SDK required to compile the C++ renderer;
- physical-device scoring with discrete-GPU preference and graphics/present queue discovery;
- Win32 Vulkan surface and swapchain creation;
- sRGB presentation, resize/out-of-date swapchain recovery and depth buffering;
- command pool/buffer, fences, semaphores and frame submission;
- graphics render pass and pipeline;
- HLSL shaders compiled to SPIR-V with Microsoft's DXC;
- first GPU-rendered voxel diorama: grass/dirt blocks, a trunk and voxel canopy rendered as instanced cubes with directional lighting, fog, gamma/tonal shaping and depth testing;
- simple live camera/orbit controls;
- clean handoff between the native hub renderer and the Vulkan game renderer on the same application window;
- separate `RuneForgeBootstrap.exe` that checks the repository's latest GitHub Release before launching the game runtime;
- rolling runtime update by replacing the `runtime/` folder only while the game is not running;
- deterministic core model and semantic-version tests;
- GitHub Actions CI and Windows release packaging.

The current voxel diorama is deliberately a **renderer proof**, not production terrain. Chunk storage, meshing, GPU upload queues, PBR material resources and streaming are the next layer built on this renderer foundation.

## Run the Windows release

1. Open the repository's **Releases** page.
2. Download `RuneForgeRealms-Windows-x64.zip` from the newest release.
3. Extract the entire `RuneForgeRealms` folder.
4. Run `RuneForgeBootstrap.exe`.

From then on, the bootstrapper checks the latest GitHub Release on startup. When a newer semantic version is published, it downloads the new release, stages it, swaps the `runtime/` folder, and then launches the updated game.

In 0.2.0, press **PLAY** from the hub to enter the Vulkan Voxel Lab. Controls: **Left/Right** orbit, **Up/Down** pitch, **W/S** zoom, **Space** toggles automatic orbit, **R** resets the camera, and **Esc** returns to the hub.

## Build locally on Windows

The C++ Vulkan loader/header dependencies are fetched automatically by CMake. Shader compilation uses the official Microsoft DXC package; the helper below downloads it into the ignored `.deps/` directory.

```powershell
pwsh -File tools/setup_windows_deps.ps1
cmake -S . -B build -A x64 -DRF_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix staging/RuneForgeRealms
```

A current Vulkan-capable graphics driver is required to run the Vulkan scene.

## Versioning rule

Every user-facing pass merged to `main` must increment the root `VERSION` file and `project(... VERSION ...)` value. The release workflow creates `v<version>` and uploads the Windows package. Never reuse a published version for different code.

## Architecture

Read `docs/README.md` first. The old WORLDWEAVE HTML is reference material only; it is not a production dependency.
