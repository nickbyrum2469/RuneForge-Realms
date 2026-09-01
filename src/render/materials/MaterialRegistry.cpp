#include "render/materials/MaterialRegistry.h"

namespace rf::render::materials {
namespace {

constexpr std::array<MaterialDefinition, MaterialRegistry::materialCount> kMaterials{{
    {world::SurfaceMaterial::GrassTop, "lush_turf", 0.91f, 0.0f, 0.0f, 0.72f, 14.0f, 0.42f, "grass_tuft_sparse"},
    {world::SurfaceMaterial::GrassSide, "rooted_turf", 0.95f, 0.0f, 0.0f, 0.82f, 11.0f, 0.34f, "thin_turf_roots"},
    {world::SurfaceMaterial::Dirt, "rich_soil", 0.96f, 0.0f, 0.0f, 0.86f, 12.0f, 0.32f, "soil_clumps_pebbles"},
    {world::SurfaceMaterial::Stone, "fractured_stone", 0.86f, 0.0f, 0.0f, 0.58f, 9.0f, 0.52f, "natural_plate_rock"},
    {world::SurfaceMaterial::WoodBark, "oak_bark", 0.93f, 0.0f, 0.0f, 0.76f, 16.0f, 0.42f, "vertical_bark_ridges"},
    {world::SurfaceMaterial::WoodCut, "oak_endgrain", 0.87f, 0.0f, 0.0f, 0.66f, 13.0f, 0.24f, "growth_rings"},
    {world::SurfaceMaterial::Leaves, "oak_leaf_cluster", 0.82f, 0.0f, 0.0f, 0.62f, 18.0f, 0.32f, "alpha_cut_leaf_clusters"},
    {world::SurfaceMaterial::FlowerWhite, "flower_white", 0.72f, 0.0f, 0.02f, 1.0f, 20.0f, 0.16f, "flower_petals"},
    {world::SurfaceMaterial::FlowerYellow, "flower_yellow", 0.70f, 0.0f, 0.03f, 1.0f, 20.0f, 0.16f, "flower_petals"},
    {world::SurfaceMaterial::FlowerBlue, "flower_blue", 0.68f, 0.0f, 0.03f, 1.0f, 20.0f, 0.16f, "flower_petals"},
    {world::SurfaceMaterial::Water, "frontier_water", 0.16f, 0.0f, 0.02f, 0.32f, 22.0f, 0.16f, "clear_stylized_water"},
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
