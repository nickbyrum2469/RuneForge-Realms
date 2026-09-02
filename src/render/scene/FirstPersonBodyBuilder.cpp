#include "render/scene/FirstPersonBodyBuilder.h"

#include "render/scene/VoxelCharacterBuilder.h"

namespace rf::render::scene {

world::VoxelMesh FirstPersonBodyBuilder::build(const game::character::PlayerBodyPose& pose,
                                               const game::character::CharacterAppearance& appearance) {
    CharacterBuildOptions options;
    // The canonical head/hair/eyes are part of the same character model, but first-person rendering
    // omits them so the camera never sits inside opaque voxels. Torso, legs, feet and both physical
    // arms remain visible when looking down, preserving an embodied player instead of floating hands.
    options.includeHead = false;
    return VoxelCharacterBuilder::build(pose, appearance, options);
}

} // namespace rf::render::scene
