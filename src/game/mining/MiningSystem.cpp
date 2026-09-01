#include "game/mining/MiningSystem.h"

#include "world/FrontierWorld.h"
#include "world/blocks/BlockRegistry.h"

#include <algorithm>

namespace rf::game::mining {

float MiningSystem::damageAt(world::BlockCoord position) const noexcept {
    const auto it = damage_.find(position);
    return it == damage_.end() ? 0.0f : it->second;
}

std::vector<MiningDamageState> MiningSystem::damageStates() const {
    std::vector<MiningDamageState> result;
    result.reserve(damage_.size());
    for (const auto& [position, progress] : damage_) {
        if (progress <= 0.0f || progress >= 1.0f) continue;
        result.push_back({position, progress});
    }
    return result;
}

void MiningSystem::restoreDamage(const std::vector<MiningDamageState>& states) {
    damage_.clear();
    for (const auto& state : states) {
        const float progress = std::clamp(state.progress, 0.0f, 0.9999f);
        if (progress <= 0.0f) continue;
        damage_[state.position] = progress;
    }
}

int MiningSystem::microChipRadius(world::BlockId block, MiningMode mode) noexcept {
    if (mode == MiningMode::Block) return 0;
    if (mode == MiningMode::Micro) {
        switch (block) {
            case world::BlockId::Leaves: return 2;
            case world::BlockId::Grass:
            case world::BlockId::Dirt:
            case world::BlockId::Stone:
            case world::BlockId::Wood: return 1;
            case world::BlockId::Air: return 0;
        }
    }
    // Mixed mode chips just enough to show impact while preserving the recognizable block
    // until its ordinary mining-health threshold is reached.
    return 1;
}

MiningOutcome MiningSystem::strike(world::FrontierWorld& world, const world::RaycastHit& hit,
                                   float toolPower) {
    MiningOutcome result;
    if (!hit.hit) return result;

    const world::BlockId block = world.getBlock(hit.block.x, hit.block.y, hit.block.z);
    if (block == world::BlockId::Air) return result;
    result.block = block;

    const auto& definition = world::blocks::BlockRegistry::get(block);
    const float hardness = std::max(definition.hardness, 0.08f);
    const float power = std::max(toolPower, 0.05f);

    if (mode_ == MiningMode::Micro) {
        const auto chip = world.chipBlock(hit.block, hit.worldX, hit.worldY, hit.worldZ,
                                         microChipRadius(block, mode_));
        result.affected = chip.changed;
        result.microCellsRemoved = chip.removedCells;
        result.brokeBlock = chip.emptied;
        result.damageProgress = chip.emptied ? 1.0f : (1.0f - chip.solidFraction);
        if (chip.emptied) damage_.erase(hit.block);
        return result;
    }

    // A hand strike is intentionally several hits even on dirt/grass. Tool power later plugs
    // into this same equation without changing mining behavior or save representation.
    float& damage = damage_[hit.block];
    damage += std::clamp((0.34f * power) / (hardness + 0.34f), 0.06f, 0.72f);

    if (mode_ == MiningMode::Mixed) {
        const auto chip = world.chipBlock(hit.block, hit.worldX, hit.worldY, hit.worldZ,
                                         microChipRadius(block, mode_));
        result.microCellsRemoved = chip.removedCells;
        result.affected = chip.changed;
        // Losing a meaningful percentage of real matter contributes modestly to structural damage.
        if (chip.changed) damage += static_cast<float>(chip.removedCells) / 512.0f * 0.42f;
        if (chip.emptied) {
            result.brokeBlock = true;
            result.damageProgress = 1.0f;
            damage_.erase(hit.block);
            return result;
        }
    }

    result.damageProgress = std::clamp(damage, 0.0f, 1.0f);
    if (damage < 1.0f) {
        result.affected = true;
        return result;
    }

    result.affected = world.setBlock(hit.block.x, hit.block.y, hit.block.z, world::BlockId::Air) || result.affected;
    result.brokeBlock = true;
    result.damageProgress = 1.0f;
    damage_.erase(hit.block);
    return result;
}

} // namespace rf::game::mining
