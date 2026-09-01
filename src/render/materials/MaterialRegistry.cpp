#include "render/materials/MaterialRegistry.h"

namespace rf::render::materials {
namespace {

constexpr std::array<MaterialDefinition, MaterialRegistry::materialCount> kMaterials{{
    {world::SurfaceMaterial::Grass, "lush_grass", 0.92f, 0.0f, 0.0f, "grass_tuft_dense"},
    {world::SurfaceMaterial::Dirt, "packed_soil", 0.98f, 0.0f, 0.0f, "soil_roots"},
    {world::SurfaceMaterial::Stone, "weathered_stone", 0.86f, 0.0f, 0.0f, "fractured_rock"},
    {world::SurfaceMaterial::Wood, "oak_bark", 0.90f, 0.0f, 0.0f, "bark_chunks"},
    {world::SurfaceMaterial::Leaves, "oak_leaves", 0.82f, 0.0f, 0.0f, "leaf_clusters"},
}};

} // namespace

const MaterialDefinition& MaterialRegistry::get(world::SurfaceMaterial id) noexcept {
    const auto index = static_cast<std::size_t>(id);
    return index < kMaterials.size() ? kMaterials[index] : kMaterials[1];
}

const std::array<MaterialDefinition, MaterialRegistry::materialCount>& MaterialRegistry::all() noexcept {
    return kMaterials;
}

} // namespace rf::render::materials
