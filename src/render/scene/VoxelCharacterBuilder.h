#pragma once

#include "game/character/CharacterAppearance.h"
#include "game/character/PlayerBodyRig.h"
#include "world/GreedyMesher.h"

namespace rf::render::scene {

struct CharacterBuildOptions {
    bool includeHead{true};
};

class VoxelCharacterBuilder {
public:
    [[nodiscard]] static world::VoxelMesh build(const game::character::PlayerBodyPose& pose,
                                                const game::character::CharacterAppearance& appearance = {},
                                                CharacterBuildOptions options = {});
};

} // namespace rf::render::scene
