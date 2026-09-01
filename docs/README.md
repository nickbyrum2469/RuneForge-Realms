# RuneForge Realms — native rebuild planning index

The old WORLDWEAVE prototype is now a historical behavior reference. Native RuneForge decisions live in these documents.

## Read in this order

1. `TECH_STACK_DECISION.md` — language/runtime/renderer decision and why there is no Electron.
2. `ARCHITECTURE.md` — repo boundaries, target modules and source-size rules.
3. `FEATURE_MASTER_PLAN.md` — complete product/game feature authority.
4. `VISUAL_RENDERING_PLAN.md` — how the supplied block/tree/water/UI references become a performant world.
5. `MIGRATION_ROADMAP.md` — phased total conversion.
6. `LEGACY_CODE_AUDIT.md` — exhaustive inventory of the old source behavior/functions/data/UI.
7. `UPDATER_RELEASE_PLAN.md` — native bootstrapper, GitHub releases, safe updates and rollback.

## Decision rule

If a historical WORLDWEAVE document conflicts with a current RuneForge document, the current RuneForge document wins. If two current documents conflict, raise the conflict in review rather than silently choosing one.

## Rewrite rule

Legacy code is never ported by copying it into an equivalently giant C++ file. The behavior gets a native owner, test and implementation in the subsystem where it belongs.
