#pragma once

#include "render/materials/MaterialDefinition.h"

#include <array>
#include <cstddef>

namespace rf::render::materials {

class MaterialRegistry {
public:
    static constexpr std::size_t materialCount = 7;

    [[nodiscard]] static const MaterialDefinition& get(world::SurfaceMaterial id) noexcept;
    [[nodiscard]] static const std::array<MaterialDefinition, materialCount>& all() noexcept;
};

} // namespace rf::render::materials
