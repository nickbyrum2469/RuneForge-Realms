#ifdef _WIN32

#include "render/vulkan/VulkanRenderer.h"

#include "render/scene/DropMeshBuilder.h"

#include <algorithm>
#include <cstring>

namespace rf::render {

bool VulkanRenderer::ensureDynamicBuffer(BufferResource& resource, VkDeviceSize requiredSize,
                                         VkBufferUsageFlags usage) {
    if (requiredSize == 0) return true;
    if (resource.buffer != VK_NULL_HANDLE && resource.size >= requiredSize) return true;

    const VkDeviceSize oldSize = resource.size;
    destroyBuffer(resource);
    const VkDeviceSize grown = std::max<VkDeviceSize>(requiredSize, std::max<VkDeviceSize>(4096, oldSize * 2));
    return createBuffer(grown, usage,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        resource);
}

bool VulkanRenderer::writeDynamicBuffer(BufferResource& resource, const void* data, VkDeviceSize size) {
    if (size == 0) return true;
    if (resource.buffer == VK_NULL_HANDLE || resource.memory == VK_NULL_HANDLE || size > resource.size) return false;
    void* mapped = nullptr;
    if (vkMapMemory(device_, resource.memory, 0, size, 0, &mapped) != VK_SUCCESS) return false;
    std::memcpy(mapped, data, static_cast<std::size_t>(size));
    vkUnmapMemory(device_, resource.memory);
    return true;
}

bool VulkanRenderer::updateDropMesh() {
    const world::VoxelMesh mesh = scene::DropMeshBuilder::build(drops_.drops());
    if (mesh.empty()) {
        dropIndexCount_ = 0;
        return true;
    }

    const VkDeviceSize vertexBytes = static_cast<VkDeviceSize>(mesh.vertices.size() * sizeof(world::MeshVertex));
    const VkDeviceSize indexBytes = static_cast<VkDeviceSize>(mesh.indices.size() * sizeof(std::uint32_t));
    if (!ensureDynamicBuffer(dropVertices_, vertexBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) ||
        !ensureDynamicBuffer(dropIndices_, indexBytes, VK_BUFFER_USAGE_INDEX_BUFFER_BIT) ||
        !writeDynamicBuffer(dropVertices_, mesh.vertices.data(), vertexBytes) ||
        !writeDynamicBuffer(dropIndices_, mesh.indices.data(), indexBytes)) {
        setError(L"RuneForge could not update collectible world-drop geometry.");
        return false;
    }

    dropIndexCount_ = static_cast<std::uint32_t>(mesh.indices.size());
    return true;
}

void VulkanRenderer::drawWorldDrops(VkCommandBuffer commandBuffer) {
    if (dropIndexCount_ == 0 || dropVertices_.buffer == VK_NULL_HANDLE || dropIndices_.buffer == VK_NULL_HANDLE) return;
    const VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &dropVertices_.buffer, &offset);
    vkCmdBindIndexBuffer(commandBuffer, dropIndices_.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(commandBuffer, dropIndexCount_, 1, 0, 0, 0);
}

void VulkanRenderer::destroyDropMesh() {
    destroyBuffer(dropIndices_);
    destroyBuffer(dropVertices_);
    dropIndexCount_ = 0;
}

} // namespace rf::render

#endif
