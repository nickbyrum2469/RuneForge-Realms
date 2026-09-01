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
};

[[nodiscard]] constexpr bool isSolid(BlockId block) noexcept {
    return block != BlockId::Air;
}

[[nodiscard]] constexpr std::string_view blockName(BlockId block) noexcept {
    switch (block) {
        case BlockId::Air: return "Air";
        case BlockId::Grass: return "Grass";
        case BlockId::Dirt: return "Dirt";
        case BlockId::Stone: return "Stone";
        case BlockId::Wood: return "Wood";
        case BlockId::Leaves: return "Leaves";
    }
    return "Unknown";
}

[[nodiscard]] inline SurfaceMaterial surfaceMaterial(BlockId block, int axis, int normalSign) noexcept {
    switch (block) {
        case BlockId::Grass:
            if (axis == 1 && normalSign > 0) return SurfaceMaterial::GrassTop;
            return SurfaceMaterial::GrassSide;
        case BlockId::Dirt:
            return SurfaceMaterial::Dirt;
        case BlockId::Stone:
            return SurfaceMaterial::Stone;
        case BlockId::Wood:
            return axis == 1 ? SurfaceMaterial::WoodCut : SurfaceMaterial::WoodBark;
        case BlockId::Leaves:
            return SurfaceMaterial::Leaves;
        case BlockId::Air:
            break;
    }
    return SurfaceMaterial::Dirt;
}

} // namespace rf::world
