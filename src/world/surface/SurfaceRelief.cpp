#include "world/surface/SurfaceRelief.h"

#include <algorithm>
#include <cmath>

namespace rf::world::surface {
namespace {

std::uint32_t mix(std::uint32_t h) noexcept {
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    h *= 0x846ca68bu;
    h ^= h >> 16;
    return h;
}

std::uint32_t hashBlock(std::uint32_t seed, BlockCoord block, std::uint32_t salt) noexcept {
    std::uint32_t h = seed ^ salt;
    h ^= static_cast<std::uint32_t>(block.x) * 0x85ebca6bu;
    h ^= static_cast<std::uint32_t>(block.y) * 0xc2b2ae35u;
    h ^= static_cast<std::uint32_t>(block.z) * 0x27d4eb2fu;
    return mix(h);
}

std::uint32_t hashCell(std::uint32_t base, int u, int v) noexcept {
    std::uint32_t h = base;
    h ^= static_cast<std::uint32_t>(u + 1) * 0x9e3779b9u;
    h ^= static_cast<std::uint32_t>(v + 1) * 0x632be59bu;
    return mix(h);
}

float unit(std::uint32_t value) noexcept {
    return static_cast<float>(value & 0xffffu) / 65535.0f;
}

float signedUnit(std::uint32_t value) noexcept {
    return unit(value) * 2.0f - 1.0f;
}

int floorDiv4(int value) noexcept {
    if (value >= 0) return value / 4;
    return -(((-value) + 3) / 4);
}

std::uint32_t patchHash(std::uint32_t seed, BlockCoord block) noexcept {
    BlockCoord patch{floorDiv4(block.x), 0, floorDiv4(block.z)};
    return hashBlock(seed, patch, 0x51ed270bu);
}

VegetationProfile profileFor(std::uint32_t h, std::uint8_t& bladeCount) noexcept {
    const std::uint32_t roll = (h >> 5) % 100u;
    if (roll < 13u) {
        bladeCount = 1;
        return VegetationProfile::TinyBlade;
    }
    if (roll < 37u) {
        bladeCount = 1;
        return VegetationProfile::ShortBlade;
    }
    if (roll < 51u) {
        bladeCount = 1;
        return VegetationProfile::TallBlade;
    }
    if (roll < 76u) {
        bladeCount = 2;
        return VegetationProfile::TwoBladeTuft;
    }
    if (roll < 96u) {
        bladeCount = 3;
        return VegetationProfile::CompactTuft;
    }
    bladeCount = 2;
    return VegetationProfile::FlowerCapable;
}

bool rootCell(std::uint32_t base, int u, int v) noexcept {
    const int depth = SurfaceReliefField::resolution - 1 - v;
    if (depth < 2) return false;

    const int startA = 2 + static_cast<int>((base >> 3) % 12u);
    const int startB = 2 + static_cast<int>((base >> 12) % 12u);
    const int driftA = ((base >> 22) & 1u) ? 1 : -1;
    const int driftB = ((base >> 23) & 1u) ? 1 : -1;
    const int centerA = std::clamp(startA + driftA * (depth / 4), 1, 14);
    const int centerB = std::clamp(startB + driftB * (depth / 5), 1, 14);

    if (u == centerA || (depth > 7 && u == centerB)) return true;

    // Deterministic short side branches. They only occupy a few cells so roots remain readable.
    if (depth >= 5 && depth <= 10 && (depth % 3) == static_cast<int>((base >> 27) % 3u)) {
        const int branch = centerA + ((((base >> (depth % 16)) & 1u) != 0u) ? 1 : -1);
        if (u == branch) return true;
    }
    return false;
}

} // namespace

SurfaceReliefField SurfaceRelief::grassTop(std::uint32_t worldSeed, BlockCoord block,
                                           float worldAgeSeconds) noexcept {
    SurfaceReliefField field;
    const std::uint32_t patch = patchHash(worldSeed, block);
    const std::uint32_t blockBase = hashBlock(worldSeed, block, 0xa24baed5u);

    const float patchDensity = 0.88f + unit(patch) * 0.08f;
    const float blockDensityOffset = signedUnit(blockBase >> 9) * 0.035f;
    field.vegetationDensity = std::clamp(patchDensity + blockDensityOffset, 0.84f, 0.98f);
    field.vegetationHeightScale = 0.90f + unit(patch >> 7) * 0.18f;
    field.paletteFamily = static_cast<std::uint8_t>((patch >> 19) & 3u);

    // Age is quantized and may mature height slightly, but it never changes layout/presence.
    const float age = std::max(worldAgeSeconds, 0.0f);
    const float mature = std::min(std::floor(age / 64.0f), 2.0f) * 0.035f;

    for (int v = 0; v < SurfaceReliefField::resolution; ++v) {
        for (int u = 0; u < SurfaceReliefField::resolution; ++u) {
            const std::uint32_t h = hashCell(blockBase, u, v);
            SurfaceCell cell;
            cell.stableId = static_cast<std::uint16_t>(h & 0xffffu);
            cell.colorFamily = static_cast<std::uint8_t>((field.paletteFamily + ((h >> 17) & 3u)) & 3u);
            cell.cavity = ((h >> 6) % 17u) == 0u;
            cell.relief = cell.cavity ? ReliefClass::Cavity : ReliefClass::Turf;
            cell.heightOffset = cell.cavity
                ? 0.0015f + unit(h >> 10) * 0.0045f
                : 0.008f + unit(h >> 10) * 0.020f;
            cell.occupied = true;

            // Jitter applies only to the vegetation attachment, not the tile itself, preserving a
            // clean continuous constructed turf surface while breaking long blade row phases.
            cell.vegetationOffsetU = signedUnit(h >> 4) * 0.020f;
            cell.vegetationOffsetV = signedUnit(h >> 13) * 0.020f;

            const float vegetationRoll = unit(h >> 20);
            if (vegetationRoll <= field.vegetationDensity) {
                cell.vegetation = profileFor(h, cell.bladeCount);
                const float localVariation = 0.80f + unit(h >> 8) * 0.40f;
                cell.bladeHeight = (0.038f + unit(h >> 15) * 0.042f) *
                                   field.vegetationHeightScale * localVariation * (1.0f + mature);
            }

            field.cells[static_cast<std::size_t>(u + v * SurfaceReliefField::resolution)] = cell;
        }
    }
    return field;
}

SurfaceReliefField SurfaceRelief::rootedSide(std::uint32_t worldSeed, BlockCoord block,
                                             SurfaceFace face) noexcept {
    SurfaceReliefField field;
    const std::uint32_t faceSalt = 0x3c6ef372u + static_cast<std::uint32_t>(face) * 0x9e3779b9u;
    const std::uint32_t base = hashBlock(worldSeed, block, faceSalt);
    field.paletteFamily = static_cast<std::uint8_t>((base >> 18) & 3u);

    for (int v = 0; v < SurfaceReliefField::resolution; ++v) {
        for (int u = 0; u < SurfaceReliefField::resolution; ++u) {
            const std::uint32_t h = hashCell(base, u, v);
            SurfaceCell cell;
            cell.stableId = static_cast<std::uint16_t>(h & 0xffffu);
            cell.colorFamily = static_cast<std::uint8_t>((field.paletteFamily + ((h >> 14) & 3u)) & 3u);

            const int irregularLip = 2 + static_cast<int>((hashCell(base ^ 0x8da6b343u, u, 0) >> 7) % 3u);
            const bool turfLip = v >= SurfaceReliefField::resolution - irregularLip;
            const bool root = !turfLip && rootCell(base, u, v);
            const bool mineral = !turfLip && !root && ((h >> 11) % 43u) == 0u;
            const bool cavity = !turfLip && !root && !mineral && ((h >> 5) % 9u) == 0u;

            if (turfLip) {
                cell.relief = ReliefClass::Turf;
                cell.heightOffset = 0.012f + unit(h >> 8) * 0.022f;
            } else if (root) {
                cell.relief = ReliefClass::Root;
                cell.heightOffset = 0.028f + unit(h >> 8) * 0.028f;
            } else if (mineral) {
                cell.relief = ReliefClass::Mineral;
                cell.heightOffset = 0.018f + unit(h >> 8) * 0.020f;
            } else if (cavity) {
                cell.relief = ReliefClass::Cavity;
                cell.cavity = true;
                cell.occupied = false; // The macro face remains behind it, reading as a recess.
                cell.heightOffset = 0.001f;
            } else {
                cell.relief = ReliefClass::SoilClod;
                cell.heightOffset = 0.007f + unit(h >> 8) * 0.030f;
            }

            field.cells[static_cast<std::size_t>(u + v * SurfaceReliefField::resolution)] = cell;
        }
    }
    return field;
}

} // namespace rf::world::surface
