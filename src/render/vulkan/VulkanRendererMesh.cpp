#ifdef _WIN32

#include "render/vulkan/VulkanRenderer.h"

#include <cstring>
#include <limits>
#include <set>

namespace rf::render {

bool VulkanRenderer::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                  VkMemoryPropertyFlags properties, BufferResource& output) {
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    info.size = size;
    info.usage = usage;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device_, &info, nullptr, &output.buffer) != VK_SUCCESS) return false;

    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device_, output.buffer, &requirements);
    const auto memoryType = findMemoryType(requirements.memoryTypeBits, properties);
    if (memoryType == std::numeric_limits<std::uint32_t>::max()) return false;

    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = memoryType;
    if (vkAllocateMemory(device_, &allocation, nullptr, &output.memory) != VK_SUCCESS) return false;
    if (vkBindBufferMemory(device_, output.buffer, output.memory, 0) != VK_SUCCESS) return false;
    output.size = size;
    return true;
}

void VulkanRenderer::destroyBuffer(BufferResource& resource) {
    if (device_ == VK_NULL_HANDLE) { resource = {}; return; }
    if (resource.buffer != VK_NULL_HANDLE) vkDestroyBuffer(device_, resource.buffer, nullptr);
    if (resource.memory != VK_NULL_HANDLE) vkFreeMemory(device_, resource.memory, nullptr);
    resource = {};
}

bool VulkanRenderer::copyBuffer(VkBuffer source, VkBuffer destination, VkDeviceSize size) {
    VkCommandBufferAllocateInfo allocation{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocation.commandPool = commandPool_;
    allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocation.commandBufferCount = 1;
    VkCommandBuffer command = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(device_, &allocation, &command) != VK_SUCCESS) return false;

    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(command, &begin);
    const VkBufferCopy copy{0, 0, size};
    vkCmdCopyBuffer(command, source, destination, 1, &copy);
    vkEndCommandBuffer(command);

    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &command;
    const bool ok = vkQueueSubmit(graphicsQueue_, 1, &submit, VK_NULL_HANDLE) == VK_SUCCESS &&
                    vkQueueWaitIdle(graphicsQueue_) == VK_SUCCESS;
    vkFreeCommandBuffers(device_, commandPool_, 1, &command);
    return ok;
}

bool VulkanRenderer::uploadDeviceLocal(const void* data, VkDeviceSize size,
                                       VkBufferUsageFlags finalUsage, BufferResource& output) {
    BufferResource staging;
    if (!createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging)) return false;

    void* mapped = nullptr;
    if (vkMapMemory(device_, staging.memory, 0, size, 0, &mapped) != VK_SUCCESS) {
        destroyBuffer(staging);
        return false;
    }
    std::memcpy(mapped, data, static_cast<std::size_t>(size));
    vkUnmapMemory(device_, staging.memory);

    if (!createBuffer(size, finalUsage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, output)) {
        destroyBuffer(staging);
        return false;
    }
    const bool ok = copyBuffer(staging.buffer, output.buffer, size);
    destroyBuffer(staging);
    if (!ok) destroyBuffer(output);
    return ok;
}

void VulkanRenderer::destroyChunkMesh(GpuChunkMesh& mesh) {
    destroyBuffer(mesh.indices);
    destroyBuffer(mesh.vertices);
    mesh = {};
}

bool VulkanRenderer::uploadChunkMesh(world::ChunkCoord coord) {
    const world::VoxelMesh mesh = world_.buildChunkMesh(coord);
    auto existing = chunkMeshes_.find(coord);
    if (existing != chunkMeshes_.end()) {
        destroyChunkMesh(existing->second);
        chunkMeshes_.erase(existing);
    }

    if (mesh.empty()) {
        world_.markChunkMeshReady(coord);
        return true;
    }

    GpuChunkMesh gpu;
    const VkDeviceSize vertexBytes = static_cast<VkDeviceSize>(mesh.vertices.size() * sizeof(world::MeshVertex));
    const VkDeviceSize indexBytes = static_cast<VkDeviceSize>(mesh.indices.size() * sizeof(std::uint32_t));
    if (!uploadDeviceLocal(mesh.vertices.data(), vertexBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, gpu.vertices) ||
        !uploadDeviceLocal(mesh.indices.data(), indexBytes, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, gpu.indices)) {
        destroyChunkMesh(gpu);
        setError(L"RuneForge could not upload a Frontier chunk mesh to GPU memory.");
        return false;
    }
    gpu.indexCount = static_cast<std::uint32_t>(mesh.indices.size());
    gpu.quadCount = mesh.quadCount;
    chunkMeshes_.emplace(coord, std::move(gpu));
    world_.markChunkMeshReady(coord);
    return true;
}

bool VulkanRenderer::syncChunkMeshes() {
    const auto loadedCoords = world_.loadedChunkCoords();
    const std::set<world::ChunkCoord> loaded(loadedCoords.begin(), loadedCoords.end());

    for (auto it = chunkMeshes_.begin(); it != chunkMeshes_.end();) {
        if (!loaded.contains(it->first)) {
            destroyChunkMesh(it->second);
            it = chunkMeshes_.erase(it);
        } else {
            ++it;
        }
    }

    const auto dirtyCoords = world_.dirtyChunkCoords();
    const std::set<world::ChunkCoord> dirty(dirtyCoords.begin(), dirtyCoords.end());
    for (const auto coord : loadedCoords) {
        if (!chunkMeshes_.contains(coord) || dirty.contains(coord)) {
            if (!uploadChunkMesh(coord)) return false;
        }
    }

    sceneQuadCount_ = 0;
    for (const auto& [coord, mesh] : chunkMeshes_) {
        (void)coord;
        sceneQuadCount_ += mesh.quadCount;
    }
    sceneBlockCount_ = static_cast<std::uint32_t>(world_.solidBlockCount());
    worldMeshDirty_ = false;
    return true;
}

bool VulkanRenderer::createSceneMesh() {
    if (!syncChunkMeshes()) return false;
    if (chunkMeshes_.empty()) {
        setError(L"Frontier world generation produced no renderable chunk meshes.");
        return false;
    }
    return true;
}

bool VulkanRenderer::rebuildSceneMesh() {
    if (device_ == VK_NULL_HANDLE) return false;
    return syncChunkMeshes();
}

void VulkanRenderer::destroySceneMesh() {
    for (auto& [coord, mesh] : chunkMeshes_) {
        (void)coord;
        destroyChunkMesh(mesh);
    }
    chunkMeshes_.clear();
    sceneQuadCount_ = 0;
    sceneBlockCount_ = 0;
    visibleChunkCount_ = 0;
}

} // namespace rf::render

#endif
