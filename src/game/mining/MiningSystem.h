#pragma once

#include "game/mining/MiningMode.h"
#include "world/Block.h"
#include "world/WorldEdit.h"

#include <map>
#include <vector>

namespace rf::world { class FrontierWorld; }

namespace rf::game::mining {

struct MiningOutcome {
    bool affected{false};
    bool brokeBlock{false};
    world::BlockId block{world::BlockId::Air};
    std::size_t microCellsRemoved{};
    float damageProgress{};
};

struct MiningDamageState {
    world::BlockCoord position{};
    float progress{};
};

class MiningSystem {
public:
    void setMode(MiningMode mode) noexcept { mode_ = mode; }
    void cycleMode() noexcept { mode_ = nextMiningMode(mode_); }
    [[nodiscard]] MiningMode mode() const noexcept { return mode_; }

    [[nodiscard]] MiningOutcome strike(world::FrontierWorld& world, const world::RaycastHit& hit,
                                       float toolPower = 1.0f);
    void clearDamage(world::BlockCoord position) noexcept { damage_.erase(position); }
    void clearAllDamage() noexcept { damage_.clear(); }
    [[nodiscard]] float damageAt(world::BlockCoord position) const noexcept;
    [[nodiscard]] std::vector<MiningDamageState> damageStates() const;
    void restoreDamage(const std::vector<MiningDamageState>& states);

private:
    [[nodiscard]] static int microChipRadius(world::BlockId block, MiningMode mode) noexcept;

    MiningMode mode_{MiningMode::Mixed};
    std::map<world::BlockCoord, float> damage_;
};

} // namespace rf::game::mining
