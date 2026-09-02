#ifdef _WIN32

#include "render/vulkan/VulkanRenderer.h"

#include "game/character/CharacterAppearance.h"
#include "game/character/PlayerBodyRig.h"
#include "render/scene/FirstPersonBodyBuilder.h"

namespace rf::render {

bool VulkanRenderer::updateFirstPersonBodyMesh() {
    const auto feet = player_.position();
    const game::Vec3 bodyForward = game::horizontalForward(player_.yaw());
    auto bodyPose = game::character::PlayerBodyRig::solve(feet, bodyForward, player_.crouching());

    // MiningSwing owns the physical right-arm animation. Rendering copies that exact solved chain
    // into the full body so visible fist position and collision sweep can never drift apart.
    if (miningSwing_.pose().active) bodyPose.rightArm = miningSwing_.pose().rightArm;

    const game::character::CharacterAppearance appearance{};
    const world::VoxelMesh mesh = scene::FirstPersonBodyBuilder::build(bodyPose, appearance);
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
        setError(L"RuneForge could not update first-person body geometry.");
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
