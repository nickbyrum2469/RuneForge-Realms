#include "world/FrontierWorld.h"

#include "world/generation/TerrainGenerator.h"
#include "world/meshing/MicroVoxelMesher.h"

#include <algorithm>
#include <cmath>

namespace rf::world {

void FrontierWorld::generate(std::uint32_t seed) {
    seed_ = seed;
    chunks_.clear();
    editsByChunk_.clear();
    microByChunk_.clear();
    recentlyUnloaded_.clear();
    streamCenter_ = {0, 0};

    const auto generator = [seed](ChunkCoord coord) {
        return generation::TerrainGenerator::generateChunk(seed, coord);
    };
    const auto delta = chunks_.update(streamCenter_, initialChunkRadius, initialChunkRadius,
                                      initialChunkRadius + 1, generator);
    for (const ChunkCoord coord : delta.loaded) {
        applyStoredEditsToChunk(coord);
        markAdjacentChunksDirty(coord);
    }
}

bool FrontierWorld::updateStreaming(float worldX, float worldZ) {
    const ChunkCoord center = chunkFromWorld(worldX, worldZ);
    const std::uint32_t capturedSeed = seed_;
    const auto generator = [capturedSeed](ChunkCoord coord) {
        return generation::TerrainGenerator::generateChunk(capturedSeed, coord);
    };
    const auto delta = chunks_.update(center, streamingResidentRadius, streamingPrefetchRadius,
                                      streamingRetainRadius, generator);
    streamCenter_ = center;

    for (const ChunkCoord coord : delta.loaded) {
        applyStoredEditsToChunk(coord);
        markAdjacentChunksDirty(coord);
    }
    for (const ChunkCoord coord : delta.unloaded) markAdjacentChunksDirty(coord);

    recentlyUnloaded_.insert(recentlyUnloaded_.end(), delta.unloaded.begin(), delta.unloaded.end());
    return delta.changed();
}

void FrontierWorld::applyStoredEditsToChunk(ChunkCoord coord) {
    VoxelChunk* chunk = chunks_.find(coord);
    if (!chunk) return;
    const auto stored = editsByChunk_.find(coord);
    if (stored == editsByChunk_.end()) return;
    for (const auto& [position, block] : stored->second) {
        if (position.y < 0 || position.y >= VoxelChunk::sizeY) continue;
        chunk->set(localBlockX(position.x), position.y, localBlockZ(position.z), block);
    }
}

BlockId FrontierWorld::getBlock(int x, int y, int z) const noexcept {
    if (y < 0 || y >= VoxelChunk::sizeY) return BlockId::Air;
    const VoxelChunk* chunk = chunks_.find(chunkFromBlock(x, z));
    if (!chunk) return BlockId::Air;
    return chunk->get(localBlockX(x), y, localBlockZ(z));
}

const micro::MicroVoxelState* FrontierWorld::microState(BlockCoord position) const noexcept {
    const ChunkCoord coord = chunkFromBlock(position.x, position.z);
    const auto chunkIt = microByChunk_.find(coord);
    if (chunkIt == microByChunk_.end()) return nullptr;
    const auto blockIt = chunkIt->second.find(position);
    return blockIt == chunkIt->second.end() ? nullptr : &blockIt->second.state;
}

void FrontierWorld::markAdjacentChunksDirty(ChunkCoord coord) noexcept {
    chunks_.markDirty({coord.x - 1, coord.z});
    chunks_.markDirty({coord.x + 1, coord.z});
    chunks_.markDirty({coord.x, coord.z - 1});
    chunks_.markDirty({coord.x, coord.z + 1});
}

void FrontierWorld::markMeshNeighborhoodDirty(ChunkCoord coord, int localX, int localZ) noexcept {
    chunks_.markDirty(coord);
    if (localX == 0) chunks_.markDirty({coord.x - 1, coord.z});
    if (localX == VoxelChunk::sizeX - 1) chunks_.markDirty({coord.x + 1, coord.z});
    if (localZ == 0) chunks_.markDirty({coord.x, coord.z - 1});
    if (localZ == VoxelChunk::sizeZ - 1) chunks_.markDirty({coord.x, coord.z + 1});
}

bool FrontierWorld::setBlock(int x, int y, int z, BlockId block, bool recordEdit) {
    if (y < 0 || y >= VoxelChunk::sizeY) return false;
    const BlockCoord position{x, y, z};
    const ChunkCoord coord = chunkFromBlock(x, z);
    if (recordEdit) editsByChunk_[coord][position] = block;

    if (auto microChunk = microByChunk_.find(coord); microChunk != microByChunk_.end()) {
        microChunk->second.erase(position);
        if (microChunk->second.empty()) microByChunk_.erase(microChunk);
    }

    VoxelChunk* chunk = chunks_.find(coord);
    if (!chunk) return recordEdit;

    const int localX = localBlockX(x);
    const int localZ = localBlockZ(z);
    if (chunk->get(localX, y, localZ) == block) {
        markMeshNeighborhoodDirty(coord, localX, localZ);
        return false;
    }
    chunk->set(localX, y, localZ, block);
    markMeshNeighborhoodDirty(coord, localX, localZ);
    return true;
}

bool FrontierWorld::chipBlock(const RaycastHit& hit) {
    if (!hit.hit || hit.block.y <= 0) return false;
    const BlockId block = getBlock(hit.block.x, hit.block.y, hit.block.z);
    if (!isSolid(block)) return false;

    const ChunkCoord coord = chunkFromBlock(hit.block.x, hit.block.z);
    auto& promoted = microByChunk_[coord][hit.block];
    promoted.block = block;

    micro::MicroCoord center = hit.micro;
    if (!hit.microResolved) {
        const auto cell = [](float worldValue, int blockValue) {
            const float local = std::clamp(worldValue - static_cast<float>(blockValue), 0.0f, 0.999999f);
            return static_cast<std::uint8_t>(std::clamp(static_cast<int>(local * micro::resolution), 0, micro::resolution - 1));
        };
        center = {cell(hit.worldX, hit.block.x), cell(hit.worldY, hit.block.y), cell(hit.worldZ, hit.block.z)};
    }

    int radius = 1;
    if (block == BlockId::Grass || block == BlockId::Dirt || block == BlockId::Leaves) radius = 2;
    const std::size_t removed = promoted.state.clearSphere(center, radius);
    if (removed == 0) return false;

    const int localX = localBlockX(hit.block.x);
    const int localZ = localBlockZ(hit.block.z);
    if (promoted.state.empty()) {
        microByChunk_[coord].erase(hit.block);
        if (microByChunk_[coord].empty()) microByChunk_.erase(coord);
        return setBlock(hit.block.x, hit.block.y, hit.block.z, BlockId::Air, true);
    }

    markMeshNeighborhoodDirty(coord, localX, localZ);
    return true;
}

int FrontierWorld::topSolidY(int x, int z) const noexcept {
    for (int y = VoxelChunk::sizeY - 1; y >= 0; --y) {
        if (isSolid(getBlock(x, y, z))) return y;
    }
    return -1;
}

bool FrontierWorld::microCollides(BlockCoord block, const micro::MicroVoxelState& state,
                                  float minX, float minY, float minZ,
                                  float maxX, float maxY, float maxZ) const noexcept {
    constexpr float epsilon = 0.0001f;
    const float lx0 = std::clamp(minX - static_cast<float>(block.x), 0.0f, 1.0f);
    const float ly0 = std::clamp(minY - static_cast<float>(block.y), 0.0f, 1.0f);
    const float lz0 = std::clamp(minZ - static_cast<float>(block.z), 0.0f, 1.0f);
    const float lx1 = std::clamp(maxX - static_cast<float>(block.x), 0.0f, 1.0f);
    const float ly1 = std::clamp(maxY - static_cast<float>(block.y), 0.0f, 1.0f);
    const float lz1 = std::clamp(maxZ - static_cast<float>(block.z), 0.0f, 1.0f);
    if (lx1 <= lx0 || ly1 <= ly0 || lz1 <= lz0) return false;

    const auto firstCell = [](float value) {
        return std::clamp(static_cast<int>(std::floor(value * micro::resolution)), 0, micro::resolution - 1);
    };
    const auto lastCell = [epsilon](float value) {
        return std::clamp(static_cast<int>(std::floor((value - epsilon) * micro::resolution)), 0, micro::resolution - 1);
    };

    const int x0 = firstCell(lx0), x1 = lastCell(lx1);
    const int y0 = firstCell(ly0), y1 = lastCell(ly1);
    const int z0 = firstCell(lz0), z1 = lastCell(lz1);
    for (int my = y0; my <= y1; ++my) {
        for (int mz = z0; mz <= z1; ++mz) {
            for (int mx = x0; mx <= x1; ++mx) {
                if (state.occupied(mx, my, mz)) return true;
            }
        }
    }
    return false;
}

bool FrontierWorld::collidesAabb(float minX, float minY, float minZ,
                                 float maxX, float maxY, float maxZ) const noexcept {
    constexpr float epsilon = 0.001f;
    const int x0 = static_cast<int>(std::floor(minX));
    const int x1 = static_cast<int>(std::floor(maxX - epsilon));
    const int y0 = static_cast<int>(std::floor(minY));
    const int y1 = static_cast<int>(std::floor(maxY - epsilon));
    const int z0 = static_cast<int>(std::floor(minZ));
    const int z1 = static_cast<int>(std::floor(maxZ - epsilon));

    for (int y = y0; y <= y1; ++y) {
        for (int z = z0; z <= z1; ++z) {
            for (int x = x0; x <= x1; ++x) {
                if (!isSolid(getBlock(x, y, z))) continue;
                const BlockCoord block{x, y, z};
                if (const auto* state = microState(block)) {
                    if (microCollides(block, *state, minX, minY, minZ, maxX, maxY, maxZ)) return true;
                } else {
                    return true;
                }
            }
        }
    }
    return false;
}

RaycastHit FrontierWorld::raycast(float ox, float oy, float oz,
                                  float dx, float dy, float dz, float maxDistance) const noexcept {
    const float len = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (len <= 0.00001f) return {};
    dx /= len;
    dy /= len;
    dz /= len;

    constexpr float step = micro::cellSize * 0.28f;
    BlockCoord previous{static_cast<int>(std::floor(ox)), static_cast<int>(std::floor(oy)),
                        static_cast<int>(std::floor(oz))};
    for (float distance = 0.0f; distance <= maxDistance; distance += step) {
        const float wx = ox + dx * distance;
        const float wy = oy + dy * distance;
        const float wz = oz + dz * distance;
        const BlockCoord current{static_cast<int>(std::floor(wx)), static_cast<int>(std::floor(wy)),
                                 static_cast<int>(std::floor(wz))};
        const BlockId block = getBlock(current.x, current.y, current.z);
        if (isSolid(block)) {
            const auto cell = [](float worldValue, int blockValue) {
                const float local = std::clamp(worldValue - static_cast<float>(blockValue), 0.0f, 0.999999f);
                return static_cast<std::uint8_t>(std::clamp(static_cast<int>(local * micro::resolution), 0, micro::resolution - 1));
            };
            const micro::MicroCoord microCoord{cell(wx, current.x), cell(wy, current.y), cell(wz, current.z)};
            const auto* promoted = microState(current);
            if (!promoted || promoted->occupied(microCoord.x, microCoord.y, microCoord.z)) {
                const float priorDistance = std::max(0.0f, distance - step * 1.5f);
                const BlockCoord adjacent{
                    static_cast<int>(std::floor(ox + dx * priorDistance)),
                    static_cast<int>(std::floor(oy + dy * priorDistance)),
                    static_cast<int>(std::floor(oz + dz * priorDistance)),
                };
                return {true, current, adjacent, microCoord, wx, wy, wz, true};
            }
        }
        previous = current;
    }
    return {};
}

std::optional<ChunkMeshingSnapshot> FrontierWorld::chunkMeshingSnapshot(ChunkCoord coord) const {
    const VoxelChunk* center = chunks_.find(coord);
    if (!center) return std::nullopt;

    ChunkMeshingSnapshot snapshot;
    snapshot.center = *center;
    if (const VoxelChunk* chunk = chunks_.find({coord.x - 1, coord.z})) snapshot.negativeX = *chunk;
    if (const VoxelChunk* chunk = chunks_.find({coord.x + 1, coord.z})) snapshot.positiveX = *chunk;
    if (const VoxelChunk* chunk = chunks_.find({coord.x, coord.z - 1})) snapshot.negativeZ = *chunk;
    if (const VoxelChunk* chunk = chunks_.find({coord.x, coord.z + 1})) snapshot.positiveZ = *chunk;

    if (const auto promotedChunk = microByChunk_.find(coord); promotedChunk != microByChunk_.end()) {
        snapshot.microBlocks.reserve(promotedChunk->second.size());
        for (const auto& [position, promoted] : promotedChunk->second) {
            if (position.y < 0 || position.y >= VoxelChunk::sizeY) continue;
            const int lx = localBlockX(position.x);
            const int lz = localBlockZ(position.z);
            if (!isSolid(snapshot.center.get(lx, position.y, lz))) continue;
            snapshot.microBlocks.push_back({lx, position.y, lz, promoted.block, promoted.state});
            snapshot.center.set(lx, position.y, lz, BlockId::Air);
        }
    }
    return snapshot;
}

VoxelMesh FrontierWorld::buildChunkMesh(ChunkCoord coord) const {
    const auto snapshot = chunkMeshingSnapshot(coord);
    if (!snapshot) return {};
    VoxelMesh local = GreedyMesher::build(*snapshot);
    meshing::MicroVoxelMesher::append(*snapshot, local);
    VoxelMesh translated;
    translated.append(local, static_cast<float>(coord.x * VoxelChunk::sizeX), 0.0f,
                      static_cast<float>(coord.z * VoxelChunk::sizeZ));
    return translated;
}

VoxelMesh FrontierWorld::buildMesh() const {
    VoxelMesh result;
    for (const ChunkCoord coord : chunks_.loadedCoords()) {
        const VoxelMesh mesh = buildChunkMesh(coord);
        result.append(mesh, 0.0f, 0.0f, 0.0f);
    }
    return result;
}

std::size_t FrontierWorld::solidBlockCount() const noexcept {
    std::size_t result = 0;
    for (const ChunkCoord coord : chunks_.loadedCoords()) {
        const VoxelChunk* chunk = chunks_.find(coord);
        if (chunk) result += chunk->solidBlockCount();
    }
    return result;
}

std::vector<ChunkCoord> FrontierWorld::takeUnloadedChunkCoords() {
    std::vector<ChunkCoord> result = std::move(recentlyUnloaded_);
    recentlyUnloaded_.clear();
    return result;
}

std::vector<BlockEdit> FrontierWorld::edits() const {
    std::vector<BlockEdit> result;
    std::size_t total = 0;
    for (const auto& [coord, chunkEdits] : editsByChunk_) {
        (void)coord;
        total += chunkEdits.size();
    }
    result.reserve(total);
    for (const auto& [coord, chunkEdits] : editsByChunk_) {
        (void)coord;
        for (const auto& [position, block] : chunkEdits) result.push_back({position, block});
    }
    return result;
}

void FrontierWorld::applyEdit(const BlockEdit& edit) {
    if (edit.position.y < 0 || edit.position.y >= VoxelChunk::sizeY) return;
    const ChunkCoord coord = chunkFromBlock(edit.position.x, edit.position.z);
    editsByChunk_[coord][edit.position] = edit.block;
    VoxelChunk* chunk = chunks_.find(coord);
    if (!chunk) return;
    const int localX = localBlockX(edit.position.x);
    const int localZ = localBlockZ(edit.position.z);
    chunk->set(localX, edit.position.y, localZ, edit.block);
    markMeshNeighborhoodDirty(coord, localX, localZ);
}

std::vector<micro::MicroVoxelEdit> FrontierWorld::microEdits() const {
    std::vector<micro::MicroVoxelEdit> result;
    for (const auto& [coord, blocks] : microByChunk_) {
        (void)coord;
        for (const auto& [position, promoted] : blocks) {
            if (promoted.state.full() || promoted.state.empty()) continue;
            result.push_back(micro::makeEdit(position, promoted.block, promoted.state));
        }
    }
    return result;
}

void FrontierWorld::applyMicroEdit(const micro::MicroVoxelEdit& edit) {
    if (edit.position.y < 0 || edit.position.y >= VoxelChunk::sizeY || !isSolid(edit.block)) return;
    const micro::MicroVoxelState state = micro::stateFromEdit(edit);
    if (state.full()) return;
    if (state.empty()) {
        setBlock(edit.position.x, edit.position.y, edit.position.z, BlockId::Air, false);
        return;
    }

    setBlock(edit.position.x, edit.position.y, edit.position.z, edit.block, false);
    const ChunkCoord coord = chunkFromBlock(edit.position.x, edit.position.z);
    microByChunk_[coord][edit.position] = PromotedBlock{edit.block, state};
    markMeshNeighborhoodDirty(coord, localBlockX(edit.position.x), localBlockZ(edit.position.z));
}

} // namespace rf::world
