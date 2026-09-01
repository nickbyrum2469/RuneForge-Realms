#include "core/HubModel.h"

#include <algorithm>

namespace rf {

void HubModel::selectNav(std::size_t index) noexcept {
    selectedNav_ = std::min<std::size_t>(index, 3);
}

} // namespace rf
