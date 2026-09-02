#pragma once

#include "game/Math.h"
#include "world/Block.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace rf::world { class FrontierWorld; }

namespace rf::game::particles {

struct BlockParticle {
    Vec3 position{};
    Vec3 velocity{};
    world::BlockId block{world::BlockId::Stone};
    float age{};
    float lifetime{0.8f};
    float size{0.055f};
};

class ParticleSystem {
public:
    static constexpr std::size_t maxParticles = 384;

    void emitBlockBurst(world::BlockId block, Vec3 origin, std::size_t count, float energy = 1.0f) noexcept;
    void update(float deltaSeconds, const world::FrontierWorld& world) noexcept;
    void clear() noexcept { particles_.clear(); }

    [[nodiscard]] const std::vector<BlockParticle>& particles() const noexcept { return particles_; }
    [[nodiscard]] std::size_t size() const noexcept { return particles_.size(); }

private:
    [[nodiscard]] static std::uint32_t mix(std::uint32_t value) noexcept;
    [[nodiscard]] static float unit(std::uint32_t value) noexcept;

    std::vector<BlockParticle> particles_;
    std::uint32_t sequence_{1};
};

} // namespace rf::game::particles
