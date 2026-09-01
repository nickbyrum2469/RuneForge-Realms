#pragma once

#include "game/mining/MiningMode.h"
#include "world/Block.h"
#include "world/WorldEdit.h"

#include <map>

namespace rf::world { class FrontierWorld; }

namespace rf::game::mining {

struct MiningOutcome {
    bool affected{false};
    bool brokeBlock{false};
    world::BlockId block{world::BlockId::Air};
    std::size_t microCellsRemoved{};
    float damageProgress{};
};

class MiningSystem {
public:
    void setMode(MiningMode mode) noexcept { mode_ = mode; }
    void cycleMode() noexcept { mode_ = nextMiningMode(mode_); }
    [[nodiscard]] MiningMode mode() const noexcept { return mode_; }

    [[nodiscard]] MiningOutcome strike(world::FrontierWorld& world, const world::RaycastHit& hit,
                                       float toolPower = 1.0f);
    void clearDamage(world::BlockCoord position) noexcept { damage_.erase(position); }
    [[nodiscard]] float damageAt(world::BlockCoord position) const noexcept;

private:
    [[nodiscard]] static int microChipRadius(world::BlockId block, MiningMode mode) noexcept;

    MiningMode mode_{MiningMode::Mixed};
    std::map<world::BlockCoord, float> damage_;
};

} // namespace rf::game::mining
