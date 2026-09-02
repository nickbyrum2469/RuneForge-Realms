#pragma once

#include <cstdint>
#include <string_view>

namespace rf::world {

enum class BlockId : std::uint8_t {
    Air = 0,
    Grass,
    Dirt,
    Stone,
    Wood,
    Leaves,
    Water,
};

// Surface materials are deliberately more granular than block IDs. A single block may
// expose different materials on different faces (grass turf vs rooted soil, bark vs end grain).
enum class SurfaceMaterial : std::uint32_t {
    GrassTop = 0,
    GrassSide,
    Dirt,
    Stone,
    WoodBark,
    WoodCut,
    Leaves,
    FlowerWhite,
    FlowerYellow,
    FlowerBlue,
    Water,
    CharacterSkin,
    CharacterBlueCloth,
    CharacterLeather,
    CharacterMetal,
    CharacterHair,
    CharacterEyeWhite,
    CharacterEyeBlue,
    CharacterLoincloth,
};

[[nodiscard]] constexpr bool isRenderable(BlockId block) noexcept {
    return block != BlockId::Air;
}

[[nodiscard]] constexpr bool isCollidable(BlockId block) noexcept {
    return block != BlockId::Air && block != BlockId::Water;
}

// Legacy gameplay code uses "solid" to mean physical collision. Rendering must use the more
// explicit isRenderable/isOpaque helpers so fluids can be visible without becoming walls.
[[nodiscard]] constexpr bool isSolid(BlockId block) noexcept {
    return isCollidable(block);
}

[[nodiscard]] constexpr bool isOpaque(BlockId block) noexcept {
    return block != BlockId::Air && block != BlockId::Water && block != BlockId::Leaves;
}

[[nodiscard]] constexpr bool isFluid(BlockId block) noexcept {
    return block == BlockId::Water;
}

[[nodiscard]] constexpr std::string_view blockName(BlockId block) noexcept {
    switch (block) {
        case BlockId::Air: return "Air";
        case BlockId::Grass: return "Grass";
        case BlockId::Dirt: return "Dirt";
        case BlockId::Stone: return "Stone";
        case BlockId::Wood: return "Wood";
        case BlockId::Leaves: return "Leaves";
        case BlockId::Water: return "Water";
    }
    return "Unknown";
}

[[nodiscard]] inline SurfaceMaterial surfaceMaterial(BlockId block, int axis, int normalSign) noexcept {
    switch (block) {
        case BlockId::Grass:
            if (axis == 1 && normalSign > 0) return SurfaceMaterial::GrassTop;
            return SurfaceMaterial::GrassSide;
        case BlockId::Dirt: return SurfaceMaterial::Dirt;
        case BlockId::Stone: return SurfaceMaterial::Stone;
        case BlockId::Wood: return axis == 1 ? SurfaceMaterial::WoodCut : SurfaceMaterial::WoodBark;
        case BlockId::Leaves: return SurfaceMaterial::Leaves;
        case BlockId::Water: return SurfaceMaterial::Water;
        case BlockId::Air: break;
    }
    return SurfaceMaterial::Dirt;
}

} // namespace rf::world
