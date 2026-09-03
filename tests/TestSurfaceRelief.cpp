#include "TestSuites.h"

#include "world/meshing/MicroDetailBuilder.h"
#include "world/surface/SurfaceRelief.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <set>

namespace {

bool sameLayout(const rf::world::surface::SurfaceReliefField& a,
                const rf::world::surface::SurfaceReliefField& b) {
    for (int v = 0; v < rf::world::surface::SurfaceReliefField::resolution; ++v) {
        for (int u = 0; u < rf::world::surface::SurfaceReliefField::resolution; ++u) {
            const auto& ca = a.cell(u, v);
            const auto& cb = b.cell(u, v);
            if (ca.stableId != cb.stableId || ca.relief != cb.relief ||
                ca.vegetation != cb.vegetation || ca.bladeCount != cb.bladeCount ||
                ca.vegetationOffsetU != cb.vegetationOffsetU ||
                ca.vegetationOffsetV != cb.vegetationOffsetV) return false;
        }
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
    assert(sameLayout(fieldA, fieldB));

    // World age may mature height, but it cannot change which cells own turf/vegetation. This keeps
    // remeshing/mining from appearing to create grass that was absent before interaction.
    const auto oldField = SurfaceRelief::grassTop(seed, block, 512.0f);
    assert(sameLayout(fieldA, oldField));

    int vegetationCells = 0;
    int wideOffsetCells = 0;
    std::set<int> rowOffsets;
    for (int v = 0; v < SurfaceReliefField::resolution; ++v) {
        for (int u = 0; u < SurfaceReliefField::resolution; ++u) {
            const auto& cell = fieldA.cell(u, v);
            assert(cell.heightOffset >= 0.0f && cell.heightOffset <= 0.030f);
            assert(std::abs(cell.vegetationOffsetU) <= 0.0281f);
            assert(std::abs(cell.vegetationOffsetV) <= 0.0281f);
            if (std::abs(cell.vegetationOffsetU) > 0.021f ||
                std::abs(cell.vegetationOffsetV) > 0.021f) ++wideOffsetCells;
            if (cell.vegetation != VegetationProfile::Bare) {
                ++vegetationCells;
                assert(cell.bladeHeight > 0.020f && cell.bladeHeight <= 0.120f);
            }
            if (v == 7) rowOffsets.insert(static_cast<int>(std::lround(cell.vegetationOffsetU * 100000.0f)));
        }
    }
    assert(vegetationCells >= 200); // Dense underlying turf field before LOD geometry selection.
    assert(rowOffsets.size() >= 8); // Regression guard against a single straight blade phase.
    assert(wideOffsetCells >= 32);  // 0.6.0 was capped near +/-0.020 and left its cell lattice visible.

    const auto neighbor = SurfaceRelief::grassTop(seed, {block.x + 1, block.y, block.z}, 0.0f);
    assert(!sameLayout(fieldA, neighbor));
    assert(std::abs(fieldA.vegetationDensity - neighbor.vegetationDensity) < 0.08f);

    // Regional ecology must vary continuously. The old hard 4x4 patch hash could create square
    // density/palette boundaries that read as long bands across a meadow.
    float previousDensity = SurfaceRelief::grassTop(seed, {-24, block.y, 11}, 0.0f).vegetationDensity;
    float maxAdjacentDensityDelta = 0.0f;
    for (int x = -23; x <= 24; ++x) {
        const float density = SurfaceRelief::grassTop(seed, {x, block.y, 11}, 0.0f).vegetationDensity;
        maxAdjacentDensityDelta = std::max(maxAdjacentDensityDelta, std::abs(density - previousDensity));
        previousDensity = density;
    }
    assert(maxAdjacentDensityDelta < 0.060f);

    const auto rooted = SurfaceRelief::soilSide(seed, block, SurfaceFace::North, true);
    const auto bareDirt = SurfaceRelief::soilSide(seed, block, SurfaceFace::North, false);
    int roots = 0, cavities = 0, turf = 0, bareRoots = 0, bareTurf = 0;
    for (int v = 0; v < SurfaceReliefField::resolution; ++v) {
        for (int u = 0; u < SurfaceReliefField::resolution; ++u) {
            const auto& cell = rooted.cell(u, v);
            assert(cell.heightOffset >= 0.0f && cell.heightOffset <= 0.030f);
            if (cell.relief == ReliefClass::Root) ++roots;
            if (cell.relief == ReliefClass::Cavity) ++cavities;
            if (cell.relief == ReliefClass::Turf) ++turf;
            const auto& dirtCell = bareDirt.cell(u, v);
            if (dirtCell.relief == ReliefClass::Root) ++bareRoots;
            if (dirtCell.relief == ReliefClass::Turf) ++bareTurf;
        }
    }
    assert(roots > 0 && cavities > 0 && turf > 0);
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
    assert(canopyStats.grassClusters > 0);

    // Detail tiers have hard geometry ceilings per isolated grass block.
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
    assert(heroStats.rootCells > 0);

    // Root paths must now be narrow dedicated fibers instead of full-cell GrassSide columns.
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
    assert(standardMesh.quadCount < heroMesh.quadCount);

    VoxelMesh distantMesh;
    SurfaceDetailStats distantStats;
    MicroDetailBuilder::append(isolated, distantMesh, SurfaceDetailTier::Distant, &distantStats);
    assert(distantMesh.quadCount == 0);
    assert(distantStats.topReliefCells == 0 && distantStats.sideReliefCells == 0 &&
           distantStats.grassClusters == 0);

    // A Hero dirt face is now mostly a dense mosaic of one-quad micro-plates. This explicitly guards
    // against regressing to the sparse six-quad cuboids that looked glued onto a flat dirt wall.
    ChunkMeshingSnapshot isolatedDirt;
    isolatedDirt.worldSeed = seed;
    isolatedDirt.center.set(8, 3, 8, BlockId::Dirt);
    VoxelMesh dirtMesh;
    SurfaceDetailStats dirtStats;
    MicroDetailBuilder::append(isolatedDirt, dirtMesh, SurfaceDetailTier::Hero, &dirtStats);
    assert(dirtStats.dirtBlocks == 1);
    assert(dirtStats.sideReliefCells > 600);
    assert(dirtMesh.quadCount <= dirtStats.sideReliefCells + 40u);

    // Promoted grass uses the same 16x16 field, with exactly 2x2 visual cells mapped to one 8x8
    // physical micro cell. Removing one top physical cell must remove four Hero top plates, not
    // regenerate a different grass pattern.
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
}
