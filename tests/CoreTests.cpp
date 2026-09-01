#include "core/HubModel.h"
#include "core/Version.h"
#include "world/GreedyMesher.h"
#include "world/VoxelChunk.h"

#include <cassert>
#include <iostream>

int main() {
    const auto a = rf::Version::parse("0.1.0");
    const auto b = rf::Version::parse("v0.2.1");
    assert(a && b);
    assert(*b > *a);
    assert(a->toString() == "0.1.0");
    assert(!rf::Version::parse("0.1"));

    rf::HubModel hub;
    assert(hub.modes().size() == 6);
    assert(hub.selectedMode().id == "frontier");
    hub.selectMode(3);
    assert(hub.selectedMode().id == "labyrinth");
    hub.selectMode(999);
    assert(hub.selectedMode().id == "labyrinth");

    using namespace rf::world;

    VoxelChunk empty;
    assert(empty.solidBlockCount() == 0);
    assert(GreedyMesher::build(empty).empty());

    VoxelChunk oneBlock;
    oneBlock.set(4, 4, 4, BlockId::Stone);
    const ChunkMesh single = GreedyMesher::build(oneBlock);
    assert(single.quadCount == 6);
    assert(single.vertices.size() == 24);
    assert(single.indices.size() == 36);

    // Two adjacent equal blocks still collapse to the same six rectangular outer faces.
    VoxelChunk twoBlocks;
    twoBlocks.set(4, 4, 4, BlockId::Stone);
    twoBlocks.set(5, 4, 4, BlockId::Stone);
    const ChunkMesh mergedPair = GreedyMesher::build(twoBlocks);
    assert(mergedPair.quadCount == 6);
    assert(mergedPair.vertices.size() == 24);
    assert(mergedPair.indices.size() == 36);

    // A completely filled homogeneous chunk should become six quads, not 1,536 naive faces.
    VoxelChunk solid;
    solid.fill(BlockId::Stone);
    const ChunkMesh solidMesh = GreedyMesher::build(solid);
    assert(solidMesh.quadCount == 6);
    assert(solidMesh.vertices.size() == 24);
    assert(solidMesh.indices.size() == 36);

    // Grass top and soil sides are distinct surface materials and must survive the mesh boundary.
    VoxelChunk grass;
    grass.set(1, 1, 1, BlockId::Grass);
    const ChunkMesh grassMesh = GreedyMesher::build(grass);
    bool sawGrass = false;
    bool sawDirt = false;
    for (const auto& vertex : grassMesh.vertices) {
        sawGrass |= vertex.material == static_cast<unsigned>(SurfaceMaterial::Grass);
        sawDirt |= vertex.material == static_cast<unsigned>(SurfaceMaterial::Dirt);
    }
    assert(sawGrass && sawDirt);

    const VoxelChunk terrain = VoxelChunk::makeDemoTerrain();
    const ChunkMesh terrainMesh = GreedyMesher::build(terrain);
    assert(terrain.solidBlockCount() > 1000);
    assert(!terrainMesh.empty());
    assert(terrainMesh.indices.size() == static_cast<std::size_t>(terrainMesh.quadCount) * 6);
    assert(terrainMesh.vertices.size() == static_cast<std::size_t>(terrainMesh.quadCount) * 4);
    // Greedy meshing must be dramatically smaller than naively emitting six faces per solid block.
    assert(terrainMesh.quadCount < terrain.solidBlockCount() * 2);

    std::cout << "RuneForge core/world tests passed — demo chunk: "
              << terrain.solidBlockCount() << " solid blocks -> "
              << terrainMesh.quadCount << " greedy quads\n";
    return 0;
}
