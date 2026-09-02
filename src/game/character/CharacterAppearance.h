#pragma once

#include <cstdint>

namespace rf::game::character {

enum class GearVisual : std::uint8_t {
    None = 0,
    Cloth,
    Leather,
    Iron,
};

// Rendering-facing equipment state. Gameplay/inventory owns which items are equipped; this
// compact appearance payload lets the character renderer layer gear over the permanent body rig
// without baking clothing into the base character mesh.
struct CharacterAppearance {
    GearVisual head{GearVisual::None};
    GearVisual chest{GearVisual::None};
    GearVisual hands{GearVisual::None};
    GearVisual legs{GearVisual::None};
    GearVisual feet{GearVisual::None};
    GearVisual back{GearVisual::None};
    bool showLoincloth{true};
};

} // namespace rf::game::character
