#pragma once

#include "game/mining/MiningMode.h"
#include "world/Block.h"
#include "world/WorldEdit.h"
#include "world/blocks/BlockDefinition.h"

#include <cstdint>
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

struct MiningToolContext {
    world::blocks::ToolClass tool{world::blocks::ToolClass::Hand};
    std::uint8_t tier{0};
    float powerMultiplier{1.0f};
    float speedMultiplier{1.0f};
};

class MiningSystem {
public:
    void setMode(MiningMode mode) noexcept { mode_ = mode; }
    void cycleMode() noexcept { mode_ = nextMiningMode(mode_); }
    [[nodiscard]] MiningMode mode() const noexcept { return mode_; }

    [[nodiscard]] MiningOutcome strike(world::FrontierWorld& world, const world::RaycastHit& hit,
                                       const MiningToolContext& tool = {});
    [[nodiscard]] static float strikeInterval(world::BlockId block, const MiningToolContext& tool = {}) noexcept;
    void clearDamage(world::BlockCoord position) noexcept { damage_.erase(position); }
    void clearAllDamage() noexcept { damage_.clear(); }
    [[nodiscard]] float damageAt(world::BlockCoord position) const noexcept;
    [[nodiscard]] std::vector<MiningDamageState> damageStates() const;
    void restoreDamage(const std::vector<MiningDamageState>& states);

private:
    [[nodiscard]] static int microChipRadius(world::BlockId block, MiningMode mode) noexcept;
    [[nodiscard]] static float toolEfficiency(const world::blocks::BlockDefinition& definition,
                                              const MiningToolContext& tool) noexcept;

    MiningMode mode_{MiningMode::Mixed};
    std::map<world::BlockCoord, float> damage_;
};

} // namespace rf::game::mining
