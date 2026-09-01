#pragma once

#include "world/Block.h"

#include <array>
#include <cstdint>
#include <string_view>

namespace rf::world::blocks {

struct BlockDefinition {
    BlockId id{BlockId::Air};
    std::string_view key;
    std::string_view displayName;
    bool solid{false};
    bool transparent{true};
    float hardness{};
    std::uint16_t maxStack{64};
    SurfaceMaterial topMaterial{SurfaceMaterial::Dirt};
    SurfaceMaterial sideMaterial{SurfaceMaterial::Dirt};
    SurfaceMaterial bottomMaterial{SurfaceMaterial::Dirt};
};

class BlockRegistry {
public:
    [[nodiscard]] static const BlockDefinition& get(BlockId id) noexcept;
    [[nodiscard]] static constexpr std::size_t count() noexcept { return definitions_.size(); }

private:
    static const std::array<BlockDefinition, 6> definitions_;
};

} // namespace rf::world::blocks
