#pragma once

#include "world/Block.h"

#include <string_view>

namespace rf::render::materials {

struct MaterialDefinition {
    world::SurfaceMaterial id{world::SurfaceMaterial::Dirt};
    std::string_view name{"dirt"};
    float roughness{1.0f};
    float metallic{0.0f};
    float emissive{0.0f};
    std::string_view detailProfile{"none"};
};

} // namespace rf::render::materials
