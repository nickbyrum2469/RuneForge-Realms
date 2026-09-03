#include "world/meshing/MicroDetailBuilder.h"

#include "world/surface/SurfaceRelief.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace rf::world::meshing {
namespace {

using Vec3 = std::array<float, 3>;

std::uint32_t detailHash(int x, int y, int z, std::uint32_t seed) noexcept {
    std::uint32_t h = seed ^ static_cast<std::uint32_t>(x) * 0x8da6b343u;
    h ^= static_cast<std::uint32_t>(y) * 0xd8163841u;
    h ^= static_cast<std::uint32_t>(z) * 0xcb1ab31fu;
    h ^= h >> 13;
    h *= 0x85ebca6bu;
    h ^= h >> 16;
    return h;
}

Vec3 subtract(const Vec3& a, const Vec3& b) noexcept {
    return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}

Vec3 cross(const Vec3& a, const Vec3& b) noexcept {
    return {a[1] * b[2] - a[2] * b[1],
            a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0]};
}

float dot(const Vec3& a, const Vec3& b) noexcept {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

Vec3 scaled(const Vec3& value, float scale) noexcept {
    return {value[0] * scale, value[1] * scale, value[2] * scale};
}

Vec3 add(const Vec3& a, const Vec3& b) noexcept {
    return {a[0] + b[0], a[1] + b[1], a[2] + b[2]};
}

void addQuad(VoxelMesh& mesh,
             Vec3 p0, Vec3 p1, Vec3 p2, Vec3 p3,
             Vec3 normal, SurfaceMaterial material) {
    const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
    const auto vertex = [&](const Vec3& p) {
        return MeshVertex{p[0], p[1], p[2], normal[0], normal[1], normal[2], packMaterial(material)};
    };
    mesh.vertices.insert(mesh.vertices.end(), {vertex(p0), vertex(p1), vertex(p2), vertex(p3)});
    mesh.indices.insert(mesh.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    ++mesh.quadCount;
}

void addOrientedQuad(VoxelMesh& mesh,
                     Vec3 p0, Vec3 p1, Vec3 p2, Vec3 p3,
                     Vec3 normal, SurfaceMaterial material) {
    const Vec3 geometric = cross(subtract(p1, p0), subtract(p2, p0));
    if (dot(geometric, normal) < 0.0f) {
        addQuad(mesh, p0, p3, p2, p1, normal, material);
    } else {
        addQuad(mesh, p0, p1, p2, p3, normal, material);
    }
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
    constexpr float size = 0.021f;
    addBox(mesh, cx - size * 0.5f, y, cz - size * 0.5f, size, 0.010f, size, material);
}

void emitTopRiserX(VoxelMesh& mesh, float x, float z0, float z1,
                   float lowY, float highY, bool normalPositiveX,
                   SurfaceDetailStats* stats) {
    if (highY - lowY <= 0.0045f) return;
    const Vec3 normal = normalPositiveX ? Vec3{1,0,0} : Vec3{-1,0,0};
    addOrientedQuad(mesh, {x,lowY,z0}, {x,lowY,z1}, {x,highY,z1}, {x,highY,z0},
                    normal, SurfaceMaterial::GrassSide);
    if (stats) ++stats->topRiserQuads;
}

void emitTopRiserZ(VoxelMesh& mesh, float z, float x0, float x1,
                   float lowY, float highY, bool normalPositiveZ,
                   SurfaceDetailStats* stats) {
    if (highY - lowY <= 0.0045f) return;
    const Vec3 normal = normalPositiveZ ? Vec3{0,0,1} : Vec3{0,0,-1};
    addOrientedQuad(mesh, {x0,lowY,z}, {x1,lowY,z}, {x1,highY,z}, {x0,highY,z},
                    normal, SurfaceMaterial::GrassSide);
    if (stats) ++stats->topRiserQuads;
}

void addGrassTop(const ChunkMeshingSnapshot& snapshot, VoxelMesh& mesh,
                 int localX, int y, int localZ, const micro::MicroVoxelState* state,
                 const surface::SurfaceReliefBudget& budget, SurfaceDetailStats* stats) {
    const BlockCoord block{snapshot.worldOriginX + localX, y, snapshot.worldOriginZ + localZ};
    const auto field = surface::SurfaceRelief::grassTop(snapshot.worldSeed, block,
                                                         snapshot.worldAgeSeconds);
    constexpr float cellSize = 1.0f / static_cast<float>(surface::SurfaceReliefField::resolution);
    const int stride = std::max(1, budget.cellStride);
    std::uint32_t topCells = 0;

    for (int v = 0; v < surface::SurfaceReliefField::resolution &&
                    topCells < budget.maxTopReliefCells; v += stride) {
        for (int u = 0; u < surface::SurfaceReliefField::resolution &&
                        topCells < budget.maxTopReliefCells; u += stride) {
            if (!microCellVisible(state, surface::SurfaceFace::Top, u, v)) continue;
            const auto& cell = field.cell(u, v);
            const float x0 = static_cast<float>(localX) + static_cast<float>(u) * cellSize;
            const float z0 = static_cast<float>(localZ) + static_cast<float>(v) * cellSize;
            const float span = cellSize * static_cast<float>(stride);
            const float surfaceY = static_cast<float>(y + 1) + cell.heightOffset;

            if (budget.emitTopRelief) {
                addQuad(mesh,
                        {x0, surfaceY, z0 + span}, {x0 + span, surfaceY, z0 + span},
                        {x0 + span, surfaceY, z0}, {x0, surfaceY, z0},
                        {0, 1, 0}, SurfaceMaterial::GrassTop);
                ++topCells;
                if (stats) ++stats->topReliefCells;
            }

            // Hero-only sparse vertical risers make the top bed read as actual little turf masses.
            // We only close meaningful height differences, not every cell edge, avoiding a new grid.
            if (budget.emitSideWalls && stride == 1) {
                if (u + 1 < surface::SurfaceReliefField::resolution &&
                    microCellVisible(state, surface::SurfaceFace::Top, u + 1, v)) {
                    const float neighborY = static_cast<float>(y + 1) + field.cell(u + 1, v).heightOffset;
                    if (surfaceY > neighborY) {
                        emitTopRiserX(mesh, x0 + cellSize, z0, z0 + cellSize,
                                      neighborY, surfaceY, true, stats);
                    } else {
                        emitTopRiserX(mesh, x0 + cellSize, z0, z0 + cellSize,
                                      surfaceY, neighborY, false, stats);
                    }
                }
                if (v + 1 < surface::SurfaceReliefField::resolution &&
                    microCellVisible(state, surface::SurfaceFace::Top, u, v + 1)) {
                    const float neighborY = static_cast<float>(y + 1) + field.cell(u, v + 1).heightOffset;
                    if (surfaceY > neighborY) {
                        emitTopRiserZ(mesh, z0 + cellSize, x0, x0 + cellSize,
                                      neighborY, surfaceY, true, stats);
                    } else {
                        emitTopRiserZ(mesh, z0 + cellSize, x0, x0 + cellSize,
                                      surfaceY, neighborY, false, stats);
                    }
                }
            }
        }
    }

    // R2 anchors are progressive: taking the first N remains spatially even, so Standard can use a
    // strict geometry budget without reintroducing row-biased modulo selection.
    std::uint32_t renderedAnchors = 0;
    for (std::size_t i = 0; i < field.grassAnchorCount &&
                           renderedAnchors < budget.maxVegetationCells; ++i) {
        const auto& anchor = field.grassAnchor(i);
        if (!microCellVisible(state, surface::SurfaceFace::Top,
                              anchor.ownerU, anchor.ownerV)) continue;
        const auto& owner = field.cell(anchor.ownerU, anchor.ownerV);
        const float surfaceY = static_cast<float>(y + 1) + owner.heightOffset;
        const float cx = static_cast<float>(localX) + anchor.localU;
        const float cz = static_cast<float>(localZ) + anchor.localV;
        const float width = (0.0100f + static_cast<float>(anchor.stableId & 3u) * 0.0017f) *
                            anchor.widthScale;
        const float depth = (0.0105f + static_cast<float>((anchor.stableId >> 2) & 3u) * 0.0015f) *
                            anchor.widthScale;

        addBox(mesh, cx - width * 0.5f, surfaceY, cz - depth * 0.5f,
               width, anchor.bladeHeight, depth, SurfaceMaterial::GrassTop);

        if (anchor.bladeCount >= 2) {
            const float companion = anchor.bladeHeight * (0.56f +
                static_cast<float>((anchor.stableId >> 5) & 3u) * 0.075f);
            const float offset = 0.015f + static_cast<float>((anchor.stableId >> 10) & 3u) * 0.004f;
            const float signU = ((anchor.stableId >> 8) & 1u) ? 1.0f : -1.0f;
            const float signV = ((anchor.stableId >> 9) & 1u) ? 1.0f : -1.0f;
            addBox(mesh, cx + signU * offset - width * 0.34f, surfaceY,
                   cz + signV * offset - depth * 0.34f,
                   width * 0.68f, companion, depth * 0.68f, SurfaceMaterial::GrassTop);
        }
        if (anchor.bladeCount >= 3) {
            const float thirdHeight = anchor.bladeHeight * (0.45f +
                static_cast<float>((anchor.stableId >> 12) & 3u) * 0.065f);
            const float sign = ((anchor.stableId >> 14) & 1u) ? 1.0f : -1.0f;
            addBox(mesh, cx - sign * 0.011f - width * 0.27f, surfaceY,
                   cz + sign * 0.017f - depth * 0.27f,
                   width * 0.54f, thirdHeight, depth * 0.54f, SurfaceMaterial::GrassTop);
        }
        if (anchor.vegetation == surface::VegetationProfile::FlowerCapable &&
            (anchor.stableId % 7u) == 0u) {
            addFlowerCap(mesh, cx, surfaceY + anchor.bladeHeight, cz, anchor.stableId);
        }
        ++renderedAnchors;
        if (stats) ++stats->grassClusters;
    }
}

SurfaceMaterial sideMaterial(bool grassBlock, surface::ReliefClass relief) noexcept {
    if (relief == surface::ReliefClass::Mineral) return SurfaceMaterial::Stone;
    if (grassBlock && relief == surface::ReliefClass::Turf) return SurfaceMaterial::GrassSide;
    return SurfaceMaterial::Dirt;
}

struct FaceBasis {
    Vec3 origin{};
    Vec3 u{};
    Vec3 v{};
    Vec3 outward{};
};

FaceBasis faceBasis(int x, int y, int z, surface::SurfaceFace face) noexcept {
    switch (face) {
        case surface::SurfaceFace::North:
            return {{static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)},
                    {1,0,0}, {0,1,0}, {0,0,-1}};
        case surface::SurfaceFace::South:
            return {{static_cast<float>(x), static_cast<float>(y), static_cast<float>(z + 1)},
                    {1,0,0}, {0,1,0}, {0,0,1}};
        case surface::SurfaceFace::East:
            return {{static_cast<float>(x + 1), static_cast<float>(y), static_cast<float>(z)},
                    {0,0,1}, {0,1,0}, {1,0,0}};
        case surface::SurfaceFace::West:
            return {{static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)},
                    {0,0,1}, {0,1,0}, {-1,0,0}};
        case surface::SurfaceFace::Top:
            break;
    }
    return {};
}

Vec3 facePoint(const FaceBasis& basis, float u, float v, float depth) noexcept {
    return add(add(add(basis.origin, scaled(basis.u, u)), scaled(basis.v, v)),
               scaled(basis.outward, depth));
}

void emitSideFront(VoxelMesh& mesh, const FaceBasis& basis,
                   float u0, float v0, float u1, float v1, float depth,
                   SurfaceMaterial material) {
    addOrientedQuad(mesh,
                    facePoint(basis, u0, v0, depth),
                    facePoint(basis, u1, v0, depth),
                    facePoint(basis, u1, v1, depth),
                    facePoint(basis, u0, v1, depth),
                    basis.outward, material);
}

void emitSideUWall(VoxelMesh& mesh, const FaceBasis& basis, float boundaryU,
                   float v0, float v1, float innerDepth, float outerDepth,
                   bool positiveU, SurfaceMaterial material) {
    if (outerDepth - innerDepth <= 0.0020f) return;
    const Vec3 normal = scaled(basis.u, positiveU ? 1.0f : -1.0f);
    addOrientedQuad(mesh,
                    facePoint(basis, boundaryU, v0, innerDepth),
                    facePoint(basis, boundaryU, v0, outerDepth),
                    facePoint(basis, boundaryU, v1, outerDepth),
                    facePoint(basis, boundaryU, v1, innerDepth),
                    normal, material);
}

void emitSideVWall(VoxelMesh& mesh, const FaceBasis& basis, float boundaryV,
                   float u0, float u1, float innerDepth, float outerDepth,
                   bool positiveV, SurfaceMaterial material) {
    if (outerDepth - innerDepth <= 0.0020f) return;
    const Vec3 normal = scaled(basis.v, positiveV ? 1.0f : -1.0f);
    addOrientedQuad(mesh,
                    facePoint(basis, u0, boundaryV, innerDepth),
                    facePoint(basis, u1, boundaryV, innerDepth),
                    facePoint(basis, u1, boundaryV, outerDepth),
                    facePoint(basis, u0, boundaryV, outerDepth),
                    normal, material);
}

float visibleCellDepth(const surface::SurfaceReliefField& field,
                       const micro::MicroVoxelState* state, surface::SurfaceFace face,
                       int u, int v) noexcept {
    if (u < 0 || v < 0 || u >= surface::SurfaceReliefField::resolution ||
        v >= surface::SurfaceReliefField::resolution) return 0.0f;
    if (!microCellVisible(state, face, u, v)) return 0.0f;
    const auto& cell = field.cell(u, v);
    return (cell.occupied && !cell.cavity) ? cell.heightOffset : 0.0f;
}

void emitSideShellCell(VoxelMesh& mesh, int x, int y, int z, surface::SurfaceFace face,
                       const surface::SurfaceReliefField& field,
                       const micro::MicroVoxelState* state,
                       int u, int v, int stride, const surface::SurfaceCell& cell,
                       bool grassBlock, bool emitWalls, SurfaceDetailStats* stats) {
    constexpr float cellSize = 1.0f / static_cast<float>(surface::SurfaceReliefField::resolution);
    const float u0 = static_cast<float>(u) * cellSize;
    const float v0 = static_cast<float>(v) * cellSize;
    const float u1 = u0 + cellSize * static_cast<float>(stride);
    const float v1 = v0 + cellSize * static_cast<float>(stride);
    const float depth = cell.heightOffset;
    const SurfaceMaterial material = sideMaterial(grassBlock, cell.relief);
    const FaceBasis basis = faceBasis(x, y, z, face);

    emitSideFront(mesh, basis, u0, v0, u1, v1, depth, material);
    if (!emitWalls || stride != 1) return;

    const float leftDepth = visibleCellDepth(field, state, face, u - 1, v);
    const float rightDepth = visibleCellDepth(field, state, face, u + 1, v);
    const float downDepth = visibleCellDepth(field, state, face, u, v - 1);
    const float upDepth = visibleCellDepth(field, state, face, u, v + 1);

    if (depth > leftDepth + 0.0020f) {
        emitSideUWall(mesh, basis, u0, v0, v1, leftDepth, depth, false, material);
        if (stats) ++stats->sideWallQuads;
    }
    if (depth > rightDepth + 0.0020f) {
        emitSideUWall(mesh, basis, u1, v0, v1, rightDepth, depth, true, material);
        if (stats) ++stats->sideWallQuads;
    }
    if (depth > downDepth + 0.0020f) {
        emitSideVWall(mesh, basis, v0, u0, u1, downDepth, depth, false, material);
        if (stats) ++stats->sideWallQuads;
    }
    if (depth > upDepth + 0.0020f) {
        emitSideVWall(mesh, basis, v1, u0, u1, upDepth, depth, true, material);
        if (stats) ++stats->sideWallQuads;
    }
}

void emitRootSegment(VoxelMesh& mesh, int x, int y, int z, surface::SurfaceFace face,
                     const surface::SurfaceReliefField& field,
                     const surface::RootSegment& segment) {
    const float midU = (segment.u0 + segment.u1) * 0.5f;
    const float midV = (segment.v0 + segment.v1) * 0.5f;
    const int cellU = std::clamp(static_cast<int>(midU * surface::SurfaceReliefField::resolution),
                                 0, surface::SurfaceReliefField::resolution - 1);
    const int cellV = std::clamp(static_cast<int>(midV * surface::SurfaceReliefField::resolution),
                                 0, surface::SurfaceReliefField::resolution - 1);
    const float soilDepth = std::max(field.cell(cellU, cellV).heightOffset, 0.018f);
    const float projection = soilDepth + segment.projection;
    const float fiberDepth = 0.0065f;
    const bool vertical = std::abs(segment.v1 - segment.v0) >= std::abs(segment.u1 - segment.u0);

    if (vertical) {
        const float localU = (segment.u0 + segment.u1) * 0.5f;
        const float v0 = std::min(segment.v0, segment.v1);
        const float length = std::max(std::abs(segment.v1 - segment.v0), 0.008f);
        switch (face) {
            case surface::SurfaceFace::North:
                addBox(mesh, static_cast<float>(x) + localU - segment.width * 0.5f,
                       static_cast<float>(y) + v0,
                       static_cast<float>(z) - projection - fiberDepth,
                       segment.width, length, fiberDepth, SurfaceMaterial::RootFiber);
                break;
            case surface::SurfaceFace::South:
                addBox(mesh, static_cast<float>(x) + localU - segment.width * 0.5f,
                       static_cast<float>(y) + v0,
                       static_cast<float>(z + 1) + projection,
                       segment.width, length, fiberDepth, SurfaceMaterial::RootFiber);
                break;
            case surface::SurfaceFace::East:
                addBox(mesh, static_cast<float>(x + 1) + projection,
                       static_cast<float>(y) + v0,
                       static_cast<float>(z) + localU - segment.width * 0.5f,
                       fiberDepth, length, segment.width, SurfaceMaterial::RootFiber);
                break;
            case surface::SurfaceFace::West:
                addBox(mesh, static_cast<float>(x) - projection - fiberDepth,
                       static_cast<float>(y) + v0,
                       static_cast<float>(z) + localU - segment.width * 0.5f,
                       fiberDepth, length, segment.width, SurfaceMaterial::RootFiber);
                break;
            case surface::SurfaceFace::Top: break;
        }
    } else {
        const float u0 = std::min(segment.u0, segment.u1);
        const float length = std::max(std::abs(segment.u1 - segment.u0), 0.008f);
        const float localV = (segment.v0 + segment.v1) * 0.5f;
        switch (face) {
            case surface::SurfaceFace::North:
                addBox(mesh, static_cast<float>(x) + u0,
                       static_cast<float>(y) + localV - segment.width * 0.5f,
                       static_cast<float>(z) - projection - fiberDepth,
                       length, segment.width, fiberDepth, SurfaceMaterial::RootFiber);
                break;
            case surface::SurfaceFace::South:
                addBox(mesh, static_cast<float>(x) + u0,
                       static_cast<float>(y) + localV - segment.width * 0.5f,
                       static_cast<float>(z + 1) + projection,
                       length, segment.width, fiberDepth, SurfaceMaterial::RootFiber);
                break;
            case surface::SurfaceFace::East:
                addBox(mesh, static_cast<float>(x + 1) + projection,
                       static_cast<float>(y) + localV - segment.width * 0.5f,
                       static_cast<float>(z) + u0,
                       fiberDepth, segment.width, length, SurfaceMaterial::RootFiber);
                break;
            case surface::SurfaceFace::West:
                addBox(mesh, static_cast<float>(x) - projection - fiberDepth,
                       static_cast<float>(y) + localV - segment.width * 0.5f,
                       static_cast<float>(z) + u0,
                       fiberDepth, segment.width, length, SurfaceMaterial::RootFiber);
                break;
            case surface::SurfaceFace::Top: break;
        }
    }
}

void addSoilSide(const ChunkMeshingSnapshot& snapshot, VoxelMesh& mesh,
                 int localX, int y, int localZ, surface::SurfaceFace face,
                 bool grassBlock, const micro::MicroVoxelState* state,
                 const surface::SurfaceReliefBudget& budget, SurfaceDetailStats* stats) {
    if (!budget.emitSideRelief || budget.maxSideReliefCells == 0) return;
    const BlockCoord block{snapshot.worldOriginX + localX, y, snapshot.worldOriginZ + localZ};
    const auto field = surface::SurfaceRelief::soilSide(snapshot.worldSeed, block, face, grassBlock);
    const int stride = std::max(1, budget.cellStride);
    std::uint32_t emitted = 0;

    for (int v = 0; v < surface::SurfaceReliefField::resolution &&
                    emitted < budget.maxSideReliefCells; v += stride) {
        for (int u = 0; u < surface::SurfaceReliefField::resolution &&
                        emitted < budget.maxSideReliefCells; u += stride) {
            if (!microCellVisible(state, face, u, v)) continue;
            const auto& cell = field.cell(u, v);
            if (cell.cavity || !cell.occupied) {
                if (stats) ++stats->cavityCells;
                continue;
            }
            emitSideShellCell(mesh, localX, y, localZ, face, field, state,
                              u, v, stride, cell, grassBlock, budget.emitSideWalls, stats);
            ++emitted;
            if (stats) {
                ++stats->sideReliefCells;
                if (cell.relief == surface::ReliefClass::Root) ++stats->rootCells;
            }
        }
    }

    if (!budget.emitRoots || !grassBlock || budget.maxRootSegments == 0) return;
    std::uint32_t rootsEmitted = 0;
    for (std::size_t i = 0; i < field.rootSegmentCount &&
                           rootsEmitted < budget.maxRootSegments; ++i) {
        const auto& segment = field.rootSegment(i);
        const float midU = (segment.u0 + segment.u1) * 0.5f;
        const float midV = (segment.v0 + segment.v1) * 0.5f;
        const int ownerU = std::clamp(static_cast<int>(midU * surface::SurfaceReliefField::resolution),
                                      0, surface::SurfaceReliefField::resolution - 1);
        const int ownerV = std::clamp(static_cast<int>(midV * surface::SurfaceReliefField::resolution),
                                      0, surface::SurfaceReliefField::resolution - 1);
        if (!microCellVisible(state, face, ownerU, ownerV)) continue;
        if (!field.cell(ownerU, ownerV).occupied) continue;
        emitRootSegment(mesh, localX, y, localZ, face, field, segment);
        ++rootsEmitted;
        if (stats) ++stats->rootSegments;
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
