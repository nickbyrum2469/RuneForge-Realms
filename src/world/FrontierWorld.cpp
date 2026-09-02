#include "world/FrontierWorld.h"

#include "world/generation/TerrainGenerator.h"
#include "world/growth/GrassGrowth.h"
#include "world/meshing/MicroDetailBuilder.h"
#include "world/meshing/MicroVoxelMesher.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace rf::world {
namespace {

micro::MicroCoord microCoordFromWorld(BlockCoord block, float wx, float wy, float wz) noexcept {
    const auto cell = [](float value, int blockValue) {
        const float local = std::clamp(value - static_cast<float>(blockValue), 0.0f, 0.999999f);
        return static_cast<std::uint8_t>(
            std::clamp(static_cast<int>(local * micro::resolution), 0, micro::resolution - 1));
    };
    return {cell(wx, block.x), cell(wy, block.y), cell(wz, block.z)};
}

} // namespace

void FrontierWorld::generate(std::uint32_t seed) {
    seed_ = seed;
    worldAgeSeconds_ = 0.0f;
    growthEpoch_ = 0;
    chunks_.clear();
    waterSimulation_.reset();
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

    // Dynamic fluid bookkeeping follows the retained chunk window. Stable generated lakes have no
    // per-cell simulation state, so trimming here keeps long-distance travel memory bounded.
    waterSimulation_.trim(center, streamingRetainRadius + 1);
    recentlyUnloaded_.insert(recentlyUnloaded_.end(), delta.unloaded.begin(), delta.unloaded.end());
    return delta.changed();
}

bool FrontierWorld::advanceSimulation(float deltaSeconds) {
    const float dt = std::clamp(deltaSeconds, 0.0f, 0.25f);
    worldAgeSeconds_ += dt;
    bool changed = waterSimulation_.update(dt, *this);

    const auto epoch = static_cast<std::uint32_t>(worldAgeSeconds_ / growth::GrassGrowth::growthStepSeconds);
    if (epoch != growthEpoch_) {
        growthEpoch_ = epoch;
        markAllLoadedDirty();
        changed = true;
    }
    return changed;
}

void FrontierWorld::setWorldAgeSeconds(float value) noexcept {
    worldAgeSeconds_ = std::max(0.0f, value);
    growthEpoch_ = static_cast<std::uint32_t>(worldAgeSeconds_ / growth::GrassGrowth::growthStepSeconds);
}

void FrontierWorld::applyStoredEditsToChunk(ChunkCoord coord) {
    VoxelChunk* chunk = chunks_.find(coord);
    if (!chunk) return;
    const auto stored = editsByChunk_.find(coord);
    if (stored == editsByChunk_.end()) return;
    for (const auto& [position, block] : stored->second) {
        if (position.y < 0 || position.y >= VoxelChunk::sizeY) continue;
        const int localX = localBlockX(position.x);
        const int localZ = localBlockZ(position.z);
        const BlockId previous = chunk->get(localX, position.y, localZ);
        chunk->set(localX, position.y, localZ, block);
        waterSimulation_.onExternalBlockChange(position, previous, block);
    }
}

BlockId FrontierWorld::getBlock(int x, int y, int z) const noexcept {
    if (y < 0 || y >= VoxelChunk::sizeY) return BlockId::Air;
    const VoxelChunk* chunk = chunks_.find(chunkFromBlock(x, z));
    if (!chunk) return BlockId::Air;
    return chunk->get(localBlockX(x), y, localBlockZ(z));
}

const FrontierWorld::PromotedBlock* FrontierWorld::promotedBlock(BlockCoord position) const noexcept {
    const ChunkCoord coord = chunkFromBlock(position.x, position.z);
    const auto chunkIt = microByChunk_.find(coord);
    if (chunkIt == microByChunk_.end()) return nullptr;
    const auto blockIt = chunkIt->second.find(position);
    return blockIt == chunkIt->second.end() ? nullptr : &blockIt->second;
}

const micro::MicroVoxelState* FrontierWorld::microState(BlockCoord position) const noexcept {
    const auto* promoted = promotedBlock(position);
    return promoted ? &promoted->state : nullptr;
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

void FrontierWorld::markAllLoadedDirty() noexcept {
    for (const ChunkCoord coord : chunks_.loadedCoords()) chunks_.markDirty(coord);
}

bool FrontierWorld::setBlock(int x, int y, int z, BlockId block, bool recordEdit) {
    if (y < 0 || y >= VoxelChunk::sizeY) return false;
    const BlockCoord position{x, y, z};
    const ChunkCoord coord = chunkFromBlock(x, z);
    const BlockId previousLoaded = getBlock(x, y, z);
    if (recordEdit) editsByChunk_[coord][position] = block;

    bool hadMicro = false;
    if (auto microChunk = microByChunk_.find(coord); microChunk != microByChunk_.end()) {
        hadMicro = microChunk->second.erase(position) > 0;
        if (microChunk->second.empty()) microByChunk_.erase(microChunk);
    }

    VoxelChunk* chunk = chunks_.find(coord);
    if (!chunk) {
        if (recordEdit) waterSimulation_.onExternalBlockChange(position, previousLoaded, block);
        return recordEdit || hadMicro;
    }

    const int localX = localBlockX(x);
    const int localZ = localBlockZ(z);
    const BlockId previous = chunk->get(localX, y, localZ);
    if (previous == block && !hadMicro) return false;
    chunk->set(localX, y, localZ, block);
    markMeshNeighborhoodDirty(coord, localX, localZ);
    if (recordEdit) waterSimulation_.onExternalBlockChange(position, previous, block);
    return true;
}

MicroChipResult FrontierWorld::chipBlock(BlockCoord position, float worldHitX, float worldHitY,
                                          float worldHitZ, int radiusCells) {
    MicroChipResult result;
    if (position.y <= 0 || radiusCells <= 0) return result;
    const BlockId block = getBlock(position.x, position.y, position.z);
    if (!isSolid(block)) return result;

    const ChunkCoord coord = chunkFromBlock(position.x, position.z);
    auto& promoted = microByChunk_[coord][position];
    promoted.block = block;

    const micro::MicroCoord center = microCoordFromWorld(position, worldHitX, worldHitY, worldHitZ);
    result.removedCells = promoted.state.clearSphere(center, radiusCells);
    result.changed = result.removedCells > 0;
    result.solidFraction = promoted.state.solidFraction();
    if (!result.changed) return result;

    const int localX = localBlockX(position.x);
    const int localZ = localBlockZ(position.z);
    if (promoted.state.empty()) {
        result.emptied = true;
        result.solidFraction = 0.0f;
        microByChunk_[coord].erase(position);
        if (microByChunk_[coord].empty()) microByChunk_.erase(coord);
        (void)setBlock(position.x, position.y, position.z, BlockId::Air, true);
        return result;
    }

    markMeshNeighborhoodDirty(coord, localX, localZ);
    return result;
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
    BlockCoord lastEmpty{static_cast<int>(std::floor(ox)), static_cast<int>(std::floor(oy)),
                         static_cast<int>(std::floor(oz))};
    for (float distance = 0.0f; distance <= maxDistance; distance += step) {
        const float wx = ox + dx * distance;
        const float wy = oy + dy * distance;
        const float wz = oz + dz * distance;
        const BlockCoord current{static_cast<int>(std::floor(wx)), static_cast<int>(std::floor(wy)),
                                 static_cast<int>(std::floor(wz))};
        const BlockId block = getBlock(current.x, current.y, current.z);
        if (!isSolid(block)) {
            lastEmpty = current;
            continue;
        }

        const micro::MicroCoord cell = microCoordFromWorld(current, wx, wy, wz);
        const auto* promoted = microState(current);
        if (promoted && !promoted->occupied(cell.x, cell.y, cell.z)) {
            lastEmpty = current;
            continue;
        }
        return {true, current, lastEmpty, cell, wx, wy, wz, true};
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
    snapshot.worldSeed = seed_;
    snapshot.worldAgeSeconds = worldAgeSeconds_;
    snapshot.worldOriginX = coord.x * VoxelChunk::sizeX;
    snapshot.worldOriginZ = coord.z * VoxelChunk::sizeZ;

    std::set<BlockCoord> candidates;
    constexpr std::array<BlockCoord, 7> offsets{{
        {0, 0, 0}, {-1, 0, 0}, {1, 0, 0}, {0, -1, 0},
        {0, 1, 0}, {0, 0, -1}, {0, 0, 1},
    }};
    const std::array<ChunkCoord, 5> sourceChunks{{
        coord, {coord.x - 1, coord.z}, {coord.x + 1, coord.z},
        {coord.x, coord.z - 1}, {coord.x, coord.z + 1},
    }};
    for (const ChunkCoord source : sourceChunks) {
        const auto promotedChunk = microByChunk_.find(source);
        if (promotedChunk == microByChunk_.end()) continue;
        for (const auto& [position, promoted] : promotedChunk->second) {
            (void)promoted;
            for (const auto& offset : offsets) {
                candidates.insert({position.x + offset.x, position.y + offset.y, position.z + offset.z});
            }
        }
    }

    for (const BlockCoord position : candidates) {
        if (position.y < 0 || position.y >= VoxelChunk::sizeY) continue;
        const int lx = position.x - snapshot.worldOriginX;
        const int lz = position.z - snapshot.worldOriginZ;
        if (lx < -1 || lx > VoxelChunk::sizeX || lz < -1 || lz > VoxelChunk::sizeZ) continue;
        if ((lx < 0 || lx >= VoxelChunk::sizeX) && (lz < 0 || lz >= VoxelChunk::sizeZ)) continue;

        const BlockId block = getBlock(position.x, position.y, position.z);
        if (!isSolid(block)) continue;
        micro::MicroVoxelState state;
        if (const auto* persistent = promotedBlock(position)) state = persistent->state;
        const bool owned = lx >= 0 && lx < VoxelChunk::sizeX && lz >= 0 && lz < VoxelChunk::sizeZ;
        snapshot.microBlocks.push_back({lx, position.y, lz, block, state, owned});

        if (owned) {
            snapshot.center.set(lx, position.y, lz, BlockId::Air);
        } else if (lx == -1 && snapshot.negativeX && lz >= 0 && lz < VoxelChunk::sizeZ) {
            snapshot.negativeX->set(VoxelChunk::sizeX - 1, position.y, lz, BlockId::Air);
        } else if (lx == VoxelChunk::sizeX && snapshot.positiveX && lz >= 0 && lz < VoxelChunk::sizeZ) {
            snapshot.positiveX->set(0, position.y, lz, BlockId::Air);
        } else if (lz == -1 && snapshot.negativeZ && lx >= 0 && lx < VoxelChunk::sizeX) {
            snapshot.negativeZ->set(lx, position.y, VoxelChunk::sizeZ - 1, BlockId::Air);
        } else if (lz == VoxelChunk::sizeZ && snapshot.positiveZ && lx >= 0 && lx < VoxelChunk::sizeX) {
            snapshot.positiveZ->set(lx, position.y, 0, BlockId::Air);
        }
    }
    return snapshot;
}

VoxelMesh FrontierWorld::buildChunkMesh(ChunkCoord coord) const {
    const auto snapshot = chunkMeshingSnapshot(coord);
    if (!snapshot) return {};
    VoxelMesh local = GreedyMesher::build(*snapshot);
    meshing::MicroVoxelMesher::append(*snapshot, local);
    meshing::MicroDetailBuilder::append(*snapshot, local);
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

std::size_t FrontierWorld::promotedBlockCount() const noexcept {
    std::size_t count = 0;
    for (const auto& [coord, blocks] : microByChunk_) {
        (void)coord;
        count += blocks.size();
    }
    return count;
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
    const BlockId previous = chunk->get(localX, edit.position.y, localZ);
    chunk->set(localX, edit.position.y, localZ, edit.block);
    markMeshNeighborhoodDirty(coord, localX, localZ);
    waterSimulation_.onExternalBlockChange(edit.position, previous, edit.block);
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
        (void)setBlock(edit.position.x, edit.position.y, edit.position.z, BlockId::Air, false);
        return;
    }

    (void)setBlock(edit.position.x, edit.position.y, edit.position.z, edit.block, false);
    const ChunkCoord coord = chunkFromBlock(edit.position.x, edit.position.z);
    microByChunk_[coord][edit.position] = PromotedBlock{edit.block, state};
    markMeshNeighborhoodDirty(coord, localBlockX(edit.position.x), localBlockZ(edit.position.z));
}

} // namespace rf::world
