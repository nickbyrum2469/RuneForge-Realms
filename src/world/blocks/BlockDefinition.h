#pragma once

#include "world/Block.h"

#include <cstdint>
#include <string_view>

namespace rf::world::blocks {

enum class ToolClass : std::uint8_t {
    Hand,
    Pickaxe,
    Axe,
};

struct BlockDefinition {
    BlockId id{BlockId::Air};
    std::string_view name{"Air"};
    bool solid{false};
    bool transparent{true};
    float hardness{0.0f};
    ToolClass preferredTool{ToolClass::Hand};
    std::uint16_t maxStack{64};
    SurfaceMaterial topMaterial{SurfaceMaterial::Dirt};
    SurfaceMaterial sideMaterial{SurfaceMaterial::Dirt};
    SurfaceMaterial bottomMaterial{SurfaceMaterial::Dirt};
};

} // namespace rf::world::blocks
