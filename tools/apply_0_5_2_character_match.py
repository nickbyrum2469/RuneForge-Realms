from pathlib import Path
import re


def replace_once(path: str, old: str, new: str) -> None:
    p = Path(path)
    text = p.read_text()
    if old not in text:
        raise SystemExit(f"missing marker in {path}: {old[:120]!r}")
    p.write_text(text.replace(old, new, 1))


def replace_regex(path: str, pattern: str, replacement: str) -> None:
    p = Path(path)
    text = p.read_text()
    updated, count = re.subn(pattern, replacement, text, count=1, flags=re.S)
    if count != 1:
        raise SystemExit(f"expected one match in {path}, got {count}: {pattern[:120]!r}")
    p.write_text(updated)


# Reference stance: broad shoulders, relaxed nearly-straight arms beside the thighs, and longer
# chunky feet. Segment lengths remain unchanged; only parent placement / rest targets change.
replace_once(
    "src/game/character/PlayerBodyRig.cpp",
    "    pose.foot = pose.ankle + forward * 0.115f + Vec3{0.0f, 0.015f, 0.0f};\n",
    "    pose.foot = pose.ankle + forward * 0.150f + Vec3{0.0f, 0.015f, 0.0f};\n",
)
replace_once(
    "src/game/character/PlayerBodyRig.cpp",
    "    const Vec3 rightShoulder = pose.chest + pose.right * 0.275f + torsoDirection * 0.020f;\n    const Vec3 leftShoulder = pose.chest - pose.right * 0.275f + torsoDirection * 0.020f;\n\n    const Vec3 rightRestHand = pose.pelvis + pose.right * 0.255f + pose.forward * 0.095f + pose.up * 0.020f;\n    const Vec3 leftRestHand = pose.pelvis - pose.right * 0.255f + pose.forward * 0.085f + pose.up * 0.015f;\n",
    "    const Vec3 rightShoulder = pose.chest + pose.right * 0.295f + torsoDirection * 0.020f;\n    const Vec3 leftShoulder = pose.chest - pose.right * 0.295f + torsoDirection * 0.020f;\n\n    // The reference hero's arms hang naturally beside the upper thighs. The previous rest target\n    // sat above the pelvis and forced a permanent bent-elbow mannequin pose.\n    const Vec3 rightRestHand = pose.pelvis + pose.right * 0.305f + pose.forward * 0.045f - pose.up * 0.075f;\n    const Vec3 leftRestHand = pose.pelvis - pose.right * 0.305f + pose.forward * 0.040f - pose.up * 0.075f;\n",
)
replace_once(
    "src/game/character/PlayerBodyRig.cpp",
    "    pose.rightArm = solveArm(rightShoulder,\n                             rightDesired,\n                             pose.right * 0.72f - pose.up * 0.78f - pose.forward * 0.12f,\n",
    "    pose.rightArm = solveArm(rightShoulder,\n                             rightDesired,\n                             pose.right * 0.58f - pose.up * 0.88f + pose.forward * 0.06f,\n",
)
replace_once(
    "src/game/character/PlayerBodyRig.cpp",
    "    pose.leftArm = solveArm(leftShoulder,\n                            leftDesired,\n                            pose.right * -0.72f - pose.up * 0.78f - pose.forward * 0.12f,\n",
    "    pose.leftArm = solveArm(leftShoulder,\n                            leftDesired,\n                            pose.right * -0.58f - pose.up * 0.88f + pose.forward * 0.06f,\n",
)

# Sculpted V-taper torso with discrete pectoral/abdominal/scapular planes rather than a sparse shell.
torso = r'''void addTorso(world::VoxelMesh& mesh, const PlayerBodyPose& pose, world::SurfaceMaterial material,
              float expansion = 0.0f) {
    constexpr int layers = 11;
    constexpr float pixel = 0.054f;
    for (int layer = 0; layer < layers; ++layer) {
        const float t = static_cast<float>(layer) / static_cast<float>(layers - 1);
        const Vec3 center = pose.pelvis * (1.0f - t) + pose.chest * t;
        int halfWidth = 4;
        if (layer >= 2 && layer <= 4) halfWidth = 3;       // narrow waist
        else if (layer >= 5 && layer <= 7) halfWidth = 4;  // ribs
        else if (layer >= 8) halfWidth = 5;                // broad heroic chest
        const int halfDepth = layer <= 3 ? 2 : (layer <= 7 ? 2 : 3);

        for (int x = -halfWidth; x <= halfWidth; ++x) {
            for (int z = -halfDepth; z <= halfDepth; ++z) {
                const bool surface = std::abs(x) == halfWidth || std::abs(z) == halfDepth ||
                                     layer == 0 || layer == layers - 1;
                if (!surface) continue;
                const Vec3 p = center + pose.right * (static_cast<float>(x) * (pixel + expansion * 0.10f)) +
                               pose.forward * (static_cast<float>(z) * (pixel + expansion * 0.085f));
                addPixel(mesh, p, pixel + expansion, material);
            }
        }
    }

    if (expansion <= 0.0001f && material == world::SurfaceMaterial::CharacterSkin) {
        // Front anatomy: two broad pec shelves tapering into paired ab blocks and obliques.
        const Vec3 pec = pose.pelvis * 0.23f + pose.chest * 0.77f;
        const Vec3 upperAbs = pose.pelvis * 0.43f + pose.chest * 0.57f;
        const Vec3 midAbs = pose.pelvis * 0.58f + pose.chest * 0.42f;
        const Vec3 lowAbs = pose.pelvis * 0.72f + pose.chest * 0.28f;
        for (int side = -1; side <= 1; side += 2) {
            const float s = static_cast<float>(side);
            addPixel(mesh, pec + pose.right * (0.145f * s) + pose.forward * 0.190f, 0.070f, material);
            addPixel(mesh, pec - pose.up * 0.052f + pose.right * (0.105f * s) + pose.forward * 0.184f, 0.063f, material);
            addPixel(mesh, upperAbs + pose.right * (0.064f * s) + pose.forward * 0.157f, 0.055f, material);
            addPixel(mesh, midAbs + pose.right * (0.060f * s) + pose.forward * 0.150f, 0.053f, material);
            addPixel(mesh, lowAbs + pose.right * (0.055f * s) + pose.forward * 0.143f, 0.050f, material);
            addPixel(mesh, midAbs + pose.right * (0.158f * s) + pose.forward * 0.105f, 0.052f, material);

            // Rear scapular/lats are visible in the supplied rear and 3/4 reference views.
            addPixel(mesh, pec + pose.right * (0.160f * s) - pose.forward * 0.184f, 0.064f, material);
            addPixel(mesh, upperAbs + pose.right * (0.135f * s) - pose.forward * 0.155f, 0.056f, material);
        }
        addPixel(mesh, upperAbs - pose.forward * 0.168f, 0.052f, material);
        addPixel(mesh, midAbs - pose.forward * 0.158f, 0.048f, material);
    }
}

'''
replace_regex(
    "src/render/scene/VoxelCharacterBuilder.cpp",
    r"void addTorso\(world::VoxelMesh& mesh,.*?\n\}\n\n(?=void addLimbCluster)",
    torso,
)

bone_cluster = r'''void addBoneCluster(world::VoxelMesh& mesh, Vec3 center, Vec3 axis, const PlayerBodyPose& body,
                    float cubeSize, float radius, world::SurfaceMaterial material) {
    axis = safeDirection({}, axis, body.up);
    Vec3 side = game::normalized(game::cross(body.up, axis));
    if (game::lengthSquared(side) <= 0.000001f) side = body.right;
    Vec3 depth = game::normalized(game::cross(axis, side));
    if (game::lengthSquared(depth) <= 0.000001f) depth = body.forward;

    // Five tightly-overlapping voxels form a dense square-ish cross-section. This keeps the small-
    // cube construction visible without leaving the hollow/beaded silhouette of the old 2x2 corners.
    addPixel(mesh, center, cubeSize * 1.06f, material);
    addPixel(mesh, center + side * radius, cubeSize, material);
    addPixel(mesh, center - side * radius, cubeSize, material);
    addPixel(mesh, center + depth * radius, cubeSize, material);
    addPixel(mesh, center - depth * radius, cubeSize, material);
}

'''
replace_regex(
    "src/render/scene/VoxelCharacterBuilder.cpp",
    r"void addLimbCluster\(world::VoxelMesh& mesh,.*?\n\}\n\n(?=void addHand)",
    bone_cluster,
)

hand = r'''void addHand(world::VoxelMesh& mesh, const PlayerBodyPose& body, const ArmPose& arm,
             world::SurfaceMaterial material, float expansion = 0.0f) {
    const Vec3 handDirection = safeDirection(arm.wrist, arm.hand, body.forward);
    Vec3 side = game::normalized(game::cross(body.up, handDirection));
    if (game::lengthSquared(side) <= 0.000001f) side = body.right;
    Vec3 palmDepth = game::normalized(game::cross(handDirection, side));
    if (game::lengthSquared(palmDepth) <= 0.000001f) palmDepth = body.up;

    const float palm = 0.062f + expansion;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 0; ++y) {
            addPixel(mesh, arm.hand + side * (static_cast<float>(x) * 0.046f) +
                                   palmDepth * (static_cast<float>(y) * 0.040f),
                     palm, material);
        }
    }
    // Four compact finger tips and an offset thumb give the hand the chunky readable reference shape.
    for (int finger = -2; finger <= 1; ++finger) {
        addPixel(mesh, arm.hand + handDirection * 0.072f + side * (static_cast<float>(finger) * 0.034f + 0.017f),
                 0.046f + expansion * 0.55f, material);
    }
    addPixel(mesh, arm.hand + side * 0.092f - palmDepth * 0.025f + handDirection * 0.020f,
             0.050f + expansion * 0.55f, material);
}

'''
replace_regex(
    "src/render/scene/VoxelCharacterBuilder.cpp",
    r"void addHand\(world::VoxelMesh& mesh,.*?\n\}\n\n(?=void addArm)",
    hand,
)

arm = r'''void addArm(world::VoxelMesh& mesh, const PlayerBodyPose& body, const ArmPose& arm,
            world::SurfaceMaterial material, float expansion = 0.0f) {
    const Vec3 upperAxis = safeDirection(arm.shoulder, arm.elbow, -body.up);
    const Vec3 foreAxis = safeDirection(arm.elbow, arm.wrist, -body.up);

    for (int i = 0; i < 8; ++i) {
        const float t = static_cast<float>(i) / 7.0f;
        const Vec3 center = arm.shoulder * (1.0f - t) + arm.elbow * t;
        const float bulge = 1.0f - std::abs(t - 0.48f) * 1.15f;
        addBoneCluster(mesh, center, upperAxis, body,
                       0.057f + expansion * 0.48f,
                       0.041f + std::max(bulge, 0.0f) * 0.011f + expansion * 0.15f,
                       material);
    }
    for (int i = 0; i < 8; ++i) {
        const float t = static_cast<float>(i) / 7.0f;
        const Vec3 center = arm.elbow * (1.0f - t) + arm.wrist * t;
        const float taper = 1.0f - t;
        addBoneCluster(mesh, center, foreAxis, body,
                       0.053f + expansion * 0.46f,
                       0.032f + taper * 0.012f + expansion * 0.13f,
                       material);
    }

    // Deltoid cap and biceps/triceps masses create the broad heroic arm silhouette from the reference.
    addPixel(mesh, arm.shoulder, 0.170f + expansion, material);
    addPixel(mesh, arm.shoulder - body.up * 0.048f, 0.137f + expansion * 0.78f, material);
    addPixel(mesh, arm.shoulder * 0.58f + arm.elbow * 0.42f + body.forward * 0.038f,
             0.083f + expansion * 0.58f, material);
    addPixel(mesh, arm.elbow, 0.116f + expansion * 0.68f, material);
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
    const Vec3 thighAxis = safeDirection(leg.hip, leg.knee, -body.up);
    const Vec3 shinAxis = safeDirection(leg.knee, leg.ankle, -body.up);

    for (int i = 0; i < 9; ++i) {
        const float t = static_cast<float>(i) / 8.0f;
        const Vec3 center = leg.hip * (1.0f - t) + leg.knee * t;
        const float bulge = 1.0f - std::abs(t - 0.38f) * 1.25f;
        addBoneCluster(mesh, center, thighAxis, body,
                       0.064f + expansion * 0.50f,
                       0.049f + std::max(bulge, 0.0f) * 0.013f + expansion * 0.16f,
                       material);
    }
    for (int i = 0; i < 9; ++i) {
        const float t = static_cast<float>(i) / 8.0f;
        const Vec3 center = leg.knee * (1.0f - t) + leg.ankle * t;
        const float calf = 1.0f - std::abs(t - 0.42f) * 1.75f;
        addBoneCluster(mesh, center, shinAxis, body,
                       0.058f + expansion * 0.48f,
                       0.037f + std::max(calf, 0.0f) * 0.014f + expansion * 0.14f,
                       material);
    }

    addPixel(mesh, leg.hip, 0.164f + expansion, material);
    addPixel(mesh, leg.knee, 0.136f + expansion * 0.72f, material);
    addPixel(mesh, leg.knee + body.forward * 0.040f, 0.082f + expansion * 0.55f, material);
    addPixel(mesh, leg.ankle, 0.124f + expansion * 0.66f, material);

    const Vec3 footDirection = safeDirection(leg.ankle, leg.foot, body.forward);
    Vec3 footSide = game::normalized(game::cross(body.up, footDirection));
    if (game::lengthSquared(footSide) <= 0.000001f) footSide = body.right;
    const Vec3 footBase = leg.foot - footDirection * 0.028f;
    addPixel(mesh, footBase, 0.150f + expansion, material);
    addPixel(mesh, leg.foot + footDirection * 0.035f, 0.145f + expansion, material);
    addPixel(mesh, footBase + footSide * 0.063f, 0.112f + expansion * 0.72f, material);
    addPixel(mesh, footBase - footSide * 0.063f, 0.112f + expansion * 0.72f, material);

    const Vec3 toeLine = leg.foot + footDirection * 0.105f;
    for (int x = -2; x <= 2; ++x) {
        const float edgeTaper = 1.0f - std::abs(static_cast<float>(x)) * 0.08f;
        addPixel(mesh, toeLine + footSide * (static_cast<float>(x) * 0.043f),
                 (0.070f * edgeTaper) + expansion * 0.48f, material);
    }
}

'''
replace_regex(
    "src/render/scene/VoxelCharacterBuilder.cpp",
    r"void addLeg\(world::VoxelMesh& mesh,.*?\n\}\n\n(?=void addLoincloth)",
    leg,
)

loincloth = r'''void addLoincloth(world::VoxelMesh& mesh, const PlayerBodyPose& pose) {
    const Vec3 beltCenter = pose.pelvis + pose.up * 0.038f;

    // Dark rope/bead belt: a thin broken line around the waist, matching the reference rather than
    // reading like a permanent armored waistband.
    constexpr float beltPixel = 0.050f;
    for (int x = -4; x <= 4; ++x) {
        const float rx = static_cast<float>(x) * 0.050f;
        addPixel(mesh, localPoint(beltCenter, pose, rx, 0.0f, 0.145f), beltPixel,
                 world::SurfaceMaterial::CharacterLeather);
        addPixel(mesh, localPoint(beltCenter, pose, rx, 0.0f, -0.145f), beltPixel,
                 world::SurfaceMaterial::CharacterLeather);
    }
    for (int z = -2; z <= 2; ++z) {
        const float fz = static_cast<float>(z) * 0.050f;
        addPixel(mesh, localPoint(beltCenter, pose, 0.218f, 0.0f, fz), beltPixel,
                 world::SurfaceMaterial::CharacterLeather);
        addPixel(mesh, localPoint(beltCenter, pose, -0.218f, 0.0f, fz), beltPixel,
                 world::SurfaceMaterial::CharacterLeather);
    }

    constexpr float clothPixel = 0.058f;
    const std::array<int, 7> frontWidths{{2, 2, 2, 2, 1, 1, 0}};
    for (int row = 0; row < static_cast<int>(frontWidths.size()); ++row) {
        const float y = -0.052f - static_cast<float>(row) * 0.052f;
        for (int x = -frontWidths[static_cast<std::size_t>(row)];
             x <= frontWidths[static_cast<std::size_t>(row)]; ++x) {
            addPixel(mesh, localPoint(beltCenter, pose, static_cast<float>(x) * 0.052f, y, 0.166f),
                     clothPixel, world::SurfaceMaterial::CharacterLoincloth);
        }
    }
    const std::array<int, 6> backWidths{{2, 2, 2, 1, 1, 0}};
    for (int row = 0; row < static_cast<int>(backWidths.size()); ++row) {
        const float y = -0.052f - static_cast<float>(row) * 0.052f;
        for (int x = -backWidths[static_cast<std::size_t>(row)];
             x <= backWidths[static_cast<std::size_t>(row)]; ++x) {
            addPixel(mesh, localPoint(beltCenter, pose, static_cast<float>(x) * 0.052f, y, -0.164f),
                     clothPixel, world::SurfaceMaterial::CharacterLoincloth);
        }
    }
    for (int side = -1; side <= 1; side += 2) {
        addPixel(mesh, localPoint(beltCenter, pose, static_cast<float>(side) * 0.198f, -0.060f, 0.0f),
                 0.056f, world::SurfaceMaterial::CharacterLoincloth);
        addPixel(mesh, localPoint(beltCenter, pose, static_cast<float>(side) * 0.190f, -0.112f, 0.0f),
                 0.052f, world::SurfaceMaterial::CharacterLoincloth);
    }
}

'''
replace_regex(
    "src/render/scene/VoxelCharacterBuilder.cpp",
    r"void addLoincloth\(world::VoxelMesh& mesh,.*?\n\}\n\n(?=void addHead)",
    loincloth,
)

head = r'''void addHead(world::VoxelMesh& mesh, const PlayerBodyPose& pose) {
    const Vec3 center = pose.head;
    constexpr float skinPixel = 0.058f;

    // Broad square/cute hero head. The bottom corners taper slightly into a jaw while the upper face
    // stays wide enough for the reference's large bright eyes and thick hair silhouette.
    for (int y = -2; y <= 2; ++y) {
        for (int x = -3; x <= 3; ++x) {
            for (int z = -2; z <= 2; ++z) {
                if (y == -2 && std::abs(x) == 3) continue;
                const bool surface = std::abs(x) == 3 || std::abs(y) == 2 || std::abs(z) == 2;
                if (!surface) continue;
                addPixel(mesh, localPoint(center, pose, static_cast<float>(x) * 0.057f,
                                          static_cast<float>(y) * 0.060f,
                                          static_cast<float>(z) * 0.061f),
                         skinPixel, world::SurfaceMaterial::CharacterSkin);
            }
        }
    }
    // Jaw/chin and ears keep profile/rear views from reading as a featureless cube.
    addPixel(mesh, localPoint(center, pose, 0.0f, -0.151f, 0.055f), 0.076f,
             world::SurfaceMaterial::CharacterSkin);
    for (int side = -1; side <= 1; side += 2) {
        const float s = static_cast<float>(side);
        addPixel(mesh, localPoint(center, pose, s * 0.205f, -0.005f, 0.0f), 0.064f,
                 world::SurfaceMaterial::CharacterSkin);
        addPixel(mesh, localPoint(center, pose, s * 0.155f, -0.105f, 0.125f), 0.062f,
                 world::SurfaceMaterial::CharacterSkin);
    }

    // Thick layered dark-brown hair: wide crown, raised second crown, full rear mass, side locks and
    // an irregular forward fringe. This is deliberately much denser than the old flat cap.
    constexpr float hairPixel = 0.057f;
    for (int x = -4; x <= 4; ++x) {
        for (int z = -3; z <= 2; ++z) {
            if (std::abs(x) == 4 && (z == 2 || z == -3)) continue;
            const int jitter = (x * 17 + z * 29 + 97) & 1;
            addPixel(mesh, localPoint(center, pose, static_cast<float>(x) * 0.052f,
                                      0.164f + static_cast<float>(jitter) * 0.016f,
                                      static_cast<float>(z) * 0.054f),
                     hairPixel, world::SurfaceMaterial::CharacterHair);
        }
    }
    for (int x = -3; x <= 3; ++x) {
        for (int z = -2; z <= 1; ++z) {
            if (std::abs(x) == 3 && z == 1) continue;
            addPixel(mesh, localPoint(center, pose, static_cast<float>(x) * 0.052f,
                                      0.215f + static_cast<float>((x + z + 9) & 1) * 0.012f,
                                      static_cast<float>(z) * 0.053f - 0.012f),
                     0.055f, world::SurfaceMaterial::CharacterHair);
        }
    }
    for (int y = -2; y <= 3; ++y) {
        for (int x = -4; x <= 4; ++x) {
            if (std::abs(x) == 4 && y < 0) continue;
            addPixel(mesh, localPoint(center, pose, static_cast<float>(x) * 0.052f,
                                      static_cast<float>(y) * 0.054f,
                                      -0.181f),
                     hairPixel, world::SurfaceMaterial::CharacterHair);
        }
    }
    for (int side = -1; side <= 1; side += 2) {
        const float s = static_cast<float>(side);
        for (int y = -2; y <= 3; ++y) {
            for (int z = -2; z <= 1; ++z) {
                addPixel(mesh, localPoint(center, pose, s * 0.211f,
                                          static_cast<float>(y) * 0.054f,
                                          static_cast<float>(z) * 0.054f - 0.015f),
                         0.056f, world::SurfaceMaterial::CharacterHair);
            }
        }
        addPixel(mesh, localPoint(center, pose, s * 0.185f, -0.125f, -0.015f), 0.058f,
                 world::SurfaceMaterial::CharacterHair);
    }

    const std::array<float, 9> fringeHeights{{0.112f, 0.142f, 0.102f, 0.154f, 0.126f,
                                               0.151f, 0.098f, 0.137f, 0.108f}};
    for (int x = -4; x <= 4; ++x) {
        addPixel(mesh, localPoint(center, pose, static_cast<float>(x) * 0.049f,
                                  fringeHeights[static_cast<std::size_t>(x + 4)], 0.171f),
                 0.056f, world::SurfaceMaterial::CharacterHair);
    }

    // Large blue eyes are the most important facial read in the reference. White, iris and tiny dark
    // pupils are separate voxel layers so the expression survives normal third-person distance.
    const Vec3 face = center + pose.forward * 0.158f + pose.up * 0.025f;
    for (int side = -1; side <= 1; side += 2) {
        const float s = static_cast<float>(side);
        const Vec3 eye = face + pose.right * (s * 0.093f);
        addPixel(mesh, eye, 0.067f, world::SurfaceMaterial::CharacterEyeWhite);
        addPixel(mesh, eye + pose.forward * 0.037f, 0.041f, world::SurfaceMaterial::CharacterEyeBlue);
        addPixel(mesh, eye + pose.forward * 0.061f - pose.right * (s * 0.004f), 0.018f,
                 world::SurfaceMaterial::CharacterHair);
        addPixel(mesh, eye + pose.up * 0.068f + pose.forward * 0.025f, 0.047f,
                 world::SurfaceMaterial::CharacterHair);
    }
    addPixel(mesh, center + pose.forward * 0.177f - pose.up * 0.027f, 0.043f,
             world::SurfaceMaterial::CharacterSkin);
    addPixel(mesh, center + pose.forward * 0.180f - pose.up * 0.093f - pose.right * 0.021f, 0.024f,
             world::SurfaceMaterial::CharacterLeather);
    addPixel(mesh, center + pose.forward * 0.180f - pose.up * 0.093f + pose.right * 0.021f, 0.024f,
             world::SurfaceMaterial::CharacterLeather);
}

'''
replace_regex(
    "src/render/scene/VoxelCharacterBuilder.cpp",
    r"void addHead\(world::VoxelMesh& mesh,.*?\n\}\n\n(?=void addHelmet)",
    head,
)

# Regression anchors for the reference stance; these complement the existing fixed-length tests.
replace_once(
    "tests/TestCharacterRig.cpp",
    "    assertFixedLeg(body.rightLeg);\n    assertFixedLeg(body.leftLeg);\n\n    const game::Vec3 impossibleHand",
    "    assertFixedLeg(body.rightLeg);\n    assertFixedLeg(body.leftLeg);\n    assert(body.rightArm.hand.y < body.pelvis.y - 0.035f);\n    assert(body.leftArm.hand.y < body.pelvis.y - 0.035f);\n    assert(distance(body.rightArm.shoulder, body.leftArm.shoulder) > 0.58f);\n    assert(game::dot(body.rightLeg.foot - body.rightLeg.ankle, body.forward) > 0.145f);\n    assert(game::dot(body.leftLeg.foot - body.leftLeg.ankle, body.forward) > 0.145f);\n\n    const game::Vec3 impossibleHand",
)
replace_once(
    "tests/TestCharacterRig.cpp",
    "    assert(!hasMaterial(baseMesh, world::SurfaceMaterial::CharacterMetal));\n",
    "    assert(!hasMaterial(baseMesh, world::SurfaceMaterial::CharacterMetal));\n    // The reference-driven base uses substantially denser small-voxel sculpting than the old sparse mannequin.\n    assert(baseMesh.quadCount > 2200);\n",
)

print("RuneForge reference-driven character accuracy pass applied")
