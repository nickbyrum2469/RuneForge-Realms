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

float fract01(float value) noexcept {
    return value - std::floor(value);
}

int floorDiv(int value, int divisor) noexcept {
    if (value >= 0) return value / divisor;
    return -(((-value) + divisor - 1) / divisor);
}

float smooth01(float t) noexcept {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

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

float globalVisualValue(std::uint32_t seed, int globalU, int globalV,
                        std::uint32_t salt) noexcept {
    return unit(hashBlock(seed, {globalU, 0, globalV}, salt));
}

// A small reconstruction filter turns independently hashed 16x16 addresses into coherent turf
// masses. The cells remain cubical addresses, but neighboring heights belong to one material bed
// instead of forming a high-frequency checkerboard of unrelated squares.
float smoothedVisualValue(std::uint32_t seed, int globalU, int globalV,
                          std::uint32_t salt) noexcept {
    float value = globalVisualValue(seed, globalU, globalV, salt) * 0.42f;
    value += globalVisualValue(seed, globalU - 1, globalV, salt) * 0.11f;
    value += globalVisualValue(seed, globalU + 1, globalV, salt) * 0.11f;
    value += globalVisualValue(seed, globalU, globalV - 1, salt) * 0.11f;
    value += globalVisualValue(seed, globalU, globalV + 1, salt) * 0.11f;
    value += globalVisualValue(seed, globalU - 1, globalV - 1, salt) * 0.035f;
    value += globalVisualValue(seed, globalU + 1, globalV - 1, salt) * 0.035f;
    value += globalVisualValue(seed, globalU - 1, globalV + 1, salt) * 0.035f;
    value += globalVisualValue(seed, globalU + 1, globalV + 1, salt) * 0.035f;
    return value;
}

VegetationProfile profileFor(std::uint32_t h, std::uint8_t& bladeCount) noexcept {
    const std::uint32_t roll = (h >> 5) % 100u;

    // Reference-correction: the premium target is a dense mass of short, blocky turf pieces,
    // not a field dominated by isolated vertical sticks. Keep a few taller accents, but make
    // two/three-piece compact tufts the majority of the deterministic anchor population.
    if (roll < 8u) {
        bladeCount = 1;
        return VegetationProfile::TinyBlade;
    }
    if (roll < 30u) {
        bladeCount = 1;
        return VegetationProfile::ShortBlade;
    }
    if (roll < 34u) {
        bladeCount = 1;
        return VegetationProfile::TallBlade;
    }
    if (roll < 64u) {
        bladeCount = 2;
        return VegetationProfile::TwoBladeTuft;
    }
    if (roll < 97u) {
        bladeCount = 3;
        return VegetationProfile::CompactTuft;
    }
    bladeCount = 2;
    return VegetationProfile::FlowerCapable;
}

void transformAnchor(float& u, float& v, std::uint32_t transform) noexcept {
    if ((transform & 1u) != 0u) std::swap(u, v);
    if ((transform & 2u) != 0u) u = fract01(1.0f - u);
    if ((transform & 4u) != 0u) v = fract01(1.0f - v);
}

void markRootCell(SurfaceReliefField& field, float u, float v) noexcept {
    const int cellU = std::clamp(static_cast<int>(u * SurfaceReliefField::resolution),
                                 0, SurfaceReliefField::resolution - 1);
    const int cellV = std::clamp(static_cast<int>(v * SurfaceReliefField::resolution),
                                 0, SurfaceReliefField::resolution - 1);
    auto& cell = field.cells[static_cast<std::size_t>(cellU + cellV * SurfaceReliefField::resolution)];
    if (cell.relief == ReliefClass::SoilClod && cell.occupied) cell.relief = ReliefClass::Root;
}

void appendRootSegment(SurfaceReliefField& field, std::uint32_t h,
                       float u0, float v0, float u1, float v1,
                       float width, float projection) noexcept {
    if (field.rootSegmentCount >= SurfaceReliefField::maxRootSegments) return;
    RootSegment segment;
    segment.stableId = static_cast<std::uint16_t>(h & 0xffffu);
    segment.u0 = std::clamp(u0, 0.001f, 0.999f);
    segment.v0 = std::clamp(v0, 0.001f, 0.999f);
    segment.u1 = std::clamp(u1, 0.001f, 0.999f);
    segment.v1 = std::clamp(v1, 0.001f, 0.999f);
    segment.width = width;
    segment.projection = projection;
    field.rootSegments[field.rootSegmentCount++] = segment;
    markRootCell(field, (segment.u0 + segment.u1) * 0.5f,
                 (segment.v0 + segment.v1) * 0.5f);
}

} // namespace

SurfaceReliefField SurfaceRelief::grassTop(std::uint32_t worldSeed, BlockCoord block,
                                           float worldAgeSeconds) noexcept {
    SurfaceReliefField field;
    const std::uint32_t blockBase = hashBlock(worldSeed, block, 0xa24baed5u);
    const float regionalDensity = regionalValue(worldSeed, block, 0x51ed270bu);
    const float regionalHeight = regionalValue(worldSeed, block, 0x7f4a7c15u, 9);
    const float regionalTone = regionalValue(worldSeed, block, 0x94d049bbu, 11);

    field.vegetationDensity = std::clamp(0.80f + regionalDensity * 0.15f +
                                         signedUnit(blockBase >> 9) * 0.015f,
                                         0.78f, 0.965f);
    field.vegetationHeightScale = 0.84f + regionalHeight * 0.18f +
                                  signedUnit(blockBase >> 15) * 0.020f;
    field.paletteFamily = static_cast<std::uint8_t>(std::min(3, static_cast<int>(regionalTone * 4.0f)));

    const float age = std::max(worldAgeSeconds, 0.0f);
    const float mature = std::min(std::floor(age / 64.0f), 2.0f) * 0.025f;

    // Physical turf bed: stable 16x16 addresses with spatially correlated height. This keeps the
    // clean micro-cube language while removing the unrelated-square/checkerboard surface character.
    for (int v = 0; v < SurfaceReliefField::resolution; ++v) {
        for (int u = 0; u < SurfaceReliefField::resolution; ++u) {
            const int globalU = block.x * SurfaceReliefField::resolution + u;
            const int globalV = block.z * SurfaceReliefField::resolution + v;
            const std::uint32_t h = hashCell(blockBase, u, v);
            const float bed = smoothedVisualValue(worldSeed, globalU, globalV, 0x2c1b3c6du);
            const float micro = globalVisualValue(worldSeed, globalU, globalV, 0x7a5b2239u);

            SurfaceCell cell;
            cell.stableId = static_cast<std::uint16_t>(h & 0xffffu);
            cell.colorFamily = static_cast<std::uint8_t>((field.paletteFamily + ((h >> 17) & 3u)) & 3u);
            cell.cavity = ((h >> 6) % 31u) == 0u;
            cell.relief = cell.cavity ? ReliefClass::Cavity : ReliefClass::Turf;
            cell.heightOffset = cell.cavity
                ? 0.0015f + micro * 0.0030f
                : std::clamp(0.0055f + bed * 0.0205f + (micro - 0.5f) * 0.0050f,
                             0.0040f, 0.0320f);
            cell.occupied = true;
            field.cells[static_cast<std::size_t>(u + v * SurfaceReliefField::resolution)] = cell;
        }
    }

    // Visible vegetation is a progressive R2 low-discrepancy sequence, independently scrambled and
    // mirrored per block. Unlike the old one-tuft-per-cell scheme, the resulting set has no row or
    // column ownership phase for the eye to reconstruct across a meadow. Each point still records
    // its owning 16x16 address for deterministic damage/promotion masking.
    constexpr float r2A = 0.7548776662466927f;
    constexpr float r2B = 0.5698402909980532f;
    const float scrambleU = unit(blockBase ^ 0x6d2b79f5u);
    const float scrambleV = unit(mix(blockBase ^ 0xb5297a4du));
    const std::uint32_t transform = (blockBase >> 21) & 7u;
    const int targetAnchors = std::clamp(
        static_cast<int>(std::lround(94.0f + field.vegetationDensity * 36.0f)), 120, 132);

    for (int i = 0; i < targetAnchors && i < SurfaceReliefField::maxGrassAnchors; ++i) {
        const std::uint32_t h = mix(blockBase ^ (0x9e3779b9u * static_cast<std::uint32_t>(i + 1)));
        float localU = fract01(scrambleU + static_cast<float>(i + 1) * r2A);
        float localV = fract01(scrambleV + static_cast<float>(i + 1) * r2B);
        transformAnchor(localU, localV, transform);

        // Tiny bounded warp prevents the same low-discrepancy motif from repeating as a rigid stamp,
        // while retaining the sequence's even spacing and clean coverage.
        localU = fract01(localU + signedUnit(h >> 4) * 0.0095f);
        localV = fract01(localV + signedUnit(h >> 13) * 0.0095f);

        GrassAnchor anchor;
        anchor.stableId = static_cast<std::uint16_t>(h & 0xffffu);
        anchor.localU = localU;
        anchor.localV = localV;
        anchor.ownerU = static_cast<std::uint8_t>(std::clamp(
            static_cast<int>(localU * SurfaceReliefField::resolution), 0,
            SurfaceReliefField::resolution - 1));
        anchor.ownerV = static_cast<std::uint8_t>(std::clamp(
            static_cast<int>(localV * SurfaceReliefField::resolution), 0,
            SurfaceReliefField::resolution - 1));
        anchor.colorFamily = static_cast<std::uint8_t>((field.paletteFamily + ((h >> 18) & 3u)) & 3u);
        anchor.vegetation = profileFor(h, anchor.bladeCount);

        // 0.6.2 removed the anchor lattice, but the actual pieces were still only ~1-1.5% of a
        // block wide and therefore read as skinny lawn spikes. Keep the same anchor/budget model
        // and widen each anchor into a short chunky turf voxel/clump instead.
        anchor.widthScale = 2.00f + unit(h >> 10) * 0.90f;
        const float localHeight = 0.80f + unit(h >> 7) * 0.28f;
        anchor.bladeHeight = (0.020f + unit(h >> 15) * 0.030f) *
                             field.vegetationHeightScale * localHeight * (1.0f + mature);

        field.grassAnchors[field.grassAnchorCount++] = anchor;

        // Keep a cell-level summary for future ecology/damage queries, but it is no longer the draw
        // distribution. Multiple anchors may belong to one cell and many cells intentionally have none.
        auto& owner = field.cells[static_cast<std::size_t>(anchor.ownerU +
            anchor.ownerV * SurfaceReliefField::resolution)];
        if (owner.vegetation == VegetationProfile::Bare) owner.vegetation = anchor.vegetation;
        owner.bladeCount = std::max(owner.bladeCount, anchor.bladeCount);
        owner.bladeHeight = std::max(owner.bladeHeight, anchor.bladeHeight);
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
            const float center = unit(h);
            const float neighborAverage = (unit(hashCell(base, u - 1, v)) +
                                           unit(hashCell(base, u + 1, v)) +
                                           unit(hashCell(base, u, v - 1)) +
                                           unit(hashCell(base, u, v + 1))) * 0.25f;
            const float clodMass = center * 0.58f + neighborAverage * 0.42f;

            SurfaceCell cell;
            cell.stableId = static_cast<std::uint16_t>(h & 0xffffu);
            cell.colorFamily = static_cast<std::uint8_t>((field.paletteFamily + ((h >> 14) & 3u)) & 3u);

            const int irregularLip = 2 + static_cast<int>((hashCell(base ^ 0x8da6b343u, u, 0) >> 7) % 3u);
            const bool turfLip = includeTurfLip && v >= SurfaceReliefField::resolution - irregularLip;
            const bool mineral = !turfLip && ((h >> 11) % 41u) == 0u;
            const bool cavity = !turfLip && !mineral && ((h >> 5) % 17u) == 0u;

            if (turfLip) {
                cell.relief = ReliefClass::Turf;
                cell.heightOffset = std::clamp(0.028f + clodMass * 0.038f +
                                               signedUnit(h >> 9) * 0.005f,
                                               0.025f, 0.071f);
            } else if (mineral) {
                cell.relief = ReliefClass::Mineral;
                cell.heightOffset = std::clamp(0.036f + clodMass * 0.036f,
                                               0.034f, 0.075f);
            } else if (cavity) {
                cell.relief = ReliefClass::Cavity;
                cell.cavity = true;
                cell.occupied = false;
                cell.heightOffset = 0.0f;
            } else {
                cell.relief = ReliefClass::SoilClod;
                cell.heightOffset = std::clamp(0.014f + clodMass * 0.047f +
                                               signedUnit(h >> 8) * 0.006f,
                                               0.011f, 0.067f);
            }

            field.cells[static_cast<std::size_t>(u + v * SurfaceReliefField::resolution)] = cell;
        }
    }

    if (!includeTurfLip) return field;

    // Four to six thinner stepped root paths descend from the turf. The reference uses many fine
    // embedded fibers rather than a few dominant columns; the soil shell remains complete beneath
    // every path so roots never replace the actual clod structure.
    const int rootCount = 4 + static_cast<int>((base >> 19) % 3u);
    for (int root = 0; root < rootCount; ++root) {
        const std::uint32_t rootHash = mix(base ^ (0x632be59bu * static_cast<std::uint32_t>(root + 1)));
        float u = std::clamp((static_cast<float>(root + 1) / static_cast<float>(rootCount + 1)) +
                             signedUnit(rootHash >> 4) * 0.070f,
                             0.07f, 0.93f);
        float v = 0.91f - unit(rootHash >> 12) * 0.055f;
        const int steps = 3 + static_cast<int>((rootHash >> 22) % 3u);

        for (int step = 0; step < steps && field.rootSegmentCount < SurfaceReliefField::maxRootSegments; ++step) {
            const std::uint32_t stepHash = mix(rootHash ^
                (0x85ebca6bu * static_cast<std::uint32_t>(step + 1)));
            const float width = 0.0045f + unit(stepHash >> 5) * 0.0045f;
            const float projection = 0.0050f + unit(stepHash >> 14) * 0.0050f;
            const float drop = 0.065f + unit(stepHash >> 8) * 0.055f;
            const float nextV = std::max(0.28f, v - drop);
            appendRootSegment(field, stepHash, u, v, u, nextV, width, projection);
            v = nextV;

            if (step + 1 < steps) {
                const float drift = signedUnit(stepHash >> 18) * (0.030f + unit(stepHash >> 2) * 0.040f);
                const float nextU = std::clamp(u + drift, 0.055f, 0.945f);
                if (std::abs(nextU - u) > 0.009f) {
                    appendRootSegment(field, mix(stepHash ^ 0x27d4eb2fu),
                                      u, v, nextU, v, width * 0.88f, projection);
                }
                u = nextU;
            }

            if (step == 1 && ((stepHash >> 25) & 1u) != 0u &&
                field.rootSegmentCount + 2 < SurfaceReliefField::maxRootSegments) {
                const float direction = ((stepHash >> 26) & 1u) != 0u ? 1.0f : -1.0f;
                const float branchU = std::clamp(u + direction * (0.045f + unit(stepHash >> 3) * 0.035f),
                                                 0.04f, 0.96f);
                appendRootSegment(field, mix(stepHash ^ 0x51ed270bu),
                                  u, v, branchU, v, width * 0.68f, projection * 0.9f);
                appendRootSegment(field, mix(stepHash ^ 0x94d049bbu),
                                  branchU, v, branchU, std::max(0.32f, v - 0.060f),
                                  width * 0.60f, projection * 0.85f);
            }
        }
    }

    return field;
}

} // namespace rf::world::surface
