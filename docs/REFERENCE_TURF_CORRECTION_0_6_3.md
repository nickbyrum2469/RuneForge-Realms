# RuneForge Realms 0.6.3 — reference turf correction note

This focused correction follows the 0.6.2 de-latticing/volumetric-soil rebuild and addresses remaining shape-language mismatches identifiable from the supplied hardware/reference comparison.

## Remaining issues corrected

- Visible vegetation pieces remained too thin and tall after the anchor lattice was removed; the reference is dominated by short, chunky, interlocking turf voxels.
- Hero top-bed vertical risers were incorrectly tagged `GrassSide`. Because the grass-side material intentionally transitions to soil near the bottom of a macro block, risers just above the macro top could become brown at grazing angles and create false checker/seam artifacts.
- Root coverage was too sparse/dominant per strand compared with the reference's larger number of finer embedded fibers.
- Exposed soil depth needed stronger but bounded clod projection while retaining real cavity omissions and a connected shell.
- Meadow flower accents were too varied/prominent for the supplied target.

## Correction

- Rebalance deterministic vegetation profiles toward compact two/three-piece tufts and strongly limit tall single pieces.
- Widen turf pieces while reducing their height so close vegetation reads as constructed turf mass rather than lawn spikes.
- Keep the existing R2 low-discrepancy anchor distribution and deterministic 16x16 ownership mapping unchanged.
- Increase coherent top-bed and side-clod structural relief within explicit bounds.
- Emit more, thinner root segments over the intact soil shell.
- Tag top-bed riser walls as `GrassTop`, with a regression test forbidding `GrassSide` geometry above the macro top.
- Restrict this meadow pass to tiny mostly-white/occasional-yellow flower accents.

Visual acceptance still requires new real-hardware screenshots. CI proves deterministic behavior, geometry invariants, budgets, and shader/native integration only.
