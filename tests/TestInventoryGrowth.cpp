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
        424242u, block, 3, 5, world::growth::GrassGrowth::growthStepSeconds * 8.0f);
    if (youngA.present) {
        assert(mature.present);
        assert(mature.stage >= youngA.stage);
        assert(mature.height >= youngA.height);
    }

    // Different nodes should not all share one global animation state.
    bool foundDifferent = false;
    for (int z = 0; z < world::growth::GrassGrowth::nodeResolution && !foundDifferent; ++z) {
        for (int x = 0; x < world::growth::GrassGrowth::nodeResolution && !foundDifferent; ++x) {
            const auto node = world::growth::GrassGrowth::sample(424242u, block, x, z, 0.0f);
            if (node.present != youngA.present || node.stage != youngA.stage || node.height != youngA.height) {
                foundDifferent = true;
            }
        }
    }
    assert(foundDifferent);
}
