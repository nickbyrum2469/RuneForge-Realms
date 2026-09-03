#include "TestSuites.h"

#include "core/HubModel.h"
#include "core/Version.h"
#include "render/materials/MaterialRegistry.h"
#include "world/blocks/BlockRegistry.h"

#include <cassert>

void runRegistryTests() {
    const auto oldVersion = rf::Version::parse("0.1.0");
    const auto currentVersion = rf::Version::parse(RF_VERSION_STRING);
    assert(oldVersion && currentVersion && *currentVersion > *oldVersion);
    assert(oldVersion->toString() == "0.1.0");
    assert(currentVersion->toString() == RF_VERSION_STRING);
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
    assert(stone.minimumToolTier == 1);
    assert(stone.hardness > 1.0f);

    const auto& waterBlock = rf::world::blocks::BlockRegistry::get(rf::world::BlockId::Water);
    assert(waterBlock.name == "Water");
    assert(!waterBlock.solid);
    assert(waterBlock.transparent);

    const auto& materials = rf::render::materials::MaterialRegistry::all();
    assert(materials.size() == rf::render::materials::MaterialRegistry::materialCount);
    assert(materials.size() == static_cast<std::size_t>(rf::world::SurfaceMaterial::RootFiber) + 1u);

    const auto& material = rf::render::materials::MaterialRegistry::get(rf::world::SurfaceMaterial::Stone);
    assert(material.name == "fractured_stone");
    assert(material.detailProfile == "natural_plate_rock");
    assert(material.reliefStrength > 0.5f);

    const auto& flower = rf::render::materials::MaterialRegistry::get(rf::world::SurfaceMaterial::FlowerBlue);
    assert(flower.name == "flower_blue");
    assert(flower.emissive > 0.0f);

    const auto& waterMaterial = rf::render::materials::MaterialRegistry::get(rf::world::SurfaceMaterial::Water);
    assert(waterMaterial.name == "frontier_water");
    assert(waterMaterial.roughness < 0.3f);

    const auto& heroMetal = rf::render::materials::MaterialRegistry::get(rf::world::SurfaceMaterial::CharacterMetal);
    assert(heroMetal.name == "hero_steel");
    assert(heroMetal.metallic > 0.5f);

    const auto& heroHair = rf::render::materials::MaterialRegistry::get(rf::world::SurfaceMaterial::CharacterHair);
    const auto& heroEyeWhite = rf::render::materials::MaterialRegistry::get(rf::world::SurfaceMaterial::CharacterEyeWhite);
    const auto& heroEyeBlue = rf::render::materials::MaterialRegistry::get(rf::world::SurfaceMaterial::CharacterEyeBlue);
    const auto& heroLoincloth = rf::render::materials::MaterialRegistry::get(rf::world::SurfaceMaterial::CharacterLoincloth);
    assert(heroHair.name == "hero_dark_hair");
    assert(heroEyeWhite.name == "hero_eye_white");
    assert(heroEyeBlue.name == "hero_eye_blue");
    assert(heroLoincloth.name == "hero_loincloth");

    const auto& rootFiber = rf::render::materials::MaterialRegistry::get(rf::world::SurfaceMaterial::RootFiber);
    assert(rootFiber.name == "root_fiber");
    assert(rootFiber.detailProfile == "thin_branching_root_fiber");

    assert(rf::world::surfaceMaterial(rf::world::BlockId::Grass, 1, +1) == rf::world::SurfaceMaterial::GrassTop);
    assert(rf::world::surfaceMaterial(rf::world::BlockId::Grass, 0, +1) == rf::world::SurfaceMaterial::GrassSide);
    assert(rf::world::surfaceMaterial(rf::world::BlockId::Wood, 1, +1) == rf::world::SurfaceMaterial::WoodCut);
    assert(rf::world::surfaceMaterial(rf::world::BlockId::Wood, 2, +1) == rf::world::SurfaceMaterial::WoodBark);
    assert(rf::world::surfaceMaterial(rf::world::BlockId::Water, 1, +1) == rf::world::SurfaceMaterial::Water);
}
