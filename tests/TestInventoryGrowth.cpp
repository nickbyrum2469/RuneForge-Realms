#include "TestSuites.h"

#include "game/inventory/Inventory.h"
#include "game/items/ItemId.h"
#include "world/growth/GrassGrowth.h"

#include <cassert>

void runInventoryGrowthTests() {
    using namespace rf;

    game::inventory::Inventory inventory;
    const auto leftover = inventory.add(game::items::ItemId::StoneBlock, 70);
    assert(leftover == 0);
    assert(inventory.slot(0).item == game::items::ItemId::StoneBlock);
    assert(inventory.slot(0).count == 64);
    assert(inventory.slot(1).count == 6);
    inventory.selectHotbar(1);
    assert(inventory.selectedHotbar() == 1);
    assert(inventory.removeFromSlot(1, 2));
    assert(inventory.slot(1).count == 4);

    const world::BlockCoord block{17, 9, -4};
    const auto youngA = world::growth::GrassGrowth::sample(424242u, block, 3, 5, 0.0f);
    const auto youngB = world::growth::GrassGrowth::sample(424242u, block, 3, 5, 0.0f);
    assert(youngA.present == youngB.present);
    assert(youngA.stage == youngB.stage);
    assert(youngA.height == youngB.height);
    assert(youngA.flower == youngB.flower);

    const auto mature = world::growth::GrassGrowth::sample(
        424242u, block, 3, 5, world::growth::GrassGrowth::growthStepSeconds * 24.0f);
    if (youngA.present) {
        assert(youngA.stage >= 2); // Turf is already established when a chunk first appears.
        assert(mature.present);
        assert(mature.stage >= youngA.stage);
        assert(mature.height >= youngA.height);
    }

    // Grass coverage is an authored deterministic surface field, not something mining/remeshing can
    // reveal. A majority-scale sample should already be populated at world age zero and the exact
    // same nodes must remain present after later growth ticks.
    int youngPresent = 0;
    int maturePresent = 0;
    bool foundDifferent = false;
    for (int z = 0; z < world::growth::GrassGrowth::nodeResolution; ++z) {
        for (int x = 0; x < world::growth::GrassGrowth::nodeResolution; ++x) {
            const auto young = world::growth::GrassGrowth::sample(424242u, block, x, z, 0.0f);
            const auto old = world::growth::GrassGrowth::sample(
                424242u, block, x, z, world::growth::GrassGrowth::growthStepSeconds * 40.0f);
            assert(young.present == old.present);
            if (young.present) {
                ++youngPresent;
                assert(young.stage >= 2);
                assert(old.stage >= young.stage);
            }
            if (old.present) ++maturePresent;
            if (young.present != youngA.present || young.stage != youngA.stage || young.height != youngA.height) {
                foundDifferent = true;
            }
        }
    }
    assert(youngPresent >= 24); // Guards against a regression to the sparse 0.5.2 field.
    assert(maturePresent == youngPresent);
    assert(foundDifferent); // Different nodes still retain individual seeded variation.
}
