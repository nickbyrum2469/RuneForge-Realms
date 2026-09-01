#include "core/HubModel.h"

#include <algorithm>

namespace rf {

HubModel::HubModel()
    : modes_{
          {"frontier", "Frontier Realms", "Persistent Survival", "SURVIVAL",
           "Gather, craft, explore and build a world that grows from hand-scale survival into settlement and world-scale control."},
          {"echo", "Echo Depths", "Expedition Caves", "ADVENTURE",
           "Descend through dangerous caverns, recover Echo Crystals and bring discoveries back to your persistent realm."},
          {"creative", "Forgekeeper", "Creative Construction", "CREATIVE",
           "Unlimited construction access for building, blueprinting and testing RuneForge's large-scale shaping tools."},
          {"labyrinth", "The Labyrinth", "Shifting Realm", "CHALLENGE",
           "Procedural maze expeditions built around discovery, shortcuts, relics and permanent mastery."},
          {"kingdom", "Kingdom Seed", "Settlement Sandbox", "SETTLEMENT",
           "Turn one shelter into roads, districts and a living settlement that understands the spaces you build."},
          {"deepblue", "Deep Blue", "Ocean Expedition", "EXPLORE",
           "Dive through reefs, trenches and flooded ruins where water, pressure and navigation reshape survival."},
      } {}

const ModeCard& HubModel::selectedMode() const noexcept {
    return modes_[std::min(selectedMode_, modes_.size() - 1)];
}

void HubModel::selectMode(const std::size_t index) noexcept {
    if (index < modes_.size()) selectedMode_ = index;
}

void HubModel::selectNav(const std::size_t index) noexcept {
    selectedNav_ = std::min<std::size_t>(index, 5);
}

} // namespace rf
