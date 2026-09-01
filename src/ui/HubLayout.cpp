#include "ui/HubLayout.h"

#include <cstddef>

namespace rf::ui {

HubLayout::HubLayout() {
    for (std::size_t i = 0; i < nav.size(); ++i) {
        nav[i] = Rect{15.0f, 64.0f + static_cast<float>(i) * 86.0f, 140.0f, 68.0f};
    }
    for (std::size_t i = 0; i < featureCards.size(); ++i) {
        featureCards[i] = Rect{195.0f + static_cast<float>(i) * 276.0f, 645.0f, 255.0f, 205.0f};
    }
}

} // namespace rf::ui
