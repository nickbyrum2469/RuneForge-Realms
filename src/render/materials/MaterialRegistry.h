#pragma once

#include "world/Block.h"

#include <array>
#include <string_view>

namespace rf::render::materials {

struct MaterialDefinition {
    world::SurfaceMaterial id{world::SurfaceMaterial::Dirt};
    std::string_view key;
    float roughness{1.0f};
    float metallic{0.0f};
    float emissiveStrength{0.0f};
    bool translucent{false};
};

class MaterialRegistry {
public:
    [[nodiscard]] static const MaterialDefinition& get(world::SurfaceMaterial id) noexcept;
    [[nodiscard]] static constexpr std::size_t count() noexcept { return definitions_.size(); }

private:
    static const std::array<MaterialDefinition, 5> definitions_;
};

} // namespace rf::render::materials
