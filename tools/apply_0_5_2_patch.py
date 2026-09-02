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
        raise SystemExit(f"expected one regex match in {path}, got {count}: {pattern[:100]!r}")
    p.write_text(updated)


# One camera basis owns movement, targeting, world projection, and third-person placement.
replace_once(
    "src/render/vulkan/VulkanRenderer.h",
    "        float yaw{};\n        float pitch{};\n",
    "        float cameraForwardX{};\n        float cameraForwardY{};\n        float cameraForwardZ{};\n",
)

replace_once(
    "src/render/vulkan/VulkanRenderer.cpp",
    "        -forward.y * right.z,\n",
    "        forward.y * right.z,\n",
)
replace_once(
    "src/render/vulkan/VulkanRenderer.cpp",
    "    constexpr float acquisitionReach = game::interaction::MiningSwing::fistReach + 0.04f;\n",
    "    constexpr float acquisitionReach = game::interaction::MiningSwing::interactionReach + 0.04f;\n",
)

new_push_data = r'''void VulkanRenderer::updatePushData(float elapsedSeconds) {
    const auto playerEye = player_.eyePosition();
    const auto direction = player_.lookDirection();
    auto eye = playerEye;

    if (player_.thirdPerson()) {
        constexpr float desiredCameraDistance = 3.20f;
        constexpr float wallPadding = 0.20f;
        const game::Vec3 backward = direction * -1.0f;
        float cameraDistance = desiredCameraDistance;
        const auto obstruction = world_.raycast(playerEye.x, playerEye.y, playerEye.z,
                                                backward.x, backward.y, backward.z,
                                                desiredCameraDistance);
        if (obstruction.hit) {
            const float dx = obstruction.worldX - playerEye.x;
            const float dy = obstruction.worldY - playerEye.y;
            const float dz = obstruction.worldZ - playerEye.z;
            const float hitDistance = std::sqrt(dx * dx + dy * dy + dz * dz);
            cameraDistance = std::clamp(hitDistance - wallPadding, 0.42f, desiredCameraDistance);
        }
        eye = playerEye + backward * cameraDistance;
    }

    pushData_.time = elapsedSeconds;
    pushData_.aspect = static_cast<float>(swapchainExtent_.width) /
                       static_cast<float>(std::max<std::uint32_t>(swapchainExtent_.height, 1));
    pushData_.eyeX = eye.x;
    pushData_.eyeY = eye.y;
    pushData_.eyeZ = eye.z;
    pushData_.cameraForwardX = direction.x;
    pushData_.cameraForwardY = direction.y;
    pushData_.cameraForwardZ = direction.z;
    pushData_.viewportWidth = static_cast<float>(swapchainExtent_.width);
    pushData_.viewportHeight = static_cast<float>(swapchainExtent_.height);
    const float fovRadians = settings_.fovDegrees * 0.01745329251994329577f;
    pushData_.fovScale = 1.0f / std::tan(fovRadians * 0.5f);
    pushData_.foliageQuality = static_cast<float>(settings_.foliageQuality);

    const auto selected = selectedPlacementBlock();
    pushData_.selectedMaterial = selected ? static_cast<float>(static_cast<std::uint32_t>(*selected)) : -1.0f;
    pushData_.miningMode = static_cast<float>(static_cast<std::uint8_t>(mining_.mode()));

    constexpr float previewReach = 4.50f;
    constexpr float interactionReach = game::interaction::MiningSwing::interactionReach + 0.04f;
    const auto hit = world_.raycast(playerEye.x, playerEye.y, playerEye.z,
                                    direction.x, direction.y, direction.z, previewReach);
    if (hit.hit && hit.block.y > 0) {
        const float dx = hit.worldX - playerEye.x;
        const float dy = hit.worldY - playerEye.y;
        const float dz = hit.worldZ - playerEye.z;
        const float hitDistance = std::sqrt(dx * dx + dy * dy + dz * dz);
        const bool reachable = hitDistance <= interactionReach;

        pushData_.targetBlockX = static_cast<float>(hit.block.x);
        pushData_.targetBlockY = static_cast<float>(hit.block.y);
        pushData_.targetBlockZ = static_cast<float>(hit.block.z);
        // +1 = reachable cyan/green outline, -1 = visible but out-of-range red/orange outline.
        pushData_.targetActive = reachable ? 1.0f : -1.0f;

        if (reachable) {
            currentMiningTarget_ = hit.block;
            if (mining_.mode() == game::mining::MiningMode::Micro) {
                const auto* state = world_.microState(hit.block);
                currentMiningProgress_ = state ? (1.0f - state->solidFraction()) : 0.0f;
            } else {
                currentMiningProgress_ = mining_.damageAt(hit.block);
            }
        } else {
            currentMiningTarget_.reset();
            currentMiningProgress_ = 0.0f;
        }
    } else {
        currentMiningTarget_.reset();
        currentMiningProgress_ = 0.0f;
        pushData_.targetBlockX = pushData_.targetBlockY = pushData_.targetBlockZ = 0.0f;
        pushData_.targetActive = 0.0f;
    }
    pushData_.miningProgress = currentMiningProgress_;

    const auto& slots = inventory_.slots();
    pushData_.hotbar0 = packHotbarStack(slots[0]);
    pushData_.hotbar1 = packHotbarStack(slots[1]);
    pushData_.hotbar2 = packHotbarStack(slots[2]);
    pushData_.hotbar3 = packHotbarStack(slots[3]);
    pushData_.hotbar4 = packHotbarStack(slots[4]);
    pushData_.hotbar5 = packHotbarStack(slots[5]);
    pushData_.hotbar6 = packHotbarStack(slots[6]);
    pushData_.hotbar7 = packHotbarStack(slots[7]);
    pushData_.hotbar8 = packHotbarStack(slots[8]);
    pushData_.selectedHotbar = static_cast<std::uint32_t>(inventory_.selectedHotbar());
}

'''
replace_regex(
    "src/render/vulkan/VulkanRenderer.cpp",
    r"void VulkanRenderer::updatePushData\(float elapsedSeconds\) \{.*?\n\}\n\n(?=void VulkanRenderer::drawFrame)",
    new_push_data,
)
replace_once(
    "src/render/vulkan/VulkanRenderer.cpp",
    "        case 'M': mining_.cycleMode(); currentMiningProgress_ = 0.0f; miningSwing_.reset(); break;\n        case VK_F5: saveNow(); break;\n",
    "        case 'M': mining_.cycleMode(); currentMiningProgress_ = 0.0f; miningSwing_.reset(); break;\n        case VK_F3: player_.toggleCameraMode(); miningSwing_.reset(); break;\n        case VK_F5: saveNow(); break;\n",
)

for shader in ["shaders/voxel_scene.hlsl", "shaders/water_scene.hlsl"]:
    replace_once(
        shader,
        "    float yaw;\n    float pitch;\n",
        "    float cameraForwardX;\n    float cameraForwardY;\n    float cameraForwardZ;\n",
    )

replace_regex(
    "shaders/voxel_scene.hlsl",
    r"float3 cameraForward\(\) \{.*?\n\}\n\n(?=float3 cameraRay)",
    r'''float3 cameraForward() {
    return normalize(float3(pushData.cameraForwardX, pushData.cameraForwardY, pushData.cameraForwardZ));
}

float3 cameraRight() {
    float3 forward = cameraForward();
    float3 right = float3(forward.z, 0.0, -forward.x);
    float lengthSq = dot(right, right);
    return lengthSq > 0.000001 ? right * rsqrt(lengthSq) : float3(1.0, 0.0, 0.0);
}

''',
)
voxel_shader = Path("shaders/voxel_scene.hlsl")
text = voxel_shader.read_text()
old_right = "float3 right = normalize(float3(cos(pushData.yaw), 0.0, -sin(pushData.yaw)));"
if text.count(old_right) != 2:
    raise SystemExit(f"expected two legacy camera-right expressions, got {text.count(old_right)}")
voxel_shader.write_text(text.replace(old_right, "float3 right = cameraRight();"))

# Crisp world-locked texels replace smooth FBM smearing on grass and soil.
material_012 = r'''    if (material == 0) {
        float2 coarseCell = floor(p.xz * 4.0);
        float2 texelCell = floor(p.xz * 16.0);
        float coarse = hash21(coarseCell + 3.7);
        float texel = hash21(texelCell + 19.1);
        float accent = hash21(texelCell * 1.73 + 47.0);
        float3 deep = float3(0.060, 0.205, 0.035);
        float3 mid = float3(0.175, 0.405, 0.070);
        float3 sunlit = float3(0.330, 0.565, 0.110);
        s.albedo = lerp(deep, mid, 0.28 + coarse * 0.52);
        s.albedo = lerp(s.albedo, sunlit, step(0.76, texel) * (0.20 + accent * 0.22));
        s.albedo *= lerp(0.88, 1.05, step(0.43, accent));
        s.roughness = 0.95;
        s.relief = (texel - 0.5) * 0.070 + step(0.88, accent) * 0.050;
        s.cavity = step(texel, 0.12) * 0.08;
    } else if (material == 1) {
        float localY = frac(p.y + 0.001);
        float turf = smoothstep(0.865, 0.955, localY);
        float2 texelCell = floor(uv * 15.0);
        float texel = hash21(texelCell + 11.0);
        float patch = hash21(floor(uv * 5.0) + 7.0);
        float rootColumn = step(0.90, hash21(float2(floor(uv.x * 11.0), floor(p.x + p.z) + 29.0)));
        float rootSegment = step(0.62, hash21(float2(floor(uv.x * 11.0), floor(uv.y * 7.0)) + 61.0));
        float root = rootColumn * rootSegment * (1.0 - turf);
        float3 darkSoil = float3(0.105, 0.048, 0.020);
        float3 warmSoil = float3(0.285, 0.145, 0.052);
        float3 turfDark = float3(0.060, 0.210, 0.032);
        float3 turfLight = float3(0.255, 0.475, 0.080);
        float3 soil = lerp(darkSoil, warmSoil, 0.22 + patch * 0.50 + step(0.70, texel) * 0.16);
        float3 green = lerp(turfDark, turfLight, 0.30 + texel * 0.55);
        s.albedo = lerp(soil, green, turf);
        s.albedo = lerp(s.albedo, float3(0.40, 0.255, 0.105), root * 0.58);
        s.roughness = 0.97;
        s.relief = (texel - 0.5) * 0.085 + root * 0.055;
        s.cavity = step(texel, 0.10) * 0.07 + root * 0.10;
    } else if (material == 2) {
        float2 coarseCell = floor(uv * 4.0);
        float2 texelCell = floor(uv * 14.0);
        float coarse = hash21(coarseCell + 5.3);
        float texel = hash21(texelCell + 23.0);
        float pebbleSeed = hash21(texelCell * 2.31 + 83.0);
        float darkChip = step(texel, 0.13);
        float pebble = step(0.93, pebbleSeed);
        float3 deep = float3(0.095, 0.043, 0.017);
        float3 brown = float3(0.270, 0.135, 0.047);
        float3 ochre = float3(0.385, 0.215, 0.080);
        s.albedo = lerp(deep, brown, 0.26 + coarse * 0.50);
        s.albedo = lerp(s.albedo, ochre, step(0.72, texel) * 0.28);
        s.albedo = lerp(s.albedo, float3(0.31, 0.29, 0.25), pebble * 0.72);
        s.albedo *= 1.0 - darkChip * 0.18;
        s.roughness = 0.97 - pebble * 0.08;
        s.relief = (texel - 0.5) * 0.11 + pebble * 0.17 - darkChip * 0.05;
        s.cavity = darkChip * 0.14;
    } else if (material == 3) {'''
replace_regex(
    "shaders/voxel_scene.hlsl",
    r"    if \(material == 0\) \{.*?    \} else if \(material == 3\) \{",
    material_012,
)

replace_once(
    "shaders/voxel_scene.hlsl",
    "    float fog = smoothstep(68.0, 205.0, max(input.depth, 0.0));\n",
    r'''    if (abs(pushData.targetActive) > 0.5) {
        float3 interior = input.worldPosition - geometricNormal * 0.0025;
        float3 blockCoord = floor(interior);
        float3 targetCoord = float3(pushData.targetBlockX, pushData.targetBlockY, pushData.targetBlockZ);
        float isTarget = 1.0 - step(0.01, length(blockCoord - targetCoord));
        if (isTarget > 0.5) {
            float2 faceUv = frac(surfaceUv(input.worldPosition, geometricNormal));
            float edgeDistance = min(min(faceUv.x, 1.0 - faceUv.x), min(faceUv.y, 1.0 - faceUv.y));
            float border = 1.0 - smoothstep(0.018, 0.050, edgeDistance);
            float3 reachColor = pushData.targetActive > 0.0 ? float3(0.20, 0.95, 0.72)
                                                           : float3(1.00, 0.30, 0.12);
            lit = lerp(lit, reachColor * 1.28, border * 0.78);
        }
    }

    float fog = smoothstep(68.0, 205.0, max(input.depth, 0.0));
''',
)
replace_once(
    "shaders/voxel_scene.hlsl",
    "    color = lerp(color, float3(0.35,0.70,1.0), centerGem);\n",
    r'''    float3 gemColor = pushData.targetActive > 0.5 ? float3(0.22,0.96,0.72) :
                      (pushData.targetActive < -0.5 ? float3(1.0,0.30,0.12) : float3(0.35,0.70,1.0));
    color = lerp(color, gemColor, centerGem);
''',
)

# Dense small-cube sculpting replaces bead-chain limbs and the sparse torso shell.
torso = r'''void addTorso(world::VoxelMesh& mesh, const PlayerBodyPose& pose, world::SurfaceMaterial material,
              float expansion = 0.0f) {
    constexpr int layers = 9;
    constexpr float pixel = 0.058f;
    for (int layer = 0; layer < layers; ++layer) {
        const float t = static_cast<float>(layer) / static_cast<float>(layers - 1);
        const Vec3 center = pose.pelvis * (1.0f - t) + pose.chest * t;
        const int halfWidth = layer <= 1 ? 3 : (layer <= 4 ? 3 : 4);
        const int halfDepth = layer <= 2 ? 1 : 2;
        for (int x = -halfWidth; x <= halfWidth; ++x) {
            for (int z = -halfDepth; z <= halfDepth; ++z) {
                const bool surface = std::abs(x) == halfWidth || std::abs(z) == halfDepth ||
                                     layer == 0 || layer == layers - 1;
                if (!surface) continue;
                const Vec3 p = center + pose.right * (static_cast<float>(x) * (pixel + expansion * 0.11f)) +
                               pose.forward * (static_cast<float>(z) * (pixel + expansion * 0.10f));
                addPixel(mesh, p, pixel + expansion, material);
            }
        }
    }

    if (expansion <= 0.0001f && material == world::SurfaceMaterial::CharacterSkin) {
        const Vec3 upper = pose.pelvis * 0.28f + pose.chest * 0.72f;
        const Vec3 middle = pose.pelvis * 0.48f + pose.chest * 0.52f;
        const Vec3 lower = pose.pelvis * 0.68f + pose.chest * 0.32f;
        for (int side = -1; side <= 1; side += 2) {
            addPixel(mesh, upper + pose.right * (0.105f * side) + pose.forward * 0.142f, 0.065f, material);
            addPixel(mesh, upper - pose.up * 0.060f + pose.right * (0.082f * side) + pose.forward * 0.136f, 0.058f, material);
            addPixel(mesh, middle + pose.right * (0.070f * side) + pose.forward * 0.132f, 0.056f, material);
            addPixel(mesh, lower + pose.right * (0.062f * side) + pose.forward * 0.126f, 0.052f, material);
            addPixel(mesh, upper + pose.right * (0.125f * side) - pose.forward * 0.136f, 0.058f, material);
        }
    }
}

void addLimbCluster(world::VoxelMesh& mesh, Vec3 center, const PlayerBodyPose& body,
                    float cubeSize, float spread, world::SurfaceMaterial material) {
    addPixel(mesh, center + body.right * spread + body.up * spread, cubeSize, material);
    addPixel(mesh, center - body.right * spread + body.up * spread, cubeSize, material);
    addPixel(mesh, center + body.right * spread - body.up * spread, cubeSize, material);
    addPixel(mesh, center - body.right * spread - body.up * spread, cubeSize, material);
}

'''
replace_regex(
    "src/render/scene/VoxelCharacterBuilder.cpp",
    r"void addTorso\(world::VoxelMesh& mesh,.*?\n\}\n\n(?=void addHand)",
    torso,
)

arm = r'''void addArm(world::VoxelMesh& mesh, const PlayerBodyPose& body, const ArmPose& arm,
            world::SurfaceMaterial material, float expansion = 0.0f) {
    for (int i = 0; i < 6; ++i) {
        const float t = static_cast<float>(i) / 5.0f;
        const Vec3 center = arm.shoulder * (1.0f - t) + arm.elbow * t;
        addLimbCluster(mesh, center, body, 0.067f + expansion * 0.55f, 0.040f + expansion * 0.18f, material);
    }
    for (int i = 0; i < 6; ++i) {
        const float t = static_cast<float>(i) / 5.0f;
        const Vec3 center = arm.elbow * (1.0f - t) + arm.wrist * t;
        addLimbCluster(mesh, center, body, 0.061f + expansion * 0.52f, 0.035f + expansion * 0.16f, material);
    }
    addPixel(mesh, arm.shoulder, 0.152f + expansion, material);
    addPixel(mesh, arm.shoulder - body.up * 0.045f, 0.126f + expansion * 0.8f, material);
    addPixel(mesh, arm.elbow, 0.112f + expansion * 0.7f, material);
    addHand(mesh, body, arm, material, expansion * 0.55f);
}

'''
replace_regex(
    "src/render/scene/VoxelCharacterBuilder.cpp",
    r"void addArm\(world::VoxelMesh& mesh,.*?\n\}\n\n(?=void addLeg)",
    arm,
)

leg = r'''void addLeg(world::VoxelMesh& mesh, const PlayerBodyPose& body, const LegPose& leg,
            world::SurfaceMaterial material, float expansion = 0.0f) {
    for (int i = 0; i < 7; ++i) {
        const float t = static_cast<float>(i) / 6.0f;
        const Vec3 center = leg.hip * (1.0f - t) + leg.knee * t;
        addLimbCluster(mesh, center, body, 0.075f + expansion * 0.55f, 0.045f + expansion * 0.18f, material);
    }
    for (int i = 0; i < 7; ++i) {
        const float t = static_cast<float>(i) / 6.0f;
        const Vec3 center = leg.knee * (1.0f - t) + leg.ankle * t;
        addLimbCluster(mesh, center, body, 0.066f + expansion * 0.52f, 0.038f + expansion * 0.16f, material);
    }
    addPixel(mesh, leg.hip, 0.145f + expansion, material);
    addPixel(mesh, leg.knee, 0.130f + expansion * 0.75f, material);

    const Vec3 footDirection = safeDirection(leg.ankle, leg.foot, body.forward);
    const Vec3 toe = leg.foot + footDirection * 0.075f;
    addPixel(mesh, leg.ankle, 0.128f + expansion, material);
    addPixel(mesh, leg.foot, 0.145f + expansion, material);
    for (int x = -2; x <= 2; ++x) {
        addPixel(mesh, toe + body.right * (static_cast<float>(x) * 0.038f),
                 0.075f + expansion * 0.55f, material);
    }
}

'''
replace_regex(
    "src/render/scene/VoxelCharacterBuilder.cpp",
    r"void addLeg\(world::VoxelMesh& mesh,.*?\n\}\n\n(?=void addLoincloth)",
    leg,
)

# Regression coverage: camera mode and usable reach beyond the literal arm chain.
replace_once(
    "tests/TestCharacterRig.cpp",
    '#include "game/Math.h"\n',
    '#include "game/Math.h"\n#include "game/PlayerController.h"\n',
)
replace_once(
    "tests/TestCharacterRig.cpp",
    "    const game::Vec3 feet{0.5f, 9.0f, 0.5f};\n",
    "    game::PlayerController cameraPlayer;\n    cameraPlayer.spawn({0.5f, 9.0f, 0.5f}, 0.0f, 0.0f);\n    assert(cameraPlayer.cameraMode() == game::CameraMode::FirstPerson);\n    cameraPlayer.toggleCameraMode();\n    assert(cameraPlayer.cameraMode() == game::CameraMode::ThirdPerson);\n    cameraPlayer.toggleCameraMode();\n    assert(cameraPlayer.cameraMode() == game::CameraMode::FirstPerson);\n\n    const game::Vec3 feet{0.5f, 9.0f, 0.5f};\n",
)
replace_once(
    "tests/TestCharacterRig.cpp",
    "    (void)swingWorld.setBlock(0, 10, 1, world::BlockId::Stone, false);\n    (void)swingWorld.setBlock(0, 10, 2, world::BlockId::Stone, false);\n",
    "    // Reach test: front voxel is farther than the literal arm chain but still comfortably interactive.\n    (void)swingWorld.setBlock(0, 10, 2, world::BlockId::Stone, false);\n    (void)swingWorld.setBlock(0, 10, 3, world::BlockId::Stone, false);\n",
)
replace_once(
    "tests/TestCharacterRig.cpp",
    "    intended.block = {0, 10, 1};\n    intended.adjacent = {0, 10, 0};\n    intended.worldX = 0.5f;\n    intended.worldY = 10.55f;\n    intended.worldZ = 1.0f;\n",
    "    intended.block = {0, 10, 2};\n    intended.adjacent = {0, 10, 1};\n    intended.worldX = 0.5f;\n    intended.worldY = 10.55f;\n    intended.worldZ = 2.0f;\n",
)
replace_once(
    "tests/TestCharacterRig.cpp",
    "            assert((contact->hit.block == world::BlockCoord{0, 10, 1}));\n",
    "            assert((contact->hit.block == world::BlockCoord{0, 10, 2}));\n",
)
replace_once(
    "tests/TestCharacterRig.cpp",
    "    assert(swingWorld.getBlock(0, 10, 2) == world::BlockId::Stone);\n",
    "    assert(swingWorld.getBlock(0, 10, 3) == world::BlockId::Stone);\n",
)

print("RuneForge 0.5.2 camera/reach/visual patch applied")
