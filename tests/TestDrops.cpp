#include "TestSuites.h"

#include "game/drops/DropSystem.h"
#include "game/inventory/Inventory.h"
#include "game/items/ItemId.h"
#include "world/FrontierWorld.h"

#include <cassert>

void runDropTests() {
    using namespace rf;

    world::FrontierWorld world;
    world.generate(424242u);
    const int groundY = world.topSolidY(0, 0);
    assert(groundY >= 0);

    game::inventory::Inventory inventory;
    game::drops::DropSystem drops;
    const game::Vec3 player{0.5f, static_cast<float>(groundY + 1), 0.5f};
    drops.spawn(game::items::ItemId::StoneBlock, 3,
                {0.55f, player.y + 0.5f, 0.55f}, {0.1f, 0.4f, 0.0f});
    assert(drops.drops().size() == 1);

    // Drops have a short pickup grace period so a newly mined item can visibly pop into the world.
    for (int i = 0; i < 5; ++i) drops.update(0.05f, world, player, inventory);
    assert(drops.drops().empty());
    assert(inventory.slot(0).item == game::items::ItemId::StoneBlock);
    assert(inventory.slot(0).count == 3);

    std::vector<game::drops::WorldDrop> saved{
        {41, game::items::ItemId::OakLog, 2, {4.0f, 9.0f, -2.0f}, {0.2f, 0.0f, 0.1f}, 2.5f},
        {77, game::items::ItemId::DirtBlock, 1, {-3.0f, 8.0f, 5.0f}, {}, 1.0f},
    };
    drops.restore(saved);
    assert(drops.drops().size() == 2);
    assert(drops.drops()[0].id == 41);
    assert(drops.drops()[1].id == 77);

    // Restored IDs must not collide with subsequently spawned drops.
    drops.spawn(game::items::ItemId::Leaves, 1, {10.0f, 10.0f, 10.0f});
    assert(drops.drops().size() == 3);
    assert(drops.drops().back().id > 77);

    drops.clear();
    assert(drops.drops().empty());
    drops.spawn(game::items::ItemId::GrassBlock, 1, {12.0f, 10.0f, 12.0f});
    assert(drops.drops().front().id == 1);
}
