#ifdef _WIN32

#include "render/vulkan/VulkanRenderer.h"

#include "render/scene/ParticleMeshBuilder.h"

namespace rf::render {

bool VulkanRenderer::updateParticleMesh() {
    const world::VoxelMesh mesh = scene::ParticleMeshBuilder::build(particles_.particles());
    if (mesh.empty()) {
        particleIndexCount_ = 0;
        return true;
    }

    const VkDeviceSize vertexBytes = static_cast<VkDeviceSize>(mesh.vertices.size() * sizeof(world::MeshVertex));
    const VkDeviceSize indexBytes = static_cast<VkDeviceSize>(mesh.indices.size() * sizeof(std::uint32_t));
    if (!ensureDynamicBuffer(particleVertices_, vertexBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) ||
        !ensureDynamicBuffer(particleIndices_, indexBytes, VK_BUFFER_USAGE_INDEX_BUFFER_BIT) ||
        !writeDynamicBuffer(particleVertices_, mesh.vertices.data(), vertexBytes) ||
        !writeDynamicBuffer(particleIndices_, mesh.indices.data(), indexBytes)) {
        setError(L"RuneForge could not update block particle geometry.");
        return false;
    }
    particleIndexCount_ = static_cast<std::uint32_t>(mesh.indices.size());
    return true;
}

void VulkanRenderer::drawBlockParticles(VkCommandBuffer commandBuffer) {
    if (particleIndexCount_ == 0 || particleVertices_.buffer == VK_NULL_HANDLE ||
        particleIndices_.buffer == VK_NULL_HANDLE) return;
    const VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &particleVertices_.buffer, &offset);
    vkCmdBindIndexBuffer(commandBuffer, particleIndices_.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(commandBuffer, particleIndexCount_, 1, 0, 0, 0);
}

void VulkanRenderer::destroyParticleMesh() {
    destroyBuffer(particleIndices_);
    destroyBuffer(particleVertices_);
    particleIndexCount_ = 0;
}

} // namespace rf::render

#endif
