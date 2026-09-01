#include "render/materials/MaterialRegistry.h"

namespace rf::render::materials {
namespace {

constexpr std::array<MaterialDefinition, MaterialRegistry::materialCount> kMaterials{{
    {world::SurfaceMaterial::GrassTop, "lush_turf", 0.91f, 0.0f, 0.0f, 0.72f, 14.0f, 0.42f, "grass_tuft_dense"},
    {world::SurfaceMaterial::GrassSide, "rooted_turf", 0.95f, 0.0f, 0.0f, 0.82f, 11.0f, 0.34f, "turf_roots_overhang"},
    {world::SurfaceMaterial::Dirt, "packed_soil", 0.98f, 0.0f, 0.0f, 0.86f, 12.0f, 0.30f, "soil_roots_pebbles"},
    {world::SurfaceMaterial::Stone, "fractured_stone", 0.84f, 0.0f, 0.0f, 0.58f, 9.0f, 0.56f, "fractured_plate_rock"},
    {world::SurfaceMaterial::WoodBark, "oak_bark", 0.90f, 0.0f, 0.0f, 0.76f, 16.0f, 0.45f, "bark_chunks"},
    {world::SurfaceMaterial::WoodCut, "oak_endgrain", 0.86f, 0.0f, 0.0f, 0.66f, 13.0f, 0.26f, "growth_rings"},
    {world::SurfaceMaterial::Leaves, "oak_leaf_cluster", 0.83f, 0.0f, 0.0f, 0.62f, 18.0f, 0.38f, "leaf_clusters"},
}};

} // namespace

const MaterialDefinition& MaterialRegistry::get(world::SurfaceMaterial id) noexcept {
    const auto index = static_cast<std::size_t>(id);
    return index < kMaterials.size() ? kMaterials[index] : kMaterials[2];
}

const std::array<MaterialDefinition, MaterialRegistry::materialCount>& MaterialRegistry::all() noexcept {
    return kMaterials;
}

} // namespace rf::render::materials
