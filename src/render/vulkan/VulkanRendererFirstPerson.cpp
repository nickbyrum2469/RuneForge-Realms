#ifdef _WIN32

#include "render/vulkan/VulkanRenderer.h"

#include "game/character/CharacterAppearance.h"
#include "game/character/PlayerBodyRig.h"
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

    world::VoxelMesh mesh;
    if (player_.thirdPerson()) {
        game::Vec3 bodyForward{cameraForward.x, 0.0f, cameraForward.z};
        bodyForward = game::normalized(bodyForward);
        if (game::lengthSquared(bodyForward) <= 0.000001f) bodyForward = {0.0f, 0.0f, 1.0f};

        auto bodyPose = game::character::PlayerBodyRig::solve(feet, bodyForward, player_.crouching());
        if (miningSwing_.pose().active) bodyPose.rightArm = miningSwing_.pose().rightArm;

        const game::character::CharacterAppearance appearance{};
        mesh = scene::VoxelCharacterBuilder::build(bodyPose, appearance);
    } else {
        scene::FirstPersonViewModelState state;
        state.eye = eye;
        state.forward = cameraForward;
        state.right = cameraRight;
        state.up = cameraUp;

        const float speed = player_.horizontalSpeed();
        state.walkAmount = std::clamp(speed / 4.8f, 0.0f, 1.0f);
        const float elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - startTime_).count();
        state.walkPhase = elapsed * (7.6f + state.walkAmount * 2.2f);

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
