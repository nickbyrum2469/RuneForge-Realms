#include "world/blocks/BlockRegistry.h"

#include <algorithm>

namespace rf::world::blocks {

const std::array<BlockDefinition, 6> BlockRegistry::definitions_{
    BlockDefinition{BlockId::Air, "air", "Air", false, true, 0.0f, 0,
                    SurfaceMaterial::Dirt, SurfaceMaterial::Dirt, SurfaceMaterial::Dirt},
    BlockDefinition{BlockId::Grass, "grass", "Grass", true, false, 0.55f, 64,
                    SurfaceMaterial::Grass, SurfaceMaterial::Dirt, SurfaceMaterial::Dirt},
    BlockDefinition{BlockId::Dirt, "dirt", "Dirt", true, false, 0.45f, 64,
                    SurfaceMaterial::Dirt, SurfaceMaterial::Dirt, SurfaceMaterial::Dirt},
    BlockDefinition{BlockId::Stone, "stone", "Stone", true, false, 1.55f, 64,
                    SurfaceMaterial::Stone, SurfaceMaterial::Stone, SurfaceMaterial::Stone},
    BlockDefinition{BlockId::Wood, "wood", "Wood", true, false, 1.0f, 64,
                    SurfaceMaterial::Wood, SurfaceMaterial::Wood, SurfaceMaterial::Wood},
    BlockDefinition{BlockId::Leaves, "leaves", "Leaves", true, true, 0.18f, 64,
                    SurfaceMaterial::Leaves, SurfaceMaterial::Leaves, SurfaceMaterial::Leaves},
};

const BlockDefinition& BlockRegistry::get(BlockId id) noexcept {
    const auto index = std::min<std::size_t>(static_cast<std::size_t>(id), definitions_.size() - 1);
    return definitions_[index];
}

} // namespace rf::world::blocks
