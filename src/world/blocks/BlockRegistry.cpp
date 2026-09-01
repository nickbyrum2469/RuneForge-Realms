#include "world/blocks/BlockRegistry.h"

namespace rf::world::blocks {
namespace {

constexpr std::array<BlockDefinition, BlockRegistry::blockCount> kBlocks{{
    {BlockId::Air, "Air", false, true, 0.0f, ToolClass::Hand, 0, 1.0f, 0.45f, 1.0f,
     SoundFamily::None, 0, SurfaceMaterial::Dirt, SurfaceMaterial::Dirt, SurfaceMaterial::Dirt},
    {BlockId::Grass, "Grass", true, false, 0.70f, ToolClass::Shovel, 0, 0.22f, 0.48f, 0.72f,
     SoundFamily::Grass, 64, SurfaceMaterial::GrassTop, SurfaceMaterial::GrassSide, SurfaceMaterial::Dirt},
    {BlockId::Dirt, "Dirt", true, false, 0.62f, ToolClass::Shovel, 0, 0.24f, 0.50f, 0.75f,
     SoundFamily::Dirt, 64, SurfaceMaterial::Dirt, SurfaceMaterial::Dirt, SurfaceMaterial::Dirt},
    {BlockId::Stone, "Stone", true, false, 2.20f, ToolClass::Pickaxe, 1, 0.14f, 0.82f, 0.30f,
     SoundFamily::Stone, 64, SurfaceMaterial::Stone, SurfaceMaterial::Stone, SurfaceMaterial::Stone},
    {BlockId::Wood, "Wood", true, false, 1.45f, ToolClass::Axe, 0, 0.17f, 0.68f, 0.42f,
     SoundFamily::Wood, 64, SurfaceMaterial::WoodCut, SurfaceMaterial::WoodBark, SurfaceMaterial::WoodCut},
    {BlockId::Leaves, "Leaves", true, true, 0.22f, ToolClass::Hand, 0, 0.48f, 0.38f, 0.90f,
     SoundFamily::Leaves, 64, SurfaceMaterial::Leaves, SurfaceMaterial::Leaves, SurfaceMaterial::Leaves},
    {BlockId::Water, "Water", false, true, 0.0f, ToolClass::Hand, 0, 0.0f, 0.45f, 0.0f,
     SoundFamily::Water, 0, SurfaceMaterial::Water, SurfaceMaterial::Water, SurfaceMaterial::Water},
}};

} // namespace

const BlockDefinition& BlockRegistry::get(BlockId id) noexcept {
    const auto index = static_cast<std::size_t>(id);
    return index < kBlocks.size() ? kBlocks[index] : kBlocks[0];
}

const std::array<BlockDefinition, BlockRegistry::blockCount>& BlockRegistry::all() noexcept {
    return kBlocks;
}

} // namespace rf::world::blocks
