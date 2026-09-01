#include "world/blocks/BlockRegistry.h"

namespace rf::world::blocks {
namespace {

constexpr std::array<BlockDefinition, BlockRegistry::blockCount> kBlocks{{
    {BlockId::Air, "Air", false, true, 0.0f, ToolClass::Hand, 0,
     SurfaceMaterial::Dirt, SurfaceMaterial::Dirt, SurfaceMaterial::Dirt},
    {BlockId::Grass, "Grass", true, false, 0.65f, ToolClass::Hand, 64,
     SurfaceMaterial::GrassTop, SurfaceMaterial::GrassSide, SurfaceMaterial::Dirt},
    {BlockId::Dirt, "Dirt", true, false, 0.55f, ToolClass::Hand, 64,
     SurfaceMaterial::Dirt, SurfaceMaterial::Dirt, SurfaceMaterial::Dirt},
    {BlockId::Stone, "Stone", true, false, 1.8f, ToolClass::Pickaxe, 64,
     SurfaceMaterial::Stone, SurfaceMaterial::Stone, SurfaceMaterial::Stone},
    {BlockId::Wood, "Wood", true, false, 1.25f, ToolClass::Axe, 64,
     SurfaceMaterial::WoodCut, SurfaceMaterial::WoodBark, SurfaceMaterial::WoodCut},
    {BlockId::Leaves, "Leaves", true, true, 0.25f, ToolClass::Hand, 64,
     SurfaceMaterial::Leaves, SurfaceMaterial::Leaves, SurfaceMaterial::Leaves},
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
