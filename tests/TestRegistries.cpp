#include "TestSuites.h"

#include "core/HubModel.h"
#include "core/Version.h"
#include "render/materials/MaterialRegistry.h"
#include "world/blocks/BlockRegistry.h"

#include <cassert>

void runRegistryTests() {
    const auto oldVersion = rf::Version::parse("0.1.0");
    const auto currentVersion = rf::Version::parse("v0.3.2");
    assert(oldVersion && currentVersion && *currentVersion > *oldVersion);
    assert(oldVersion->toString() == "0.1.0");
    assert(!rf::Version::parse("0.1"));

    rf::HubModel hub;
    assert(!hub.hasSave());
    hub.setHasSave(true);
    assert(hub.hasSave());
    hub.selectNav(99);
    assert(hub.selectedNavIndex() == 3);

    const auto& stone = rf::world::blocks::BlockRegistry::get(rf::world::BlockId::Stone);
    assert(stone.name == "Stone");
    assert(stone.preferredTool == rf::world::blocks::ToolClass::Pickaxe);
    assert(stone.hardness > 1.0f);

    const auto& material = rf::render::materials::MaterialRegistry::get(rf::world::SurfaceMaterial::Stone);
    assert(material.name == "weathered_stone");
    assert(material.detailProfile == "fractured_rock");
}
