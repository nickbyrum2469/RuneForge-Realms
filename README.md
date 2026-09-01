# RuneForge Realms

RuneForge Realms is the native rebuild of the former WORLDWEAVE prototype: a high-detail voxel survival/RPG sandbox where progression increases the **scale of agency** — from individual blocks, to shapes, structures, settlements, landscapes, and eventually realm-scale rules.

## Current status

The previous browser/WebGL prototype is now treated as **legacy reference material only**. RuneForge Realms will be rebuilt as a native desktop game from scratch.

**Hard requirements for the rebuild:**

- No Electron and no embedded Chromium runtime.
- Native Windows executable first, with architecture that can support additional platforms later.
- Modern low-level renderer with direct access to GPU capabilities.
- Highly modular source tree; no giant all-in-one source files.
- Preserve and reimplement the proven prototype ideas instead of porting the old monolithic HTML line-for-line.
- Versioned persistent worlds, migration support, backups, export/import, and update-safe saves.
- A separate native updater/bootstrapper capable of checking GitHub Releases, verifying downloads, updating atomically, and rolling back on failure.
- Visual target: richly detailed voxel/micro-voxel materials, deep lighting and atmospheric depth, and a dark carved-metal / stone / gold-trim fantasy UI with gem accents.

## Planning docs

A native-rebuild architecture and migration plan is being developed under `docs/` before production code begins. The purpose is to lock down system boundaries, file ownership, save compatibility, rendering strategy, and migration order so the new implementation does not repeat the prototype's monolithic-code problem.

## Legacy naming

Documentation may use `WORLDWEAVE` when describing the old prototype or historical systems. The product and repository name going forward is **RuneForge Realms**.
