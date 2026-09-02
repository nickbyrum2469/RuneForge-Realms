#ifdef _WIN32

#include "render/vulkan/VulkanRenderer.h"

#include "game/character/CharacterAppearance.h"
#include "game/character/CharacterLocomotion.h"
#include "game/character/PlayerBodyRig.h"
#include "render/scene/CharacterVoxelOrientation.h"
#include "render/scene/FirstPersonBodyBuilder.h"
#include "render/scene/VoxelCharacterBuilder.h"

#include <algorithm>
#include <chrono>

namespace rf::render {

bool VulkanRenderer::updateFirstPersonBodyMesh() {
    const auto feet = player_.position();
    const auto eye = player_.eyePosition();
    game::Vec3 cameraForward = player_.lookDirection();
    cameraForward = game::normalized(cameraForward);
    if (game::lengthSquared(cameraForward) <= 0.000001f) cameraForward = {0.0f, 0.0f, 1.0f};

    // Character/viewmodel orientation derives from the exact camera direction used by the world
    // renderer. Do not reconstruct it independently from yaw with a second sign convention.
    game::Vec3 cameraRight = game::normalized({cameraForward.z, 0.0f, -cameraForward.x});
    if (game::lengthSquared(cameraRight) <= 0.000001f) cameraRight = {1.0f, 0.0f, 0.0f};
    game::Vec3 cameraUp = game::normalized(game::cross(cameraForward, cameraRight));
    if (game::lengthSquared(cameraUp) <= 0.000001f) cameraUp = {0.0f, 1.0f, 0.0f};

    const float elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - startTime_).count();

    world::VoxelMesh mesh;
    if (player_.thirdPerson()) {
        game::Vec3 bodyForward{cameraForward.x, 0.0f, cameraForward.z};
        bodyForward = game::normalized(bodyForward);
        if (game::lengthSquared(bodyForward) <= 0.000001f) bodyForward = {0.0f, 0.0f, 1.0f};

        game::character::BodyMotionState motion;
        const float speed = player_.actualHorizontalSpeed();
        motion.locomotionAmount = std::clamp(speed / 4.8f, 0.0f, 1.0f);
        // Drive cadence from collision-resolved distance instead of wall-clock time. The calibrated
        // stride now completes once per ~1.31 m of actual travel, which keeps the planted phase much
        // closer to world motion and removes the slow 3.31 m moonwalk cycle from the previous pass.
        motion.locomotionPhase = game::character::locomotionPhaseFromDistance(player_.horizontalTravelDistance());
        motion.idlePhase = elapsed * 1.75f;

        // MiningSwing currently owns an exact fixed-length world-space right-arm pose. Freeze the
        // gait only during its short active window so the shoulder cannot visually detach from a
        // bobbing torso; normal movement immediately resumes after recovery.
        if (miningSwing_.pose().active) motion.locomotionAmount = 0.0f;

        auto bodyPose = game::character::PlayerBodyRig::solve(feet,
                                                               bodyForward,
                                                               player_.crouching(),
                                                               nullptr,
                                                               nullptr,
                                                               &motion);
        if (miningSwing_.pose().active) bodyPose.rightArm = miningSwing_.pose().rightArm;

        const game::character::CharacterAppearance appearance{};
        mesh = scene::VoxelCharacterBuilder::build(bodyPose, appearance);

        // Voxel centers already follow the articulated character pose, but VoxelCharacterBuilder's
        // individual micro-cubes are emitted on the world XYZ basis. Rotate each cube around its own
        // center into the same actor-local basis so 0/45/90-degree yaw all preserve the same clean
        // body silhouette instead of turning the hero's pixels into world-aligned diamonds.
        scene::orientCharacterVoxels(mesh, bodyPose);
    } else {
        scene::FirstPersonViewModelState state;
        state.eye = eye;
        state.forward = cameraForward;
        state.right = cameraRight;
        state.up = cameraUp;

        const float speed = player_.actualHorizontalSpeed();
        state.walkAmount = std::clamp(speed / 4.8f, 0.0f, 1.0f);
        state.walkPhase = game::character::locomotionPhaseFromDistance(player_.horizontalTravelDistance());

        const auto& swing = miningSwing_.pose();
        state.swingActive = swing.active;
        state.swingTime = swing.normalizedTime;
        state.targetDistance = swing.targetDistance > 0.001f ? swing.targetDistance
                                                             : game::interaction::MiningSwing::interactionReach;
        state.equippedBlock = selectedPlacementBlock();

        // Unlike 0.5.2, first person is never empty at rest. Bare-handed play keeps both hands subtly
        // visible at the bottom edge; equipping a hotbar item shows only the dominant hand plus item.
        mesh = scene::FirstPersonBodyBuilder::build(state);
    }

    if (mesh.empty()) {
        firstPersonIndexCount_ = 0;
        return true;
    }

    const VkDeviceSize vertexBytes = static_cast<VkDeviceSize>(mesh.vertices.size() * sizeof(world::MeshVertex));
    const VkDeviceSize indexBytes = static_cast<VkDeviceSize>(mesh.indices.size() * sizeof(std::uint32_t));
    if (!ensureDynamicBuffer(firstPersonVertices_, vertexBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) ||
        !ensureDynamicBuffer(firstPersonIndices_, indexBytes, VK_BUFFER_USAGE_INDEX_BUFFER_BIT) ||
        !writeDynamicBuffer(firstPersonVertices_, mesh.vertices.data(), vertexBytes) ||
        !writeDynamicBuffer(firstPersonIndices_, mesh.indices.data(), indexBytes)) {
        setError(L"RuneForge could not update player body geometry.");
        return false;
    }
    firstPersonIndexCount_ = static_cast<std::uint32_t>(mesh.indices.size());
    return true;
}

void VulkanRenderer::drawFirstPersonBody(VkCommandBuffer commandBuffer) {
    if (firstPersonIndexCount_ == 0 || firstPersonVertices_.buffer == VK_NULL_HANDLE ||
        firstPersonIndices_.buffer == VK_NULL_HANDLE) return;
    const VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &firstPersonVertices_.buffer, &offset);
    vkCmdBindIndexBuffer(commandBuffer, firstPersonIndices_.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(commandBuffer, firstPersonIndexCount_, 1, 0, 0, 0);
}

void VulkanRenderer::destroyFirstPersonBodyMesh() {
    destroyBuffer(firstPersonIndices_);
    destroyBuffer(firstPersonVertices_);
    firstPersonIndexCount_ = 0;
}

} // namespace rf::render

#endif