#include "TestSuites.h"

#include "world/meshing/MicroDetailBuilder.h"
#include "world/surface/SurfaceRelief.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <set>

namespace {

bool sameCellLayout(const rf::world::surface::SurfaceReliefField& a,
                    const rf::world::surface::SurfaceReliefField& b) {
    for (int v = 0; v < rf::world::surface::SurfaceReliefField::resolution; ++v) {
        for (int u = 0; u < rf::world::surface::SurfaceReliefField::resolution; ++u) {
            const auto& ca = a.cell(u, v);
            const auto& cb = b.cell(u, v);
            if (ca.stableId != cb.stableId || ca.relief != cb.relief ||
                ca.occupied != cb.occupied || ca.cavity != cb.cavity ||
                ca.heightOffset != cb.heightOffset) return false;
        }
    }
    return true;
}

bool sameGrassAnchorLayout(const rf::world::surface::SurfaceReliefField& a,
                           const rf::world::surface::SurfaceReliefField& b) {
    if (a.grassAnchorCount != b.grassAnchorCount) return false;
    for (std::size_t i = 0; i < a.grassAnchorCount; ++i) {
        const auto& aa = a.grassAnchor(i);
        const auto& ab = b.grassAnchor(i);
        if (aa.stableId != ab.stableId || aa.localU != ab.localU || aa.localV != ab.localV ||
            aa.ownerU != ab.ownerU || aa.ownerV != ab.ownerV ||
            aa.vegetation != ab.vegetation || aa.bladeCount != ab.bladeCount) return false;
    }
    return true;
}

bool sameRootLayout(const rf::world::surface::SurfaceReliefField& a,
                    const rf::world::surface::SurfaceReliefField& b) {
    if (a.rootSegmentCount != b.rootSegmentCount) return false;
    for (std::size_t i = 0; i < a.rootSegmentCount; ++i) {
        const auto& ra = a.rootSegment(i);
        const auto& rb = b.rootSegment(i);
        if (ra.stableId != rb.stableId || ra.u0 != rb.u0 || ra.v0 != rb.v0 ||
            ra.u1 != rb.u1 || ra.v1 != rb.v1 || ra.width != rb.width ||
            ra.projection != rb.projection) return false;
    }
    return true;
}

} // namespace

void runSurfaceReliefTests() {
    using namespace rf::world;
    using namespace rf::world::surface;
    using namespace rf::world::meshing;

    constexpr std::uint32_t seed = 0x51a7f00du;
    const BlockCoord block{17, 5, -9};

    const auto fieldA = SurfaceRelief::grassTop(seed, block, 0.0f);
    const auto fieldB = SurfaceRelief::grassTop(seed, block, 0.0f);
    assert(sameCellLayout(fieldA, fieldB));
    assert(sameGrassAnchorLayout(fieldA, fieldB));

    // World age may mature blade height, but cannot move/reseed the underlying physical turf or
    // visible grass anchors. Mining/remeshing therefore cannot invent a different meadow pattern.
    const auto oldField = SurfaceRelief::grassTop(seed, block, 512.0f);
    assert(sameCellLayout(fieldA, oldField));
    assert(sameGrassAnchorLayout(fieldA, oldField));

    assert(fieldA.grassAnchorCount >= 120 && fieldA.grassAnchorCount <= 132);
    std::set<int> ownerCells;
    int nearCellEdge = 0;
    float nearestMin = std::numeric_limits<float>::max();
    float nearestMax = 0.0f;

    for (std::size_t i = 0; i < fieldA.grassAnchorCount; ++i) {
        const auto& anchor = fieldA.grassAnchor(i);
        assert(anchor.localU >= 0.0f && anchor.localU < 1.0f);
        assert(anchor.localV >= 0.0f && anchor.localV < 1.0f);
        assert(anchor.ownerU < SurfaceReliefField::resolution);
        assert(anchor.ownerV < SurfaceReliefField::resolution);
        assert(anchor.bladeHeight > 0.018f && anchor.bladeHeight <= 0.110f);
        ownerCells.insert(static_cast<int>(anchor.ownerU) +
                          static_cast<int>(anchor.ownerV) * SurfaceReliefField::resolution);

        // Critical anti-lattice guard: old 0.6.0/0.6.1 vegetation was generated from cell centers
        // and could never approach the cell boundary closer than roughly 0.00325 world units. The
        // independent R2 point field must genuinely occupy the whole surface domain.
        const float phaseU = anchor.localU * static_cast<float>(SurfaceReliefField::resolution);
        const float phaseV = anchor.localV * static_cast<float>(SurfaceReliefField::resolution);
        const float localU = phaseU - std::floor(phaseU);
        const float localV = phaseV - std::floor(phaseV);
        const float edgeU = std::min(localU, 1.0f - localU);
        const float edgeV = std::min(localV, 1.0f - localV);
        if (edgeU < 0.050f || edgeV < 0.050f) ++nearCellEdge;

        float nearest = std::numeric_limits<float>::max();
        for (std::size_t j = 0; j < fieldA.grassAnchorCount; ++j) {
            if (i == j) continue;
            const auto& other = fieldA.grassAnchor(j);
            const float du = anchor.localU - other.localU;
            const float dv = anchor.localV - other.localV;
            nearest = std::min(nearest, std::sqrt(du * du + dv * dv));
        }
        nearestMin = std::min(nearestMin, nearest);
        nearestMax = std::max(nearestMax, nearest);
    }
    assert(ownerCells.size() >= 100);  // Even coverage, not a few noisy clumps.
    assert(nearCellEdge >= 10);        // Proves placement is not cell-center jitter anymore.
    assert(nearestMin > 0.025f);       // Controlled/blue-noise-like: no random garbage clumping.
    assert(nearestMax < 0.145f);       // No bald holes inside a mature block.

    for (int v = 0; v < SurfaceReliefField::resolution; ++v) {
        for (int u = 0; u < SurfaceReliefField::resolution; ++u) {
            const auto& cell = fieldA.cell(u, v);
            assert(cell.heightOffset >= 0.0f && cell.heightOffset <= 0.026f);
        }
    }

    const auto neighbor = SurfaceRelief::grassTop(seed, {block.x + 1, block.y, block.z}, 0.0f);
    assert(!sameGrassAnchorLayout(fieldA, neighbor));
    assert(std::abs(fieldA.vegetationDensity - neighbor.vegetationDensity) < 0.06f);

    // Regional ecology must remain smooth across block boundaries rather than exposing a hard patch
    // lattice. This is independent of the per-block R2 scramble.
    float previousDensity = SurfaceRelief::grassTop(seed, {-24, block.y, 11}, 0.0f).vegetationDensity;
    float maxAdjacentDensityDelta = 0.0f;
    for (int x = -23; x <= 24; ++x) {
        const float density = SurfaceRelief::grassTop(seed, {x, block.y, 11}, 0.0f).vegetationDensity;
        maxAdjacentDensityDelta = std::max(maxAdjacentDensityDelta, std::abs(density - previousDensity));
        previousDensity = density;
    }
    assert(maxAdjacentDensityDelta < 0.050f);

    const auto rooted = SurfaceRelief::soilSide(seed, block, SurfaceFace::North, true);
    const auto rootedAgain = SurfaceRelief::soilSide(seed, block, SurfaceFace::North, true);
    const auto bareDirt = SurfaceRelief::soilSide(seed, block, SurfaceFace::North, false);
    assert(sameCellLayout(rooted, rootedAgain));
    assert(sameRootLayout(rooted, rootedAgain));

    int roots = 0, cavities = 0, turf = 0, occupied = 0, bareRoots = 0, bareTurf = 0;
    float minDepth = std::numeric_limits<float>::max();
    float maxDepth = 0.0f;
    for (int v = 0; v < SurfaceReliefField::resolution; ++v) {
        for (int u = 0; u < SurfaceReliefField::resolution; ++u) {
            const auto& cell = rooted.cell(u, v);
            assert(cell.heightOffset >= 0.0f && cell.heightOffset <= 0.067f);
            if (cell.relief == ReliefClass::Root) ++roots;
            if (cell.relief == ReliefClass::Cavity) ++cavities;
            if (cell.relief == ReliefClass::Turf) ++turf;
            if (cell.occupied) {
                ++occupied;
                minDepth = std::min(minDepth, cell.heightOffset);
                maxDepth = std::max(maxDepth, cell.heightOffset);
            }
            const auto& dirtCell = bareDirt.cell(u, v);
            if (dirtCell.relief == ReliefClass::Root) ++bareRoots;
            if (dirtCell.relief == ReliefClass::Turf) ++bareTurf;
        }
    }
    assert(occupied > 220);
    assert(cavities >= 5 && cavities <= 30);
    assert(turf >= 40);
    assert(roots > 0);
    assert(maxDepth - minDepth > 0.030f); // Real structural depth, not a 1-2% paper offset.
    assert(rooted.rootSegmentCount >= 14 && rooted.rootSegmentCount <= SurfaceReliefField::maxRootSegments);
    assert(bareDirt.rootSegmentCount == 0);
    assert(bareRoots == 0 && bareTurf == 0);

    assert(SurfaceRelief::microCellForVisualCell(0) == 0);
    assert(SurfaceRelief::microCellForVisualCell(1) == 0);
    assert(SurfaceRelief::microCellForVisualCell(2) == 1);
    assert(SurfaceRelief::microCellForVisualCell(15) == 7);

    // Canopy regression: leaves above a grass block must not steal the ground's surface-detail pass.
    ChunkMeshingSnapshot canopy;
    canopy.worldSeed = seed;
    canopy.worldOriginX = 0;
    canopy.worldOriginZ = 0;
    canopy.center.set(4, 2, 4, BlockId::Grass);
    canopy.center.set(4, 8, 4, BlockId::Leaves);
    VoxelMesh canopyMesh;
    SurfaceDetailStats canopyStats;
    MicroDetailBuilder::append(canopy, canopyMesh, SurfaceDetailTier::Hero, &canopyStats);
    assert(canopyStats.grassBlocks == 1);
    assert(canopyStats.topReliefCells == SurfaceReliefField::cellCount);
    assert(canopyStats.grassClusters >= 115);

    ChunkMeshingSnapshot isolated;
    isolated.worldSeed = seed;
    isolated.center.set(7, 3, 7, BlockId::Grass);
    VoxelMesh heroMesh;
    SurfaceDetailStats heroStats;
    MicroDetailBuilder::append(isolated, heroMesh, SurfaceDetailTier::Hero, &heroStats);
    const auto heroBudget = SurfaceRelief::heroBudget();
    assert(heroStats.topReliefCells <= heroBudget.maxTopReliefCells);
    assert(heroStats.sideReliefCells <= heroBudget.maxSideReliefCells * 4u);
    assert(heroStats.grassClusters <= heroBudget.maxVegetationCells);
    assert(heroStats.topRiserQuads > 0);
    assert(heroStats.sideWallQuads > 0);
    assert(heroStats.rootCells > 0);
    assert(heroStats.rootSegments > 0);
    assert(heroStats.rootSegments <= heroBudget.maxRootSegments * 4u);
    assert(heroMesh.quadCount < 10000u); // Explicit Hero upper bound for one fully exposed block.

    bool foundRootFiber = false;
    for (const auto& vertex : heroMesh.vertices) {
        if ((vertex.material & surfaceMaterialMask) == static_cast<std::uint32_t>(SurfaceMaterial::RootFiber)) {
            foundRootFiber = true;
            break;
        }
    }
    assert(foundRootFiber);

    VoxelMesh standardMesh;
    SurfaceDetailStats standardStats;
    MicroDetailBuilder::append(isolated, standardMesh, SurfaceDetailTier::Standard, &standardStats);
    const auto standardBudget = SurfaceRelief::standardBudget();
    assert(standardStats.topReliefCells <= standardBudget.maxTopReliefCells);
    assert(standardStats.sideReliefCells <= standardBudget.maxSideReliefCells * 4u);
    assert(standardStats.grassClusters <= standardBudget.maxVegetationCells);
    assert(standardStats.sideWallQuads == 0); // Mid range keeps a cheap front shell.
    assert(standardMesh.quadCount < heroMesh.quadCount);

    VoxelMesh distantMesh;
    SurfaceDetailStats distantStats;
    MicroDetailBuilder::append(isolated, distantMesh, SurfaceDetailTier::Distant, &distantStats);
    assert(distantMesh.quadCount == 0);
    assert(distantStats.topReliefCells == 0 && distantStats.sideReliefCells == 0 &&
           distantStats.grassClusters == 0);

    // Hero dirt must now be a connected relief shell. The old 0.6.1 implementation emitted only
    // paper-thin front quads; from a grazing camera those collapsed into detached flat fragments.
    ChunkMeshingSnapshot isolatedDirt;
    isolatedDirt.worldSeed = seed;
    isolatedDirt.center.set(8, 3, 8, BlockId::Dirt);
    VoxelMesh dirtMesh;
    SurfaceDetailStats dirtStats;
    MicroDetailBuilder::append(isolatedDirt, dirtMesh, SurfaceDetailTier::Hero, &dirtStats);
    assert(dirtStats.dirtBlocks == 1);
    assert(dirtStats.sideReliefCells > 850);
    assert(dirtStats.sideWallQuads > 400);
    assert(dirtMesh.quadCount >= dirtStats.sideReliefCells + dirtStats.sideWallQuads);
    assert(dirtMesh.quadCount < 5000u);

    // Promoted grass uses the same deterministic 16x16 physical address field. Removing one 8x8
    // top micro voxel removes exactly its four owning turf cells rather than regenerating the block.
    ChunkMeshingSnapshot promoted;
    promoted.worldSeed = seed;
    micro::MicroVoxelState fullState;
    promoted.microBlocks.push_back({5, 2, 5, BlockId::Grass, fullState, true});
    VoxelMesh promotedFullMesh;
    SurfaceDetailStats promotedFullStats;
    MicroDetailBuilder::append(promoted, promotedFullMesh, SurfaceDetailTier::Hero, &promotedFullStats);
    assert(promotedFullStats.promotedGrassBlocks == 1);
    assert(promotedFullStats.topReliefCells == SurfaceReliefField::cellCount);

    micro::MicroVoxelState chippedState;
    assert(chippedState.setOccupied(3, micro::resolution - 1, 4, false));
    promoted.microBlocks.clear();
    promoted.microBlocks.push_back({5, 2, 5, BlockId::Grass, chippedState, true});
    VoxelMesh promotedChippedMesh;
    SurfaceDetailStats promotedChippedStats;
    MicroDetailBuilder::append(promoted, promotedChippedMesh, SurfaceDetailTier::Hero, &promotedChippedStats);
    assert(promotedChippedStats.topReliefCells == SurfaceReliefField::cellCount - 4);
    assert(promotedChippedStats.topReliefCells < promotedFullStats.topReliefCells);
    assert(promotedChippedStats.grassClusters <= promotedFullStats.grassClusters);
}
