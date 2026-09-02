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
            case world::BlockId::Water:
            case world::BlockId::Air: return 0;
        }
    }
    return block == world::BlockId::Water || block == world::BlockId::Air ? 0 : 1;
}

float MiningSystem::toolEfficiency(const world::blocks::BlockDefinition& definition,
                                   const MiningToolContext& tool) noexcept {
    float efficiency = std::max(tool.powerMultiplier, 0.05f);
    const bool preferred = definition.preferredTool == world::blocks::ToolClass::Hand ||
                           definition.preferredTool == tool.tool;
    if (!preferred) efficiency *= definition.wrongToolEfficiency;
    if (tool.tier < definition.minimumToolTier) {
        const int deficit = static_cast<int>(definition.minimumToolTier) - static_cast<int>(tool.tier);
        for (int i = 0; i < deficit; ++i) efficiency *= 0.45f;
    }
    return std::clamp(efficiency, 0.03f, 5.0f);
}

float MiningSystem::strikeInterval(world::BlockId block, const MiningToolContext& tool) noexcept {
    const auto& definition = world::blocks::BlockRegistry::get(block);
    const float speed = std::clamp(tool.speedMultiplier, 0.15f, 6.0f);
    // Wrong or under-tier tools are not only weaker; they also recover more slowly against hard material.
    const float efficiency = toolEfficiency(definition, tool);
    const float recoveryPenalty = efficiency < 0.75f ? (1.0f + (0.75f - efficiency) * 0.60f) : 1.0f;
    return std::clamp(definition.baseStrikeIntervalSeconds * recoveryPenalty / speed, 0.10f, 3.0f);
}

MiningOutcome MiningSystem::strike(world::FrontierWorld& world, const world::RaycastHit& hit,
                                   const MiningToolContext& tool) {
    MiningOutcome result;
    if (!hit.hit) return result;

    const world::BlockId block = world.getBlock(hit.block.x, hit.block.y, hit.block.z);
    if (block == world::BlockId::Air || world::isFluid(block)) return result;
    result.block = block;

    const auto& definition = world::blocks::BlockRegistry::get(block);
    const float efficiency = toolEfficiency(definition, tool);

    if (mode_ == MiningMode::Micro) {
        // Precision carving is still tool-limited: a poor tool removes a smaller physical volume.
        int radius = microChipRadius(block, mode_);
        if (efficiency < 0.22f) radius = 0;
        if (radius <= 0) return result;
        const auto chip = world.chipBlock(hit.block, hit.worldX, hit.worldY, hit.worldZ, radius);
        result.affected = chip.changed;
        result.microCellsRemoved = chip.removedCells;
        result.brokeBlock = chip.emptied;
        result.damageProgress = chip.emptied ? 1.0f : (1.0f - chip.solidFraction);
        if (chip.emptied) damage_.erase(hit.block);
        return result;
    }

    float& damage = damage_[hit.block];
    const float increment = definition.damagePerPreferredStrike * efficiency;
    damage += std::clamp(increment, 0.01f, 0.65f);

    if (mode_ == MiningMode::Mixed) {
        // Mixed mode gives tactile chips, but the physical chip volume is intentionally small so the
        // recognizable block survives until structural mining health gives way.
        const int radius = efficiency >= 0.18f ? microChipRadius(block, mode_) : 0;
        if (radius > 0) {
            const auto chip = world.chipBlock(hit.block, hit.worldX, hit.worldY, hit.worldZ, radius);
            result.microCellsRemoved = chip.removedCells;
            result.affected = chip.changed;
            if (chip.changed) damage += static_cast<float>(chip.removedCells) / 512.0f * 0.28f;
            if (chip.emptied) {
                result.brokeBlock = true;
                result.damageProgress = 1.0f;
                damage_.erase(hit.block);
                return result;
            }
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
