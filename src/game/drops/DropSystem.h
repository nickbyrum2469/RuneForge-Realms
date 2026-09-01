#pragma once

#include "game/Math.h"
#include "game/inventory/Inventory.h"
#include "game/items/ItemId.h"

#include <cstdint>
#include <vector>

namespace rf::world { class FrontierWorld; }

namespace rf::game::drops {

struct WorldDrop {
    std::uint64_t id{};
    items::ItemId item{items::ItemId::None};
    std::uint16_t count{1};
    Vec3 position{};
    Vec3 velocity{};
    float age{};
};

class DropSystem {
public:
    void spawn(items::ItemId item, std::uint16_t count, Vec3 position, Vec3 impulse = {});
    void update(float deltaSeconds, const world::FrontierWorld& world, Vec3 playerPosition,
                inventory::Inventory& inventory);
    void clear() noexcept { drops_.clear(); }

    [[nodiscard]] const std::vector<WorldDrop>& drops() const noexcept { return drops_; }

private:
    std::vector<WorldDrop> drops_;
    std::uint64_t nextId_{1};
};

} // namespace rf::game::drops
