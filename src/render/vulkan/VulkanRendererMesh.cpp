#ifdef _WIN32

#include "render/vulkan/VulkanRenderer.h"

#include "render/scene/ChunkCulling.h"
#include "render/scene/ChunkMeshScheduling.h"
#include "world/meshing/MicroDetailBuilder.h"
#include "world/meshing/MicroVoxelMesher.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>

namespace rf::render {
namespace {

void attachDamageVisuals(world::ChunkMeshingSnapshot& snapshot,
                         const std::vector<game::mining::MiningDamageState>& states,
                         world::ChunkCoord meshCoord) {
    snapshot.damageBlocks.clear();
    for (const auto& state : states) {
        const auto damageChunk = world::chunkFromBlock(state.position.x, state.position.z);
        if (world::chebyshevDistance(damageChunk, meshCoord) > 1) continue;
        const auto stage = static_cast<std::uint8_t>(
            std::clamp(static_cast<int>(std::ceil(std::clamp(state.progress, 0.0f, 1.0f) * 5.0f)), 1, 5));
        snapshot.damageBlocks.push_back({state.position, stage});
    }
}

void splitIndicesByMaterial(const world::VoxelMesh& mesh,
                            std::vector<std::uint32_t>& opaque,
                            std::vector<std::uint32_t>& water) {
    opaque.clear();
    water.clear();
    opaque.reserve(mesh.indices.size());
    water.reserve(mesh.indices.size() / 8);
    for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const std::uint32_t i0 = mesh.indices[i];
        const auto material = static_cast<world::SurfaceMaterial>(
            mesh.vertices[i0].material & world::surfaceMaterialMask);
        auto& destination = material == world::SurfaceMaterial::Water ? water : opaque;
        destination.push_back(mesh.indices[i]);
        destination.push_back(mesh.indices[i + 1]);
        destination.push_back(mesh.indices[i + 2]);
    }
}

} // namespace

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
    if (size == 0) return true;
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
    destroyBuffer(mesh.waterIndices);
    destroyBuffer(mesh.opaqueIndices);
    destroyBuffer(mesh.vertices);
    mesh = {};
}

world::meshing::SurfaceDetailTier VulkanRenderer::surfaceDetailTierFor(world::ChunkCoord coord) const noexcept {
    const auto position = player_.position();
    const world::ChunkCoord playerChunk = world::chunkFromWorld(position.x, position.z);
    const int distance = world::chebyshevDistance(coord, playerChunk);

    if (settings_.foliageQuality <= 0) {
        return distance <= 2 ? world::meshing::SurfaceDetailTier::Standard
                             : world::meshing::SurfaceDetailTier::Distant;
    }
    if (settings_.foliageQuality == 1) {
        if (distance <= 1) return world::meshing::SurfaceDetailTier::Hero;
        if (distance <= 3) return world::meshing::SurfaceDetailTier::Standard;
        return world::meshing::SurfaceDetailTier::Distant;
    }
    if (distance <= kHeroDetailRadius) return world::meshing::SurfaceDetailTier::Hero;
    if (distance <= kStandardDetailRadius) return world::meshing::SurfaceDetailTier::Standard;
    return world::meshing::SurfaceDetailTier::Distant;
}

bool VulkanRenderer::uploadChunkMesh(world::ChunkCoord coord, std::uint64_t revision,
                                     world::meshing::SurfaceDetailTier detailTier,
                                     const world::VoxelMesh& mesh, std::uint32_t solidBlockCount) {
    if (mesh.empty()) {
        if (auto existing = chunkMeshes_.find(coord); existing != chunkMeshes_.end()) {
            destroyChunkMesh(existing->second);
            chunkMeshes_.erase(existing);
        }
        refreshSceneCounters();
        return true;
    }

    std::vector<std::uint32_t> opaqueIndices;
    std::vector<std::uint32_t> waterIndices;
    splitIndicesByMaterial(mesh, opaqueIndices, waterIndices);

    GpuChunkMesh replacement;
    const VkDeviceSize vertexBytes = static_cast<VkDeviceSize>(mesh.vertices.size() * sizeof(world::MeshVertex));
    if (!uploadDeviceLocal(mesh.vertices.data(), vertexBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, replacement.vertices)) {
        destroyChunkMesh(replacement);
        setError(L"RuneForge could not upload streamed Frontier vertex data to GPU memory.");
        return false;
    }

    if (!opaqueIndices.empty()) {
        const VkDeviceSize bytes = static_cast<VkDeviceSize>(opaqueIndices.size() * sizeof(std::uint32_t));
        if (!uploadDeviceLocal(opaqueIndices.data(), bytes, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, replacement.opaqueIndices)) {
            destroyChunkMesh(replacement);
            setError(L"RuneForge could not upload opaque Frontier indices to GPU memory.");
            return false;
        }
    }
    if (!waterIndices.empty()) {
        const VkDeviceSize bytes = static_cast<VkDeviceSize>(waterIndices.size() * sizeof(std::uint32_t));
        if (!uploadDeviceLocal(waterIndices.data(), bytes, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, replacement.waterIndices)) {
            destroyChunkMesh(replacement);
            setError(L"RuneForge could not upload transparent water indices to GPU memory.");
            return false;
        }
    }

    replacement.opaqueIndexCount = static_cast<std::uint32_t>(opaqueIndices.size());
    replacement.waterIndexCount = static_cast<std::uint32_t>(waterIndices.size());
    replacement.quadCount = mesh.quadCount;
    replacement.solidBlockCount = solidBlockCount;
    replacement.revision = revision;
    replacement.detailTier = detailTier;

    auto existing = chunkMeshes_.find(coord);
    if (existing != chunkMeshes_.end()) destroyChunkMesh(existing->second);
    chunkMeshes_.insert_or_assign(coord, std::move(replacement));
    refreshSceneCounters();
    return true;
}

bool VulkanRenderer::createSceneMesh() {
    const auto playerPosition = player_.position();
    const world::ChunkCoord playerChunk = world::chunkFromWorld(playerPosition.x, playerPosition.z);
    const auto damageStates = mining_.damageStates();

    for (const world::ChunkCoord coord : world_.dirtyChunkCoords()) {
        if (world::chebyshevDistance(coord, playerChunk) > kStartupGpuRadius) continue;
        const auto source = world_.chunkMeshingSnapshot(coord);
        if (!source) continue;
        auto snapshot = *source;
        attachDamageVisuals(snapshot, damageStates, coord);

        const auto detailTier = surfaceDetailTierFor(coord);
        world::VoxelMesh local = world::GreedyMesher::build(snapshot);
        world::meshing::MicroVoxelMesher::append(snapshot, local);
        world::meshing::MicroDetailBuilder::append(snapshot, local, detailTier);
        world::VoxelMesh translated;
        translated.append(local, static_cast<float>(coord.x * world::VoxelChunk::sizeX), 0.0f,
                          static_cast<float>(coord.z * world::VoxelChunk::sizeZ));
        if (!uploadChunkMesh(coord, world_.chunkRevision(coord), detailTier, translated,
                             static_cast<std::uint32_t>(snapshot.center.solidBlockCount()))) return false;
        world_.markChunkMeshQueued(coord);
    }

    (void)world_.takeUnloadedChunkCoords();
    queueDirtyChunkMeshes();
    if (chunkMeshes_.empty()) {
        setError(L"Frontier world generation produced no renderable nearby chunk meshes.");
        return false;
    }
    return true;
}

bool VulkanRenderer::meshJobPending(world::ChunkCoord coord, std::uint64_t revision,
                                    world::meshing::SurfaceDetailTier detailTier) const noexcept {
    (void)revision;
    (void)detailTier;
    return scene::chunkMeshCoordPending(pendingChunkMeshes_, coord);
}

bool VulkanRenderer::queueChunkMesh(world::ChunkCoord coord) {
    const std::uint64_t revision = world_.chunkRevision(coord);
    if (revision == 0) return false;
    const auto detailTier = surfaceDetailTierFor(coord);

    if (const auto existing = chunkMeshes_.find(coord);
        existing != chunkMeshes_.end() && existing->second.revision == revision &&
        existing->second.detailTier == detailTier) {
        world_.markChunkMeshQueued(coord);
        return false;
    }
    if (meshJobPending(coord, revision, detailTier)) {
        // Keep the chunk dirty while its current job is in flight. If that result is stale when it
        // completes, pumpChunkMeshJobs discards it and the next scheduling pass queues the newest
        // revision/detail tier instead of accumulating obsolete duplicate work now.
        return false;
    }
    if (pendingChunkMeshes_.size() >= kMaxPendingChunkMeshes) return false;

    const auto source = world_.chunkMeshingSnapshot(coord);
    if (!source) return false;
    auto snapshot = *source;
    attachDamageVisuals(snapshot, mining_.damageStates(), coord);

    try {
        pendingChunkMeshes_.push_back(PendingChunkMesh{
            coord,
            revision,
            detailTier,
            meshJobs_.submitResult([snapshot = std::move(snapshot), coord, detailTier]() mutable {
                world::VoxelMesh local = world::GreedyMesher::build(snapshot);
                world::meshing::MicroVoxelMesher::append(snapshot, local);
                world::meshing::MicroDetailBuilder::append(snapshot, local, detailTier);
                world::VoxelMesh translated;
                translated.append(local, static_cast<float>(coord.x * world::VoxelChunk::sizeX), 0.0f,
                                  static_cast<float>(coord.z * world::VoxelChunk::sizeZ));
                return translated;
            }),
        });
    } catch (...) {
        return false;
    }

    world_.markChunkMeshQueued(coord);
    return true;
}

void VulkanRenderer::queueDirtyChunkMeshes() {
    auto coords = world_.loadedChunkCoords();
    const auto position = player_.position();
    const world::ChunkCoord playerChunk = world::chunkFromWorld(position.x, position.z);
    std::sort(coords.begin(), coords.end(), [playerChunk](world::ChunkCoord a, world::ChunkCoord b) {
        const int da = world::chebyshevDistance(a, playerChunk);
        const int db = world::chebyshevDistance(b, playerChunk);
        if (da != db) return da < db;
        if (a.x != b.x) return a.x < b.x;
        return a.z < b.z;
    });

    int scheduled = 0;
    for (const world::ChunkCoord coord : coords) {
        if (scheduled >= kMeshScheduleBudgetPerFrame || pendingChunkMeshes_.size() >= kMaxPendingChunkMeshes) break;
        if (queueChunkMesh(coord)) ++scheduled;
    }
}

void VulkanRenderer::pumpChunkMeshJobs() {
    using namespace std::chrono_literals;
    constexpr int uploadBudgetPerFrame = 2;
    int uploaded = 0;
    for (auto it = pendingChunkMeshes_.begin(); it != pendingChunkMeshes_.end() && uploaded < uploadBudgetPerFrame;) {
        if (it->future.wait_for(0ms) != std::future_status::ready) { ++it; continue; }

        const world::ChunkCoord coord = it->coord;
        const std::uint64_t revision = it->revision;
        const auto detailTier = it->detailTier;
        world::VoxelMesh mesh = it->future.get();
        it = pendingChunkMeshes_.erase(it);
        if (world_.chunkRevision(coord) != revision) continue;
        if (surfaceDetailTierFor(coord) != detailTier) continue;
        const auto snapshot = world_.chunkMeshingSnapshot(coord);
        if (!snapshot) continue;
        if (!uploadChunkMesh(coord, revision, detailTier, mesh,
                             static_cast<std::uint32_t>(snapshot->center.solidBlockCount()))) return;
        ++uploaded;
    }
}

void VulkanRenderer::removeUnloadedChunkMeshes() {
    for (const world::ChunkCoord coord : world_.takeUnloadedChunkCoords()) {
        const auto existing = chunkMeshes_.find(coord);
        if (existing == chunkMeshes_.end()) continue;
        destroyChunkMesh(existing->second);
        chunkMeshes_.erase(existing);
    }
    refreshSceneCounters();
}

void VulkanRenderer::drawSceneMeshes(VkCommandBuffer commandBuffer, bool waterPass) {
    const scene::ChunkCullInput cull{
        player_.eyePosition(), player_.lookDirection(),
        static_cast<float>((world::FrontierWorld::streamingPrefetchRadius + 1) * world::VoxelChunk::sizeX),
    };
    if (!waterPass) visibleChunkCount_ = 0;
    for (auto& [coord, mesh] : chunkMeshes_) {
        if (!scene::ChunkCulling::visible(coord, cull)) continue;
        const auto indexCount = waterPass ? mesh.waterIndexCount : mesh.opaqueIndexCount;
        const auto indexBuffer = waterPass ? mesh.waterIndices.buffer : mesh.opaqueIndices.buffer;
        if (indexCount == 0 || indexBuffer == VK_NULL_HANDLE) continue;
        const VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &mesh.vertices.buffer, &offset);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
        if (!waterPass) ++visibleChunkCount_;
    }
    if (!waterPass) drawWorldDrops(commandBuffer);
}

void VulkanRenderer::refreshSceneCounters() {
    sceneQuadCount_ = 0;
    sceneBlockCount_ = 0;
    for (const auto& [coord, mesh] : chunkMeshes_) {
        (void)coord;
        sceneQuadCount_ += mesh.quadCount;
        sceneBlockCount_ += mesh.solidBlockCount;
    }
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