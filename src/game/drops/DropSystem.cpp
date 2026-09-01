#include "game/drops/DropSystem.h"

#include "world/FrontierWorld.h"

#include <algorithm>
#include <cmath>

namespace rf::game::drops {

void DropSystem::spawn(items::ItemId item, std::uint16_t count, Vec3 position, Vec3 impulse) {
    if (item == items::ItemId::None || count == 0) return;
    drops_.push_back(WorldDrop{nextId_++, item, count, position, impulse, 0.0f});
}

void DropSystem::restore(const std::vector<WorldDrop>& drops) {
    drops_.clear();
    nextId_ = 1;
    for (const auto& drop : drops) {
        if (drop.item == items::ItemId::None || drop.count == 0) continue;
        WorldDrop restored = drop;
        restored.id = restored.id == 0 ? nextId_ : restored.id;
        restored.age = std::max(0.0f, restored.age);
        nextId_ = std::max(nextId_, restored.id + 1);
        drops_.push_back(restored);
    }
}

void DropSystem::update(float deltaSeconds, const world::FrontierWorld& world, Vec3 playerPosition,
                        inventory::Inventory& inventory) {
    const float dt = std::clamp(deltaSeconds, 0.0f, 0.05f);
    constexpr float gravity = -18.0f;
    constexpr float pickupRadiusSquared = 1.65f * 1.65f;

    for (auto& drop : drops_) {
        drop.age += dt;
        drop.velocity.y += gravity * dt;
        drop.position.x += drop.velocity.x * dt;
        drop.position.y += drop.velocity.y * dt;
        drop.position.z += drop.velocity.z * dt;

        const int groundY = world.topSolidY(static_cast<int>(std::floor(drop.position.x)),
                                            static_cast<int>(std::floor(drop.position.z)));
        const float floorHeight = static_cast<float>(groundY + 1) + 0.12f;
        if (groundY >= 0 && drop.position.y < floorHeight) {
            drop.position.y = floorHeight;
            if (drop.velocity.y < -1.0f) drop.velocity.y *= -0.24f;
            else drop.velocity.y = 0.0f;
            drop.velocity.x *= 0.82f;
            drop.velocity.z *= 0.82f;
        }
    }

    drops_.erase(std::remove_if(drops_.begin(), drops_.end(), [&](WorldDrop& drop) {
        if (drop.age < 0.18f) return false;
        const float dx = drop.position.x - playerPosition.x;
        const float dy = drop.position.y - (playerPosition.y + 0.9f);
        const float dz = drop.position.z - playerPosition.z;
        if (dx * dx + dy * dy + dz * dz > pickupRadiusSquared) return drop.age > 300.0f;
        const std::uint16_t remaining = inventory.add(drop.item, drop.count);
        drop.count = remaining;
        return remaining == 0 || drop.age > 300.0f;
    }), drops_.end());
}

} // namespace rf::game::drops
