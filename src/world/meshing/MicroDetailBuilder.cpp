#include "world/meshing/MicroDetailBuilder.h"

#include "world/surface/SurfaceRelief.h"

#include <algorithm>
#include <array>
#include <cstdint>

namespace rf::world::meshing {
namespace {

std::uint32_t detailHash(int x, int y, int z, std::uint32_t seed) noexcept {
    std::uint32_t h = seed ^ static_cast<std::uint32_t>(x) * 0x8da6b343u;
    h ^= static_cast<std::uint32_t>(y) * 0xd8163841u;
    h ^= static_cast<std::uint32_t>(z) * 0xcb1ab31fu;
    h ^= h >> 13;
    h *= 0x85ebca6bu;
    h ^= h >> 16;
    return h;
}

void addQuad(VoxelMesh& mesh,
             std::array<float, 3> p0, std::array<float, 3> p1,
             std::array<float, 3> p2, std::array<float, 3> p3,
             std::array<float, 3> normal, SurfaceMaterial material) {
    const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
    const auto vertex = [&](const std::array<float, 3>& p) {
        return MeshVertex{p[0], p[1], p[2], normal[0], normal[1], normal[2], packMaterial(material)};
    };
    mesh.vertices.insert(mesh.vertices.end(), {vertex(p0), vertex(p1), vertex(p2), vertex(p3)});
    mesh.indices.insert(mesh.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    ++mesh.quadCount;
}

void addBox(VoxelMesh& mesh, float x, float y, float z,
            float sx, float sy, float sz, SurfaceMaterial material) {
    const float x1 = x + sx, y1 = y + sy, z1 = z + sz;
    addQuad(mesh, {x,y,z1}, {x1,y,z1}, {x1,y1,z1}, {x,y1,z1}, {0,0,1}, material);
    addQuad(mesh, {x1,y,z}, {x,y,z}, {x,y1,z}, {x1,y1,z}, {0,0,-1}, material);
    addQuad(mesh, {x1,y,z1}, {x1,y,z}, {x1,y1,z}, {x1,y1,z1}, {1,0,0}, material);
    addQuad(mesh, {x,y,z}, {x,y,z1}, {x,y1,z1}, {x,y1,z}, {-1,0,0}, material);
    addQuad(mesh, {x,y1,z1}, {x1,y1,z1}, {x1,y1,z}, {x,y1,z}, {0,1,0}, material);
    addQuad(mesh, {x,y,z}, {x1,y,z}, {x1,y,z1}, {x,y,z1}, {0,-1,0}, material);
}

surface::SurfaceReliefBudget budgetFor(SurfaceDetailTier tier) noexcept {
    switch (tier) {
        case SurfaceDetailTier::Hero: return surface::SurfaceRelief::heroBudget();
        case SurfaceDetailTier::Standard: return surface::SurfaceRelief::standardBudget();
        case SurfaceDetailTier::Distant: return surface::SurfaceRelief::distantBudget();
    }
    return surface::SurfaceRelief::distantBudget();
}

BlockId blockAt(const ChunkMeshingSnapshot& snapshot, int x, int y, int z) noexcept {
    if (y < 0 || y >= VoxelChunk::sizeY) return BlockId::Air;
    if (x >= 0 && x < VoxelChunk::sizeX && z >= 0 && z < VoxelChunk::sizeZ) {
        return snapshot.center.get(x, y, z);
    }
    if (x == -1 && z >= 0 && z < VoxelChunk::sizeZ && snapshot.negativeX) {
        return snapshot.negativeX->get(VoxelChunk::sizeX - 1, y, z);
    }
    if (x == VoxelChunk::sizeX && z >= 0 && z < VoxelChunk::sizeZ && snapshot.positiveX) {
        return snapshot.positiveX->get(0, y, z);
    }
    if (z == -1 && x >= 0 && x < VoxelChunk::sizeX && snapshot.negativeZ) {
        return snapshot.negativeZ->get(x, y, VoxelChunk::sizeZ - 1);
    }
    if (z == VoxelChunk::sizeZ && x >= 0 && x < VoxelChunk::sizeX && snapshot.positiveZ) {
        return snapshot.positiveZ->get(x, y, 0);
    }
    return BlockId::Air;
}

bool grassTopVisible(BlockId above) noexcept {
    return above == BlockId::Air || above == BlockId::Leaves;
}

bool horizontalFaceVisible(BlockId neighbor) noexcept {
    return !isOpaque(neighbor);
}

bool microCellVisible(const micro::MicroVoxelState* state, surface::SurfaceFace face,
                      int u, int v) noexcept {
    if (!state) return true;
    const int mu = surface::SurfaceRelief::microCellForVisualCell(u);
    const int mv = surface::SurfaceRelief::microCellForVisualCell(v);
    switch (face) {
        case surface::SurfaceFace::Top:
            return state->occupied(mu, micro::resolution - 1, mv);
        case surface::SurfaceFace::North:
            return state->occupied(mu, mv, 0);
        case surface::SurfaceFace::South:
            return state->occupied(mu, mv, micro::resolution - 1);
        case surface::SurfaceFace::East:
            return state->occupied(micro::resolution - 1, mv, mu);
        case surface::SurfaceFace::West:
            return state->occupied(0, mv, mu);
    }
    return false;
}

void addFlowerCap(VoxelMesh& mesh, float cx, float y, float cz, std::uint16_t id) {
    SurfaceMaterial material = SurfaceMaterial::FlowerWhite;
    if ((id % 3u) == 1u) material = SurfaceMaterial::FlowerYellow;
    else if ((id % 3u) == 2u) material = SurfaceMaterial::FlowerBlue;
    constexpr float size = 0.025f;
    addBox(mesh, cx - size * 0.5f, y, cz - size * 0.5f, size, 0.012f, size, material);
}

void addGrassTop(const ChunkMeshingSnapshot& snapshot, VoxelMesh& mesh,
                 int localX, int y, int localZ, const micro::MicroVoxelState* state,
                 const surface::SurfaceReliefBudget& budget, SurfaceDetailStats* stats) {
    const BlockCoord block{snapshot.worldOriginX + localX, y, snapshot.worldOriginZ + localZ};
    const auto field = surface::SurfaceRelief::grassTop(snapshot.worldSeed, block,
                                                         snapshot.worldAgeSeconds);
    constexpr float cellSize = 1.0f / static_cast<float>(surface::SurfaceReliefField::resolution);
    const int stride = std::max(1, budget.cellStride);
    const int sampledAxis = surface::SurfaceReliefField::resolution / stride;
    const int sampledCells = sampledAxis * sampledAxis;
    std::uint32_t vegetationCount = 0;

    for (int v = 0; v < surface::SurfaceReliefField::resolution; v += stride) {
        for (int u = 0; u < surface::SurfaceReliefField::resolution; u += stride) {
            if (!microCellVisible(state, surface::SurfaceFace::Top, u, v)) continue;
            const auto& cell = field.cell(u, v);
            const float x0 = static_cast<float>(localX) + static_cast<float>(u) * cellSize;
            const float z0 = static_cast<float>(localZ) + static_cast<float>(v) * cellSize;
            const float span = cellSize * static_cast<float>(stride);
            const float surfaceY = static_cast<float>(y + 1) + cell.heightOffset;

            if (budget.emitTopRelief) {
                // Adjacent top plates meet edge-to-edge. 0.6.0 inset every cell and therefore drew
                // long artificial grout channels through the meadow. Height variation now supplies
                // the readable micro-piece separation without a world-scale 16x16 line lattice.
                addQuad(mesh,
                        {x0, surfaceY, z0 + span}, {x0 + span, surfaceY, z0 + span},
                        {x0 + span, surfaceY, z0}, {x0, surfaceY, z0},
                        {0, 1, 0}, SurfaceMaterial::GrassTop);
                if (stats) ++stats->topReliefCells;
            }

            if (cell.vegetation == surface::VegetationProfile::Bare || budget.maxVegetationCells == 0) continue;
            const std::uint32_t selection = static_cast<std::uint32_t>(cell.stableId) %
                                            static_cast<std::uint32_t>(std::max(sampledCells, 1));
            if (selection >= budget.maxVegetationCells || vegetationCount >= budget.maxVegetationCells) continue;

            const float cx = static_cast<float>(localX) + (static_cast<float>(u) + 0.5f) * cellSize +
                             cell.vegetationOffsetU;
            const float cz = static_cast<float>(localZ) + (static_cast<float>(v) + 0.5f) * cellSize +
                             cell.vegetationOffsetV;
            const float width = 0.012f + static_cast<float>(cell.stableId & 3u) * 0.0020f;
            const float depth = 0.013f + static_cast<float>((cell.stableId >> 2) & 3u) * 0.0018f;
            addBox(mesh, cx - width * 0.5f, surfaceY, cz - depth * 0.5f,
                   width, cell.bladeHeight, depth, SurfaceMaterial::GrassTop);

            if (cell.bladeCount >= 2) {
                const float companion = cell.bladeHeight * (0.58f +
                    static_cast<float>((cell.stableId >> 5) & 3u) * 0.075f);
                const float offset = cellSize * (0.16f + static_cast<float>((cell.stableId >> 10) & 3u) * 0.025f);
                const float signU = ((cell.stableId >> 8) & 1u) ? 1.0f : -1.0f;
                const float signV = ((cell.stableId >> 9) & 1u) ? 1.0f : -1.0f;
                addBox(mesh, cx + signU * offset - width * 0.34f, surfaceY,
                       cz + signV * offset - depth * 0.34f,
                       width * 0.68f, companion, depth * 0.68f, SurfaceMaterial::GrassTop);
            }
            if (cell.vegetation == surface::VegetationProfile::FlowerCapable &&
                (cell.stableId % 11u) == 0u) {
                addFlowerCap(mesh, cx, surfaceY + cell.bladeHeight, cz, cell.stableId);
            }
            ++vegetationCount;
            if (stats) ++stats->grassClusters;
        }
    }
}

SurfaceMaterial sideMaterial(bool grassBlock, surface::ReliefClass relief) noexcept {
    if (relief == surface::ReliefClass::Root) return SurfaceMaterial::RootFiber;
    if (relief == surface::ReliefClass::Mineral) return SurfaceMaterial::Stone;
    if (grassBlock && relief == surface::ReliefClass::Turf) return SurfaceMaterial::GrassSide;
    return SurfaceMaterial::Dirt;
}

void emitSidePlate(VoxelMesh& mesh, int x, int y, int z, surface::SurfaceFace face,
                   int u, int v, int stride, float outwardOffset, SurfaceMaterial material) {
    constexpr float cellSize = 1.0f / static_cast<float>(surface::SurfaceReliefField::resolution);
    const float span = cellSize * static_cast<float>(stride);
    const float horizontal = static_cast<float>(u) * cellSize;
    const float vertical = static_cast<float>(y) + static_cast<float>(v) * cellSize;
    const float offset = std::max(0.002f, outwardOffset);

    switch (face) {
        case surface::SurfaceFace::North: {
            const float x0 = static_cast<float>(x) + horizontal;
            const float zz = static_cast<float>(z) - offset;
            addQuad(mesh, {x0 + span, vertical, zz}, {x0, vertical, zz},
                    {x0, vertical + span, zz}, {x0 + span, vertical + span, zz},
                    {0, 0, -1}, material);
            break;
        }
        case surface::SurfaceFace::South: {
            const float x0 = static_cast<float>(x) + horizontal;
            const float zz = static_cast<float>(z + 1) + offset;
            addQuad(mesh, {x0, vertical, zz}, {x0 + span, vertical, zz},
                    {x0 + span, vertical + span, zz}, {x0, vertical + span, zz},
                    {0, 0, 1}, material);
            break;
        }
        case surface::SurfaceFace::East: {
            const float z0 = static_cast<float>(z) + horizontal;
            const float xx = static_cast<float>(x + 1) + offset;
            addQuad(mesh, {xx, vertical, z0 + span}, {xx, vertical, z0},
                    {xx, vertical + span, z0}, {xx, vertical + span, z0 + span},
                    {1, 0, 0}, material);
            break;
        }
        case surface::SurfaceFace::West: {
            const float z0 = static_cast<float>(z) + horizontal;
            const float xx = static_cast<float>(x) - offset;
            addQuad(mesh, {xx, vertical, z0}, {xx, vertical, z0 + span},
                    {xx, vertical + span, z0 + span}, {xx, vertical + span, z0},
                    {-1, 0, 0}, material);
            break;
        }
        case surface::SurfaceFace::Top: break;
    }
}

void emitRootFiber(VoxelMesh& mesh, int x, int y, int z, surface::SurfaceFace face,
                   int u, int v, int stride, const surface::SurfaceCell& cell) {
    constexpr float cellSize = 1.0f / static_cast<float>(surface::SurfaceReliefField::resolution);
    const float span = cellSize * static_cast<float>(stride);
    const float rootWidth = 0.010f + static_cast<float>((cell.stableId >> 3) & 3u) * 0.0022f;
    const float rootDepth = 0.006f + static_cast<float>((cell.stableId >> 7) & 3u) * 0.0015f;
    const float soilOffset = 0.006f + static_cast<float>((cell.stableId >> 11) & 3u) * 0.0013f;
    const float jitter = (static_cast<float>((cell.stableId >> 13) & 7u) / 7.0f - 0.5f) * cellSize * 0.28f;
    const float vertical = static_cast<float>(y) + static_cast<float>(v) * cellSize;
    const float localCenter = (static_cast<float>(u) + 0.5f) * cellSize + jitter;

    // Every root path cell still has soil behind it. Only the actual fiber projects farther out.
    emitSidePlate(mesh, x, y, z, face, u, v, stride, soilOffset, SurfaceMaterial::Dirt);

    switch (face) {
        case surface::SurfaceFace::North:
            addBox(mesh, static_cast<float>(x) + localCenter - rootWidth * 0.5f, vertical,
                   static_cast<float>(z) - soilOffset - rootDepth,
                   rootWidth, span, rootDepth, SurfaceMaterial::RootFiber);
            break;
        case surface::SurfaceFace::South:
            addBox(mesh, static_cast<float>(x) + localCenter - rootWidth * 0.5f, vertical,
                   static_cast<float>(z + 1) + soilOffset,
                   rootWidth, span, rootDepth, SurfaceMaterial::RootFiber);
            break;
        case surface::SurfaceFace::East:
            addBox(mesh, static_cast<float>(x + 1) + soilOffset, vertical,
                   static_cast<float>(z) + localCenter - rootWidth * 0.5f,
                   rootDepth, span, rootWidth, SurfaceMaterial::RootFiber);
            break;
        case surface::SurfaceFace::West:
            addBox(mesh, static_cast<float>(x) - soilOffset - rootDepth, vertical,
                   static_cast<float>(z) + localCenter - rootWidth * 0.5f,
                   rootDepth, span, rootWidth, SurfaceMaterial::RootFiber);
            break;
        case surface::SurfaceFace::Top: return;
    }

    // Occasional short side branch breaks the old ladder/column silhouette without creating giant
    // roots. It stays within the owning surface cell so the future damage mapping remains stable.
    if (((cell.stableId >> 5) & 3u) == 0u) {
        const float branchLength = cellSize * (0.30f + static_cast<float>((cell.stableId >> 9) & 3u) * 0.055f);
        const bool positive = ((cell.stableId >> 12) & 1u) != 0u;
        const float branchStart = localCenter + (positive ? 0.0f : -branchLength);
        const float branchY = vertical + span * 0.55f - rootWidth * 0.40f;
        switch (face) {
            case surface::SurfaceFace::North:
                addBox(mesh, static_cast<float>(x) + branchStart, branchY,
                       static_cast<float>(z) - soilOffset - rootDepth,
                       branchLength, rootWidth * 0.80f, rootDepth, SurfaceMaterial::RootFiber);
                break;
            case surface::SurfaceFace::South:
                addBox(mesh, static_cast<float>(x) + branchStart, branchY,
                       static_cast<float>(z + 1) + soilOffset,
                       branchLength, rootWidth * 0.80f, rootDepth, SurfaceMaterial::RootFiber);
                break;
            case surface::SurfaceFace::East:
                addBox(mesh, static_cast<float>(x + 1) + soilOffset, branchY,
                       static_cast<float>(z) + branchStart,
                       rootDepth, rootWidth * 0.80f, branchLength, SurfaceMaterial::RootFiber);
                break;
            case surface::SurfaceFace::West:
                addBox(mesh, static_cast<float>(x) - soilOffset - rootDepth, branchY,
                       static_cast<float>(z) + branchStart,
                       rootDepth, rootWidth * 0.80f, branchLength, SurfaceMaterial::RootFiber);
                break;
            case surface::SurfaceFace::Top: break;
        }
    }
}

void emitSideCell(VoxelMesh& mesh, int x, int y, int z, surface::SurfaceFace face,
                  int u, int v, int stride, const surface::SurfaceCell& cell,
                  bool grassBlock) {
    if (cell.relief == surface::ReliefClass::Root) {
        emitRootFiber(mesh, x, y, z, face, u, v, stride, cell);
        return;
    }
    emitSidePlate(mesh, x, y, z, face, u, v, stride, cell.heightOffset,
                  sideMaterial(grassBlock, cell.relief));
}

void addSoilSide(const ChunkMeshingSnapshot& snapshot, VoxelMesh& mesh,
                 int localX, int y, int localZ, surface::SurfaceFace face,
                 bool grassBlock, const micro::MicroVoxelState* state,
                 const surface::SurfaceReliefBudget& budget, SurfaceDetailStats* stats) {
    if (!budget.emitSideRelief || budget.maxSideReliefCells == 0) return;
    const BlockCoord block{snapshot.worldOriginX + localX, y, snapshot.worldOriginZ + localZ};
    const auto field = surface::SurfaceRelief::soilSide(snapshot.worldSeed, block, face, grassBlock);
    const int stride = std::max(1, budget.cellStride);
    const int sampledAxis = surface::SurfaceReliefField::resolution / stride;
    const int sampledCells = sampledAxis * sampledAxis;
    std::uint32_t emitted = 0;

    // Turf/root fibers are preserved first. Remaining budget fills the face with cheap one-quad
    // micro-plates. This replaces the sparse six-quad boxes that looked glued onto a flat wall.
    for (int priority = 0; priority < 2 && emitted < budget.maxSideReliefCells; ++priority) {
        for (int v = 0; v < surface::SurfaceReliefField::resolution && emitted < budget.maxSideReliefCells; v += stride) {
            for (int u = 0; u < surface::SurfaceReliefField::resolution && emitted < budget.maxSideReliefCells; u += stride) {
                if (!microCellVisible(state, face, u, v)) continue;
                const auto& cell = field.cell(u, v);
                if (cell.cavity || !cell.occupied) {
                    if (priority == 0 && stats) ++stats->cavityCells;
                    continue;
                }
                const bool important = cell.relief == surface::ReliefClass::Root ||
                                       cell.relief == surface::ReliefClass::Turf;
                if ((priority == 0) != important) continue;
                if (cell.relief == surface::ReliefClass::Root && !budget.emitRoots) continue;
                if (!important) {
                    const std::uint32_t selection = static_cast<std::uint32_t>(cell.stableId) %
                                                    static_cast<std::uint32_t>(std::max(sampledCells, 1));
                    if (selection >= budget.maxSideReliefCells) continue;
                }
                emitSideCell(mesh, localX, y, localZ, face, u, v, stride, cell, grassBlock);
                ++emitted;
                if (stats) {
                    ++stats->sideReliefCells;
                    if (cell.relief == surface::ReliefClass::Root) ++stats->rootCells;
                }
            }
        }
    }
}

void addBlockSides(const ChunkMeshingSnapshot& snapshot, VoxelMesh& mesh,
                   int x, int y, int z, bool grassBlock,
                   const micro::MicroVoxelState* state,
                   const surface::SurfaceReliefBudget& budget, SurfaceDetailStats* stats) {
    if (horizontalFaceVisible(blockAt(snapshot, x, y, z - 1))) {
        addSoilSide(snapshot, mesh, x, y, z, surface::SurfaceFace::North,
                    grassBlock, state, budget, stats);
    }
    if (horizontalFaceVisible(blockAt(snapshot, x, y, z + 1))) {
        addSoilSide(snapshot, mesh, x, y, z, surface::SurfaceFace::South,
                    grassBlock, state, budget, stats);
    }
    if (horizontalFaceVisible(blockAt(snapshot, x + 1, y, z))) {
        addSoilSide(snapshot, mesh, x, y, z, surface::SurfaceFace::East,
                    grassBlock, state, budget, stats);
    }
    if (horizontalFaceVisible(blockAt(snapshot, x - 1, y, z))) {
        addSoilSide(snapshot, mesh, x, y, z, surface::SurfaceFace::West,
                    grassBlock, state, budget, stats);
    }
}

void addSoilTopClods(VoxelMesh& mesh, int x, int y, int z, std::uint32_t h, SurfaceDetailTier tier) {
    const int count = tier == SurfaceDetailTier::Hero ? 5 : 3;
    for (int i = 0; i < count; ++i) {
        const std::uint32_t ch = h ^ (0x6d2b79f5u * static_cast<std::uint32_t>(i + 1));
        if ((ch % 7u) == 0u) continue;
        const float ox = 0.08f + static_cast<float>((ch >> 3) % 8u) * 0.105f;
        const float oz = 0.08f + static_cast<float>((ch >> 9) % 8u) * 0.105f;
        const float sx = 0.060f + static_cast<float>((ch >> 15) % 4u) * 0.028f;
        const float sz = 0.060f + static_cast<float>((ch >> 19) % 4u) * 0.026f;
        const float sy = 0.012f + static_cast<float>((ch >> 23) % 3u) * 0.009f;
        addBox(mesh, static_cast<float>(x) + ox, static_cast<float>(y + 1), static_cast<float>(z) + oz,
               sx, sy, sz, SurfaceMaterial::Dirt);
    }
}

void addLegacyTopDetail(const ChunkMeshingSnapshot& snapshot, VoxelMesh& mesh,
                        SurfaceDetailTier tier) {
    for (int z = 0; z < VoxelChunk::sizeZ; ++z) {
        for (int x = 0; x < VoxelChunk::sizeX; ++x) {
            for (int y = VoxelChunk::sizeY - 1; y >= 0; --y) {
                const BlockId block = snapshot.center.get(x, y, z);
                if (block == BlockId::Air) continue;
                if (blockAt(snapshot, x, y + 1, z) != BlockId::Air) continue;
                const std::uint32_t h = detailHash(snapshot.worldOriginX + x, y,
                                                   snapshot.worldOriginZ + z, snapshot.worldSeed);
                if (block == BlockId::Stone) {
                    const int plateCount = tier == SurfaceDetailTier::Hero ? 7 : 4;
                    for (int plate = 0; plate < plateCount; ++plate) {
                        const std::uint32_t ph = h ^ (0x9e3779b9u * static_cast<std::uint32_t>(plate + 1));
                        if ((ph % 6u) == 0u) continue;
                        const float ox = 0.035f + static_cast<float>((ph >> 4) % 8u) * 0.112f;
                        const float oz = 0.035f + static_cast<float>((ph >> 9) % 8u) * 0.112f;
                        const float width = 0.105f + static_cast<float>((ph >> 13) % 5u) * 0.040f;
                        const float depth = 0.095f + static_cast<float>((ph >> 17) % 5u) * 0.038f;
                        const float height = 0.014f + static_cast<float>((ph >> 21) % 4u) * 0.012f;
                        addBox(mesh, static_cast<float>(x) + ox, static_cast<float>(y + 1), static_cast<float>(z) + oz,
                               width, height, depth, SurfaceMaterial::Stone);
                    }
                } else if (block == BlockId::Leaves) {
                    const int clusterCount = tier == SurfaceDetailTier::Hero ? 4 : 2;
                    for (int cluster = 0; cluster < clusterCount; ++cluster) {
                        const std::uint32_t lh = h ^ (0x632be59bu * static_cast<std::uint32_t>(cluster + 1));
                        if ((lh % 5u) == 0u) continue;
                        const float ox = 0.06f + static_cast<float>((lh >> 3) % 7u) * 0.128f;
                        const float oz = 0.06f + static_cast<float>((lh >> 11) % 7u) * 0.128f;
                        const float size = 0.070f + static_cast<float>((lh >> 19) % 4u) * 0.028f;
                        addBox(mesh, static_cast<float>(x) + ox, static_cast<float>(y + 1), static_cast<float>(z) + oz,
                               size, size * 0.62f, size, SurfaceMaterial::Leaves);
                    }
                }
                break;
            }
        }
    }
}

} // namespace

void MicroDetailBuilder::append(const ChunkMeshingSnapshot& snapshot, VoxelMesh& mesh,
                                SurfaceDetailTier tier, SurfaceDetailStats* stats) {
    if (tier == SurfaceDetailTier::Distant) return;
    const auto budget = budgetFor(tier);

    for (int z = 0; z < VoxelChunk::sizeZ; ++z) {
        for (int x = 0; x < VoxelChunk::sizeX; ++x) {
            for (int y = 0; y < VoxelChunk::sizeY; ++y) {
                const BlockId block = snapshot.center.get(x, y, z);
                if (block == BlockId::Grass) {
                    if (stats) ++stats->grassBlocks;
                    if (grassTopVisible(blockAt(snapshot, x, y + 1, z))) {
                        addGrassTop(snapshot, mesh, x, y, z, nullptr, budget, stats);
                    }
                    addBlockSides(snapshot, mesh, x, y, z, true, nullptr, budget, stats);
                } else if (block == BlockId::Dirt) {
                    if (stats) ++stats->dirtBlocks;
                    if (blockAt(snapshot, x, y + 1, z) == BlockId::Air) {
                        const std::uint32_t h = detailHash(snapshot.worldOriginX + x, y,
                                                           snapshot.worldOriginZ + z, snapshot.worldSeed);
                        addSoilTopClods(mesh, x, y, z, h, tier);
                    }
                    addBlockSides(snapshot, mesh, x, y, z, false, nullptr, budget, stats);
                }
            }
        }
    }

    // Promoted blocks use the exact same deterministic 16x16 field and only mask visual cells
    // through existing 8x8 physical occupancy. Remeshing therefore cannot invent a new pattern.
    for (const auto& promoted : snapshot.microBlocks) {
        if (!promoted.owned || promoted.block != BlockId::Grass) continue;
        if (stats) {
            ++stats->grassBlocks;
            ++stats->promotedGrassBlocks;
        }
        if (grassTopVisible(blockAt(snapshot, promoted.localX, promoted.y + 1, promoted.localZ))) {
            addGrassTop(snapshot, mesh, promoted.localX, promoted.y, promoted.localZ,
                        &promoted.state, budget, stats);
        }
        addBlockSides(snapshot, mesh, promoted.localX, promoted.y, promoted.localZ,
                      true, &promoted.state, budget, stats);
    }

    addLegacyTopDetail(snapshot, mesh, tier);
}

} // namespace rf::world::meshing
