#pragma once

#include "game/Math.h"
#include "world/Block.h"
#include "world/GreedyMesher.h"

#include <optional>

namespace rf::render::scene {

struct FirstPersonViewModelState {
    game::Vec3 eye{};
    game::Vec3 forward{0.0f, 0.0f, 1.0f};
    game::Vec3 right{1.0f, 0.0f, 0.0f};
    game::Vec3 up{0.0f, 1.0f, 0.0f};
    float walkPhase{};
    float walkAmount{};
    bool swingActive{};
    float swingTime{};
    float targetDistance{1.0f};
    std::optional<world::BlockId> equippedBlock{};
};

class FirstPersonBodyBuilder {
public:
    // First-person geometry is a camera-space viewmodel, not the world-space third-person skeleton.
    // This guarantees that the visible hands rotate with the camera instead of appearing to counter-
    // rotate against it. Unarmed idle shows both hands peeking into frame; an equipped item shows only
    // the dominant right hand plus the held item.
    [[nodiscard]] static world::VoxelMesh build(const FirstPersonViewModelState& state);
};

} // namespace rf::render::scene
