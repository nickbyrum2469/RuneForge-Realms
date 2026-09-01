#include "render/materials/MaterialRegistry.h"

#include <algorithm>

namespace rf::render::materials {

const std::array<MaterialDefinition, 5> MaterialRegistry::definitions_{
    MaterialDefinition{world::SurfaceMaterial::Grass, "grass", 0.92f, 0.0f, 0.0f, false},
    MaterialDefinition{world::SurfaceMaterial::Dirt, "dirt", 0.98f, 0.0f, 0.0f, false},
    MaterialDefinition{world::SurfaceMaterial::Stone, "stone", 0.86f, 0.0f, 0.0f, false},
    MaterialDefinition{world::SurfaceMaterial::Wood, "wood", 0.90f, 0.0f, 0.0f, false},
    MaterialDefinition{world::SurfaceMaterial::Leaves, "leaves", 0.82f, 0.0f, 0.0f, true},
};

const MaterialDefinition& MaterialRegistry::get(world::SurfaceMaterial id) noexcept {
    const auto index = std::min<std::size_t>(static_cast<std::size_t>(id), definitions_.size() - 1);
    return definitions_[index];
}

} // namespace rf::render::materials
