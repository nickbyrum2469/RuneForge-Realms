#pragma once

#include "game/particles/ParticleSystem.h"
#include "world/GreedyMesher.h"

namespace rf::render::scene {

class ParticleMeshBuilder {
public:
    [[nodiscard]] static world::VoxelMesh build(const std::vector<game::particles::BlockParticle>& particles);
};

} // namespace rf::render::scene
