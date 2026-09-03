#pragma once

#include "world/WorldEdit.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace rf::world::surface {

enum class SurfaceFace : std::uint8_t {
    Top = 0,
    North,
    South,
    East,
    West,
};

enum class ReliefClass : std::uint8_t {
    Turf = 0,
    SoilClod,
    Cavity,
    Root,
    Mineral,
};

enum class VegetationProfile : std::uint8_t {
    Bare = 0,
    TinyBlade,
    ShortBlade,
    TallBlade,
    TwoBladeTuft,
    CompactTuft,
    FlowerCapable,
};

struct SurfaceCell {
    std::uint16_t stableId{};
    float heightOffset{};
    float vegetationOffsetU{};
    float vegetationOffsetV{};
    float bladeHeight{};
    std::uint8_t colorFamily{};
    std::uint8_t bladeCount{};
    ReliefClass relief{ReliefClass::SoilClod};
    VegetationProfile vegetation{VegetationProfile::Bare};
    bool occupied{true};
    bool cavity{false};
};

struct SurfaceReliefField {
    static constexpr int resolution = 16;
    static constexpr int cellCount = resolution * resolution;

    std::array<SurfaceCell, cellCount> cells{};
    float vegetationDensity{};
    float vegetationHeightScale{1.0f};
    std::uint8_t paletteFamily{};

    [[nodiscard]] const SurfaceCell& cell(int u, int v) const noexcept {
        return cells[static_cast<std::size_t>(u + v * resolution)];
    }
};

struct SurfaceReliefBudget {
    int cellStride{1};
    std::uint16_t maxTopReliefCells{};
    std::uint16_t maxSideReliefCells{};
    std::uint16_t maxVegetationCells{};
    bool emitTopRelief{true};
    bool emitSideRelief{true};
    bool emitRoots{true};
};

class SurfaceRelief {
public:
    // 16 visual cells per block face remains the stable address density. It maps exactly 2x2
    // visual cells onto each existing 8x8 physical micro voxel. Placement/relief may vary inside
    // that address, but damage/promotion ownership never depends on frame-time randomness.
    static constexpr int visualResolution = SurfaceReliefField::resolution;
    static constexpr int visualCellsPerMicroVoxel = 2;

    [[nodiscard]] static SurfaceReliefField grassTop(std::uint32_t worldSeed, BlockCoord block,
                                                      float worldAgeSeconds) noexcept;
    [[nodiscard]] static SurfaceReliefField soilSide(std::uint32_t worldSeed, BlockCoord block,
                                                      SurfaceFace face,
                                                      bool includeTurfLip) noexcept;

    [[nodiscard]] static constexpr int microCellForVisualCell(int visualCell) noexcept {
        return visualCell / visualCellsPerMicroVoxel;
    }

    // Side cells are now single outward micro-plates instead of six-quad cuboids, so Hero can cover
    // nearly the whole visible soil face at roughly the same order of geometry cost as the older
    // sparse 48-box treatment. The dense mosaic is what makes the soil itself read as constructed.
    [[nodiscard]] static constexpr SurfaceReliefBudget heroBudget() noexcept {
        return {1, 256, 224, 112, true, true, true};
    }
    [[nodiscard]] static constexpr SurfaceReliefBudget standardBudget() noexcept {
        return {2, 64, 48, 40, true, true, true};
    }
    [[nodiscard]] static constexpr SurfaceReliefBudget distantBudget() noexcept {
        return {4, 0, 0, 0, false, false, false};
    }
};

} // namespace rf::world::surface
