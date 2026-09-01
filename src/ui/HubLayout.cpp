#include "ui/HubLayout.h"

#include <cstddef>

namespace rf::ui {

HubLayout::HubLayout() {
    for (std::size_t i = 0; i < nav.size(); ++i) {
        nav[i] = Rect{15.0f, 55.0f + static_cast<float>(i) * 82.0f, 140.0f, 66.0f};
    }
    for (std::size_t i = 0; i < cards.size(); ++i) {
        cards[i] = Rect{195.0f + static_cast<float>(i) * 226.0f, 645.0f, 214.0f, 205.0f};
    }
}

} // namespace rf::ui
