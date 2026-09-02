from pathlib import Path
import re


def replace_once(path: str, old: str, new: str) -> None:
    p = Path(path)
    text = p.read_text()
    if old not in text:
        raise SystemExit(f"missing marker in {path}: {old[:100]!r}")
    p.write_text(text.replace(old, new, 1))


def replace_regex(path: str, pattern: str, replacement: str) -> None:
    p = Path(path)
    text = p.read_text()
    updated, count = re.subn(pattern, replacement, text, count=1, flags=re.S)
    if count != 1:
        raise SystemExit(f"expected one match in {path}, got {count}")
    p.write_text(updated)


# The remaining immediately visible terrain materials were still smooth/noise-heavy. Replace them
# with discrete, world-locked voxel texels and broken-up macro forms so bark, stone and leaves do not
# resemble stretched procedural noise.
materials_3_to_6 = r'''    } else if (material == 3) {
        float2 largeCell = floor(uv * 4.0);
        float2 texelCell = floor(uv * 15.0);
        float large = hash21(largeCell + 17.0);
        float texel = hash21(texelCell + 47.0);
        float fractureSeed = hash21(texelCell * 1.91 + 101.0);
        float fracture = step(fractureSeed, 0.105);
        float quartz = step(0.955, hash21(texelCell * 2.77 + 211.0));
        float3 stoneDeep = float3(0.155, 0.170, 0.178);
        float3 stoneMid = float3(0.330, 0.345, 0.345);
        float3 stoneWarm = float3(0.455, 0.425, 0.365);
        s.albedo = lerp(stoneDeep, stoneMid, 0.28 + large * 0.48);
        s.albedo = lerp(s.albedo, stoneWarm, step(0.78, texel) * 0.20);
        s.albedo *= 1.0 - fracture * 0.30;
        s.albedo = lerp(s.albedo, float3(0.63, 0.61, 0.55), quartz * 0.52);
        s.roughness = 0.91 - quartz * 0.08;
        s.relief = (texel - 0.5) * 0.13 + large * 0.08 - fracture * 0.10 + quartz * 0.12;
        s.cavity = fracture * 0.22;
    } else if (material == 4) {
        float2 plateUv = uv * float2(5.0, 8.0);
        float row = floor(plateUv.y);
        plateUv.x += fmod(abs(row), 2.0) * 0.47;
        float2 plateId = floor(plateUv);
        float2 plateLocal = frac(plateUv);
        float plateRand = hash21(plateId + 13.0);
        float plateRand2 = hash21(plateId * 2.31 + 53.0);
        float edge = min(min(plateLocal.x, 1.0 - plateLocal.x), min(plateLocal.y, 1.0 - plateLocal.y));
        float fissure = 1.0 - smoothstep(0.045, 0.115, edge);
        float chip = step(0.88, hash21(floor(uv * 17.0) + 91.0));
        float3 barkBlack = float3(0.060, 0.025, 0.010);
        float3 barkBrown = float3(0.245, 0.105, 0.030);
        float3 barkWarm = float3(0.390, 0.205, 0.070);
        s.albedo = lerp(barkBlack, barkBrown, 0.34 + plateRand * 0.48);
        s.albedo = lerp(s.albedo, barkWarm, step(0.74, plateRand2) * 0.26);
        s.albedo *= 1.0 - fissure * 0.42;
        s.albedo *= lerp(0.88, 1.05, chip);
        s.roughness = 0.96;
        s.relief = (0.5 - abs(plateLocal.x - 0.5)) * 0.10 + plateRand * 0.12 - fissure * 0.20 + chip * 0.045;
        s.cavity = fissure * 0.38;
    } else if (material == 5) {
        float2 q = frac(uv) - 0.5;
        float r = length(q);
        float ringBand = floor((r + hash21(floor(uv) + 7.0) * 0.018) * 22.0);
        float ringTone = hash21(float2(ringBand, floor(uv.x + uv.y)) + 33.0);
        float2 texelCell = floor(uv * 16.0);
        float texel = hash21(texelCell + 59.0);
        float centerDark = 1.0 - smoothstep(0.035, 0.115, r);
        float split = step(0.94, hash21(texelCell * 1.73 + 117.0));
        float3 heart = float3(0.180, 0.070, 0.018);
        float3 wood = float3(0.455, 0.245, 0.075);
        float3 fresh = float3(0.645, 0.405, 0.145);
        s.albedo = lerp(heart, wood, 0.30 + ringTone * 0.55);
        s.albedo = lerp(s.albedo, fresh, step(0.75, texel) * 0.22);
        s.albedo *= 1.0 - centerDark * 0.18 - split * 0.18;
        s.roughness = 0.90;
        s.relief = (ringTone - 0.5) * 0.11 + step(0.80, texel) * 0.05 - split * 0.07;
        s.cavity = split * 0.16 + centerDark * 0.08;
    } else if (material == 6) {
        float2 texelCell = floor(uv * 11.0);
        float leaf = hash21(texelCell + 19.0);
        float leaf2 = hash21(texelCell * 2.17 + 73.0);
        float clump = hash21(floor(p * 4.0).xz + floor(p.y * 4.0) * 7.0);
        float3 leafDeep = float3(0.025, 0.125, 0.025);
        float3 leafMid = float3(0.095, 0.315, 0.050);
        float3 leafSun = float3(0.250, 0.500, 0.095);
        s.albedo = lerp(leafDeep, leafMid, 0.30 + clump * 0.52);
        s.albedo = lerp(s.albedo, leafSun, step(0.78, leaf) * 0.28);
        s.albedo *= lerp(0.88, 1.06, step(0.50, leaf2));
        s.roughness = 0.90;
        s.relief = (leaf - 0.5) * 0.10 + step(0.84, leaf2) * 0.055;
        s.cavity = step(leaf, 0.11) * 0.08;
        // Pixel-shaped holes keep leaf masses porous without soft, blobby alpha noise.
        s.alpha = step(0.12, leaf2);
    } else if (material == 7) {'''
replace_regex(
    "shaders/voxel_scene.hlsl",
    r"    \} else if \(material == 3\) \{.*?    \} else if \(material == 7\) \{",
    materials_3_to_6,
)

# Keep the live debug/title bar useful while hardware testing the new camera mode.
replace_once(
    "src/render/vulkan/VulkanRenderer.cpp",
    '    else title += L" | Hold LMB Swing/Mine | RMB Place | M Mining Mode | 1-9 Hotbar | Tab/I Inventory | Esc Pause";\n',
    '    else title += L" | Hold LMB Swing/Mine | RMB Place | F3 Camera | M Mining Mode | 1-9 Hotbar | Tab/I Inventory | Esc Pause";\n',
)

print("visible material patch applied")
