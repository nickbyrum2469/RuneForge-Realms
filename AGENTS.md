# RuneForge Realms — Agent Instructions

**Before editing this repository, read `docs/RUNEFORGE_MASTER_DEVELOPMENT_PLAYBOOK.md` in full.** It is the canonical development handoff, progress ledger, architecture/file-placement guide, visual-quality contract, testing procedure, Git workflow, and release-verification policy.

Then read, in order as relevant:

1. `docs/FEATURE_MASTER_PLAN.md` — product/feature authority.
2. `docs/FRONTIER_REALMS_0_6_PLUS_ROADMAP.md` — major milestone dependency order.
3. `docs/LAY_OF_THE_LAND_SYSTEMS_RESEARCH.md` — transferable systems research, not assets/code to copy.
4. Relevant source files/tests on the current branch.

Hard rules:

- Inspect current repository state before claiming what exists or is broken.
- Do not put major feature work directly on `main`; use a focused branch/PR.
- Preserve clean subsystem ownership. Do not turn `VulkanRenderer.cpp`, `PlayerController.cpp`, `NativeWindow.cpp`, or a new `Game.cpp` into a monolith.
- Rendering consumes game/world data; it does not own inventory, crafting, mining rules, generation, fluid truth, or simulation state.
- UI painters do not own inventory/crafting truth.
- Simulation must be bounded, active-region/sleep based, and independent of raw input frequency/framerate.
- Add deterministic portable tests for core behavior and verify native Windows integration through CI.
- A green run only proves the exact SHA that ran. Re-run/verify after later commits.
- Never weaken a useful regression test merely to get green CI.
- Do not call something visually fixed until real-hardware observation/screenshots support it; compilation is not visual acceptance.
- Never say a user-facing release is fully released just because a PR merged or a publish step succeeded. Independently fetch the public GitHub Release and verify tag, intended merge commit, ZIP asset, checksum asset, latest-release status when intended, and updater/package compatibility.
- Use precise status language: branch-only, CI-green, merged, published, or fully release-verified.

If any instruction here appears to conflict with the canonical playbook, follow `docs/RUNEFORGE_MASTER_DEVELOPMENT_PLAYBOOK.md` and `docs/FEATURE_MASTER_PLAN.md` according to the authority order defined there.
