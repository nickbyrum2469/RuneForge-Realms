#include "game/particles/ParticleSystem.h"

#include "world/FrontierWorld.h"

#include <algorithm>
#include <cmath>

namespace rf::game::particles {

std::uint32_t ParticleSystem::mix(std::uint32_t value) noexcept {
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

float ParticleSystem::unit(std::uint32_t value) noexcept {
    return static_cast<float>(value & 0xffffu) / 65535.0f;
}

void ParticleSystem::emitBlockBurst(world::BlockId block, Vec3 origin,
                                    std::size_t count, float energy) noexcept {
    if (block == world::BlockId::Air || world::isFluid(block) || count == 0) return;
    energy = std::clamp(energy, 0.2f, 2.5f);

    const std::size_t room = particles_.size() < maxParticles ? maxParticles - particles_.size() : 0;
    count = std::min(count, room);
    for (std::size_t i = 0; i < count; ++i) {
        const std::uint32_t h0 = mix(sequence_++ ^ static_cast<std::uint32_t>(block) * 0x9e3779b9u);
        const std::uint32_t h1 = mix(h0 + 0x85ebca6bu);
        const std::uint32_t h2 = mix(h1 + 0xc2b2ae35u);
        const float angle = unit(h0) * 6.28318530718f;
        const float radial = (0.55f + unit(h1) * 1.25f) * energy;
        const float upward = (1.0f + unit(h2) * 2.4f) * energy;

        BlockParticle particle;
        particle.block = block;
        particle.position = {
            origin.x + (unit(h1 >> 8) - 0.5f) * 0.24f,
            origin.y + (unit(h2 >> 7) - 0.5f) * 0.18f,
            origin.z + (unit(h0 >> 9) - 0.5f) * 0.24f,
        };
        particle.velocity = {std::cos(angle) * radial, upward, std::sin(angle) * radial};

        const bool light = block == world::BlockId::Leaves || block == world::BlockId::Grass;
        particle.lifetime = (light ? 0.55f : 0.38f) + unit(h2 >> 12) * (light ? 0.65f : 0.48f);
        particle.size = (light ? 0.035f : 0.045f) + unit(h1 >> 13) * (light ? 0.035f : 0.050f);
        particles_.push_back(particle);
    }
}

void ParticleSystem::update(float deltaSeconds, const world::FrontierWorld& world) noexcept {
    const float dt = std::clamp(deltaSeconds, 0.0f, 0.05f);
    for (auto& particle : particles_) {
        particle.age += dt;
        const bool light = particle.block == world::BlockId::Leaves || particle.block == world::BlockId::Grass;
        const float gravity = light ? 6.2f : 14.5f;
        const float drag = light ? 0.965f : 0.985f;
        particle.velocity.y -= gravity * dt;
        particle.velocity.x *= std::pow(drag, dt * 60.0f);
        particle.velocity.z *= std::pow(drag, dt * 60.0f);

        Vec3 candidate = particle.position + particle.velocity * dt;
        const float half = particle.size * 0.45f;
        if (world.collidesAabb(candidate.x - half, candidate.y - half, candidate.z - half,
                               candidate.x + half, candidate.y + half, candidate.z + half)) {
            particle.velocity.x *= 0.48f;
            particle.velocity.z *= 0.48f;
            if (particle.velocity.y < 0.0f) particle.velocity.y = -particle.velocity.y * 0.22f;
        } else {
            particle.position = candidate;
        }
    }

    std::erase_if(particles_, [](const BlockParticle& particle) {
        return particle.age >= particle.lifetime;
    });
}

} // namespace rf::game::particles
