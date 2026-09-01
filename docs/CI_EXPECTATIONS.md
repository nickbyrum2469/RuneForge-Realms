# CI Expectations

RuneForge integration branches are validated in two complementary lanes:

- **Windows native** validates the actual desktop application, Vulkan integration, DXC HLSL-to-SPIR-V shader compilation, Windows UI code, bootstrapper, packaging prerequisites, and core tests.
- **Portable core** validates deterministic world generation, meshing, jobs, persistence, registries, mining, inventory, growth, drops, and culling independently of Win32/Vulkan presentation.

Neither lane replaces the other. Both must pass before release promotion.
