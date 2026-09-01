#include "world/micro/MicroVoxelState.h"

#include <algorithm>

namespace rf::world::micro {

MicroVoxelState::MicroVoxelState() { occupied_.set(); }

bool MicroVoxelState::occupied(int x, int y, int z) const noexcept {
    if (x < 0 || y < 0 || z < 0 || x >= resolution || y >= resolution || z >= resolution) return false;
    return occupied_.test(index(x, y, z));
}

bool MicroVoxelState::setOccupied(int x, int y, int z, bool value) noexcept {
    if (x < 0 || y < 0 || z < 0 || x >= resolution || y >= resolution || z >= resolution) return false;
    const std::size_t bit = index(x, y, z);
    const bool previous = occupied_.test(bit);
    occupied_.set(bit, value);
    return previous != value;
}

std::size_t MicroVoxelState::clearSphere(MicroCoord center, int radiusCells) noexcept {
    const int radius = std::clamp(radiusCells, 0, resolution);
    const int r2 = radius * radius;
    std::size_t removed = 0;
    for (int y = std::max(0, static_cast<int>(center.y) - radius);
         y <= std::min(resolution - 1, static_cast<int>(center.y) + radius); ++y) {
        for (int z = std::max(0, static_cast<int>(center.z) - radius);
             z <= std::min(resolution - 1, static_cast<int>(center.z) + radius); ++z) {
            for (int x = std::max(0, static_cast<int>(center.x) - radius);
                 x <= std::min(resolution - 1, static_cast<int>(center.x) + radius); ++x) {
                const int dx = x - static_cast<int>(center.x);
                const int dy = y - static_cast<int>(center.y);
                const int dz = z - static_cast<int>(center.z);
                if (dx * dx + dy * dy + dz * dz > r2) continue;
                const std::size_t bit = index(x, y, z);
                if (!occupied_.test(bit)) continue;
                occupied_.reset(bit);
                ++removed;
            }
        }
    }
    return removed;
}

} // namespace rf::world::micro
