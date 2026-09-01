#include "TestSuites.h"

#include "game/mining/MiningSystem.h"
#include "world/FrontierWorld.h"
#include "world/micro/MicroVoxelEdit.h"
#include "world/micro/MicroVoxelState.h"

#include <cassert>

namespace {

rf::world::RaycastHit hitFor(rf::world::BlockCoord block) {
    return {
        true,
        block,
        {block.x, block.y + 1, block.z},
        {4, 7, 4},
        static_cast<float>(block.x) + 0.56f,
        static_cast<float>(block.y) + 0.98f,
        static_cast<float>(block.z) + 0.56f,
        true,
    };
}

rf::world::BlockCoord surfaceAtOrigin(rf::world::FrontierWorld& world) {
    const int y = world.topSolidY(0, 0);
    assert(y > 0);
    return {0, y, 0};
}

} // namespace

void runMicroMiningTests() {
    using namespace rf;

    world::micro::MicroVoxelState state;
    assert(state.full());
    assert(state.occupiedCount() == static_cast<std::size_t>(world::micro::cellCount));
    const auto removed = state.clearSphere({4, 4, 4}, 1);
    assert(removed > 0 && removed < static_cast<std::size_t>(world::micro::cellCount));
    assert(!state.full() && !state.empty());

    const world::BlockCoord editPosition{3, 8, -2};
    const auto edit = world::micro::makeEdit(editPosition, world::BlockId::Stone, state);
    const auto restored = world::micro::stateFromEdit(edit);
    assert(restored.bits() == state.bits());

    {
        world::FrontierWorld world;
        world.generate(1001);
        const auto target = surfaceAtOrigin(world);
        const auto before = world.getBlock(target.x, target.y, target.z);
        const auto chip = world.chipBlock(target,
                                          static_cast<float>(target.x) + 0.5f,
                                          static_cast<float>(target.y) + 0.98f,
                                          static_cast<float>(target.z) + 0.5f, 1);
        assert(chip.changed);
        assert(!chip.emptied);
        assert(chip.removedCells > 0);
        assert(world.getBlock(target.x, target.y, target.z) == before);
        assert(world.microState(target) != nullptr);
        assert(world.promotedBlockCount() == 1);
        const auto snapshot = world.chunkMeshingSnapshot(world::chunkFromBlock(target.x, target.z));
        assert(snapshot && !snapshot->microBlocks.empty());
    }

    {
        world::FrontierWorld world;
        world.generate(1002);
        const auto target = surfaceAtOrigin(world);
        game::mining::MiningSystem mining;
        mining.setMode(game::mining::MiningMode::Block);
        const auto hit = hitFor(target);
        bool broke = false;
        for (int i = 0; i < 30 && !broke; ++i) broke = mining.strike(world, hit).brokeBlock;
        assert(broke);
        assert(world.getBlock(target.x, target.y, target.z) == world::BlockId::Air);
        assert(world.microState(target) == nullptr);
    }

    {
        world::FrontierWorld world;
        world.generate(1003);
        const auto target = surfaceAtOrigin(world);
        const auto original = world.getBlock(target.x, target.y, target.z);
        game::mining::MiningSystem mining;
        mining.setMode(game::mining::MiningMode::Micro);
        const auto outcome = mining.strike(world, hitFor(target));
        assert(outcome.affected);
        assert(!outcome.brokeBlock);
        assert(outcome.microCellsRemoved > 0);
        assert(world.getBlock(target.x, target.y, target.z) == original);
        assert(world.microState(target) != nullptr);
    }

    {
        world::FrontierWorld world;
        world.generate(1004);
        const auto target = surfaceAtOrigin(world);
        game::mining::MiningSystem mining;
        mining.setMode(game::mining::MiningMode::Mixed);
        const auto first = mining.strike(world, hitFor(target));
        assert(first.affected);
        assert(first.microCellsRemoved > 0);
        bool broke = first.brokeBlock;
        for (int i = 0; i < 30 && !broke; ++i) broke = mining.strike(world, hitFor(target)).brokeBlock;
        assert(broke);
        assert(world.getBlock(target.x, target.y, target.z) == world::BlockId::Air);
    }
}
