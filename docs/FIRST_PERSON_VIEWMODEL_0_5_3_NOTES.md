# RuneForge Realms 0.5.3 — First-Person Viewmodel Invariants

This pass responds to the 0.5.2 real-hardware screenshots and clarification that the opposite-direction rotation is isolated to character/viewmodel geometry rather than terrain/world blocks.

## Required invariants

- World camera/rendering remains unchanged by this pass.
- First-person hands/items use the exact camera forward/right/up basis used for viewing.
- Unarmed first person keeps both hands subtly visible at the bottom edge during idle/walking.
- Equipped hotbar block/item shows only the dominant right hand plus held item.
- Hands/items never pop into existence only when attacking.
- Walking produces restrained camera-space hand bob while keeping the center view readable.
- Swing motion starts from the persistent rest pose, winds up, moves toward the center crosshair, follows through, and returns to the same rest pose.
- Swing visual depth responds to the locked target distance.
- A reachable center-crosshair target is the authoritative impact target; unrelated off-axis blocks are never substituted.
- Third-person character facing derives from the exact horizontal projection of the canonical camera look direction rather than a separately reconstructed yaw convention.
- No version bump or public release until exact-head Linux + native Windows CI and real-hardware validation of these changes.
