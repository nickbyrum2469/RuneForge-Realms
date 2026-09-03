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

int floorDiv(int value, int divisor) noexcept {
    if (value >= 0) return value / divisor;
    return -(((-value) + divisor - 1) / divisor);
}

float smooth01(float t) noexcept {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// Broad ecological variation must be continuous. The 0.6.0 implementation hashed hard 4x4
// regions, which removed exact block cloning but created square density/palette boundaries that
// were still visible as long meadow bands. Bilinear lattice noise keeps the regional tendency
// while removing those hard world-grid transitions.
float regionalValue(std::uint32_t seed, BlockCoord block, std::uint32_t salt,
                    int span = 7) noexcept {
    const int gx = floorDiv(block.x, span);
    const int gz = floorDiv(block.z, span);
    const int localX = block.x - gx * span;
    const int localZ = block.z - gz * span;
    const float tx = smooth01((static_cast<float>(localX) + 0.5f) / static_cast<float>(span));
    const float tz = smooth01((static_cast<float>(localZ) + 0.5f) / static_cast<float>(span));

    const auto lattice = [&](int x, int z) {
        return unit(hashBlock(seed, {x, 0, z}, salt));
    };
    const float a = lattice(gx, gz);
    const float b = lattice(gx + 1, gz);
    const float c = lattice(gx, gz + 1);
    const float d = lattice(gx + 1, gz + 1);
    const float x0 = a + (b - a) * tx;
    const float x1 = c + (d - c) * tx;
    return x0 + (x1 - x0) * tz;
}

VegetationProfile profileFor(std::uint32_t h, std::uint8_t& bladeCount) noexcept {
    const std::uint32_t roll = (h >> 5) % 100u;
    if (roll < 12u) {
        bladeCount = 1;
        return VegetationProfile::TinyBlade;
    }
    if (roll < 35u) {
        bladeCount = 1;
        return VegetationProfile::ShortBlade;
    }
    if (roll < 49u) {
        bladeCount = 1;
        return VegetationProfile::TallBlade;
    }
    if (roll < 75u) {
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

    if (u == centerA || (depth > 8 && u == centerB)) return true;
    if (depth >= 5 && depth <= 11 && (depth % 3) == static_cast<int>((base >> 27) % 3u)) {
        const int branch = centerA + ((((base >> (depth % 16)) & 1u) != 0u) ? 1 : -1);
        if (u == branch) return true;
    }
    return false;
}

} // namespace

SurfaceReliefField SurfaceRelief::grassTop(std::uint32_t worldSeed, BlockCoord block,
                                           float worldAgeSeconds) noexcept {
    SurfaceReliefField field;
    const std::uint32_t blockBase = hashBlock(worldSeed, block, 0xa24baed5u);
    const float regionalDensity = regionalValue(worldSeed, block, 0x51ed270bu);
    const float regionalHeight = regionalValue(worldSeed, block, 0x7f4a7c15u, 9);
    const float regionalTone = regionalValue(worldSeed, block, 0x94d049bbu, 11);

    // Dense meadow coverage with a small per-block identity layered over a continuous regional
    // field. No hard 4x4 patch boundaries are allowed to show up in the world anymore.
    field.vegetationDensity = std::clamp(0.900f + regionalDensity * 0.055f +
                                         signedUnit(blockBase >> 9) * 0.015f,
                                         0.88f, 0.975f);
    field.vegetationHeightScale = 0.88f + regionalHeight * 0.20f +
                                  signedUnit(blockBase >> 15) * 0.025f;
    field.paletteFamily = static_cast<std::uint8_t>(std::min(3, static_cast<int>(regionalTone * 4.0f)));

    const float age = std::max(worldAgeSeconds, 0.0f);
    const float mature = std::min(std::floor(age / 64.0f), 2.0f) * 0.035f;

    for (int v = 0; v < SurfaceReliefField::resolution; ++v) {
        for (int u = 0; u < SurfaceReliefField::resolution; ++u) {
            const std::uint32_t h = hashCell(blockBase, u, v);
            const std::uint32_t warp = hashCell(blockBase ^ 0xb5297a4du, v, u);
            SurfaceCell cell;
            cell.stableId = static_cast<std::uint16_t>(h & 0xffffu);
            cell.colorFamily = static_cast<std::uint8_t>((field.paletteFamily + ((h >> 17) & 3u)) & 3u);
            cell.cavity = ((h >> 6) % 19u) == 0u;
            cell.relief = cell.cavity ? ReliefClass::Cavity : ReliefClass::Turf;
            cell.heightOffset = cell.cavity
                ? 0.0015f + unit(h >> 10) * 0.0040f
                : 0.0060f + unit(h >> 10) * 0.0220f;
            cell.occupied = true;

            // A 16x16 address still maps cleanly to physical micro voxels, but the vegetation anchor
            // is allowed to roam through almost half of its cell. The extra cross-axis warp destroys
            // the visible lattice/row phase without turning the turf into unbounded random noise.
            const float jitterU = signedUnit(h >> 4) * 0.0215f + signedUnit(warp >> 8) * 0.0080f;
            const float jitterV = signedUnit(h >> 13) * 0.0215f + signedUnit(warp >> 17) * 0.0080f;
            cell.vegetationOffsetU = std::clamp(jitterU, -0.0280f, 0.0280f);
            cell.vegetationOffsetV = std::clamp(jitterV, -0.0280f, 0.0280f);

            const float vegetationRoll = unit(h >> 20);
            if (vegetationRoll <= field.vegetationDensity) {
                cell.vegetation = profileFor(h, cell.bladeCount);
                const float localVariation = 0.76f + unit(h >> 8) * 0.46f;
                cell.bladeHeight = (0.034f + unit(h >> 15) * 0.046f) *
                                   field.vegetationHeightScale * localVariation * (1.0f + mature);
            }

            field.cells[static_cast<std::size_t>(u + v * SurfaceReliefField::resolution)] = cell;
        }
    }
    return field;
}

SurfaceReliefField SurfaceRelief::soilSide(std::uint32_t worldSeed, BlockCoord block,
                                           SurfaceFace face, bool includeTurfLip) noexcept {
    SurfaceReliefField field;
    const std::uint32_t faceSalt = 0x3c6ef372u + static_cast<std::uint32_t>(face) * 0x9e3779b9u;
    const std::uint32_t base = hashBlock(worldSeed, block, faceSalt);
    const float regionalTone = regionalValue(worldSeed, block, faceSalt ^ 0x68e31da4u, 9);
    field.paletteFamily = static_cast<std::uint8_t>(std::min(3, static_cast<int>(regionalTone * 4.0f)));

    for (int v = 0; v < SurfaceReliefField::resolution; ++v) {
        for (int u = 0; u < SurfaceReliefField::resolution; ++u) {
            const std::uint32_t h = hashCell(base, u, v);
            SurfaceCell cell;
            cell.stableId = static_cast<std::uint16_t>(h & 0xffffu);
            cell.colorFamily = static_cast<std::uint8_t>((field.paletteFamily + ((h >> 14) & 3u)) & 3u);

            const int irregularLip = 2 + static_cast<int>((hashCell(base ^ 0x8da6b343u, u, 0) >> 7) % 3u);
            const bool turfLip = includeTurfLip && v >= SurfaceReliefField::resolution - irregularLip;
            const bool root = includeTurfLip && !turfLip && rootCell(base, u, v);
            const bool mineral = !turfLip && !root && ((h >> 11) % 47u) == 0u;
            const bool cavity = !turfLip && !root && !mineral && ((h >> 5) % 11u) == 0u;

            if (turfLip) {
                cell.relief = ReliefClass::Turf;
                cell.heightOffset = 0.008f + unit(h >> 8) * 0.018f;
            } else if (root) {
                // Root cells are path/address markers. The mesher now draws a narrow fiber over a
                // normal soil plate instead of extruding an entire 1/16-cell brown column.
                cell.relief = ReliefClass::Root;
                cell.heightOffset = 0.009f + unit(h >> 8) * 0.012f;
            } else if (mineral) {
                cell.relief = ReliefClass::Mineral;
                cell.heightOffset = 0.010f + unit(h >> 8) * 0.017f;
            } else if (cavity) {
                cell.relief = ReliefClass::Cavity;
                cell.cavity = true;
                cell.occupied = false;
                cell.heightOffset = 0.001f;
            } else {
                cell.relief = ReliefClass::SoilClod;
                cell.heightOffset = 0.004f + unit(h >> 8) * 0.021f;
            }

            field.cells[static_cast<std::size_t>(u + v * SurfaceReliefField::resolution)] = cell;
        }
    }
    return field;
}

} // namespace rf::world::surface
