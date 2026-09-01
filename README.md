# RuneForge Realms

RuneForge Realms is the native successor to the WORLDWEAVE browser prototype: a persistent voxel survival sandbox whose core progression increases the **scale of agency** from individual blocks to shapes, structures, settlements, landscapes and eventually world rules.

## Status — 0.1.0 Native Foundation

This repository is now a real C++23 Windows application rather than an Electron/browser wrapper.

The first released build proves the distribution and update foundation:

- native Windows executable;
- native C++23 hub shell using Direct2D/DirectWrite for a dependency-light first release;
- layout based on the new RuneForge hub direction: left navigation rail, featured realm, friends panel, mode carousel, player/currency strip and news bar;
- separate `RuneForgeBootstrap.exe` that checks the repository's latest GitHub Release before launching the game runtime;
- rolling runtime update by replacing the `runtime/` folder only while the game is not running;
- rollback folder retained during update swaps;
- deterministic core model and semantic-version tests;
- GitHub Actions CI and Windows release packaging.

The temporary Direct2D hub renderer is **not** the final 3D renderer. The game-world renderer remains Vulkan-first per `docs/TECH_STACK_DECISION.md`. Keeping the bootstrap/hub foundation extremely dependency-light lets us prove releases and automatic updating before introducing the heavier Vulkan/asset stack.

## Run the Windows release

1. Open the repository's **Releases** page.
2. Download `RuneForgeRealms-Windows-x64.zip` from the newest release.
3. Extract the entire `RuneForgeRealms` folder.
4. Run `RuneForgeBootstrap.exe`.

From then on, the bootstrapper checks the latest GitHub Release on startup. When a newer semantic version is published, it downloads the new release, stages it, swaps the `runtime/` folder, and then launches the updated game.

## Build locally on Windows

```powershell
cmake -S . -B build -A x64 -DRF_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix staging/RuneForgeRealms
```

## Versioning rule

Every user-facing pass merged to `main` must increment the root `VERSION` file and `project(... VERSION ...)` value. The release workflow creates `v<version>` and uploads the Windows package. Never reuse a published version for different code.

## Architecture

Read `docs/README.md` first. The old WORLDWEAVE HTML is reference material only; it is not a production dependency.
