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

// Vegetation is deliberately not one-anchor-per-surface-cell. The 16x16 SurfaceCell lattice is a
// stable physical address space for damage/promotion; GrassAnchor is a separate deterministic
// low-discrepancy point set that can place zero, one, or several tufts in an owning cell. This is
// the key invariant that prevents the physical address grid from becoming visible as meadow rows.
struct GrassAnchor {
    std::uint16_t stableId{};
    float localU{}; // [0,1) within the macro block.
    float localV{}; // [0,1) within the macro block.
    float bladeHeight{};
    float widthScale{1.0f};
    std::uint8_t ownerU{};
    std::uint8_t ownerV{};
    std::uint8_t colorFamily{};
    std::uint8_t bladeCount{1};
    VegetationProfile vegetation{VegetationProfile::ShortBlade};
};

// Roots are explicit short voxel-fiber segments over a complete soil shell. A RootSegment never
// replaces the soil cell beneath it, so a grazing view cannot reveal a detached root floating over
// an otherwise flat wall. Segments are axis-aligned in face-local space by design: the reference
// language is stepped/voxel-rooted rather than smooth spline geometry.
struct RootSegment {
    std::uint16_t stableId{};
    float u0{};
    float v0{};
    float u1{};
    float v1{};
    float width{};
    float projection{};
};

struct SurfaceReliefField {
    static constexpr int resolution = 16;
    static constexpr int cellCount = resolution * resolution;
    static constexpr int maxGrassAnchors = 144;
    static constexpr int maxRootSegments = 32;

    std::array<SurfaceCell, cellCount> cells{};
    std::array<GrassAnchor, maxGrassAnchors> grassAnchors{};
    std::array<RootSegment, maxRootSegments> rootSegments{};
    std::uint16_t grassAnchorCount{};
    std::uint8_t rootSegmentCount{};
    float vegetationDensity{};
    float vegetationHeightScale{1.0f};
    std::uint8_t paletteFamily{};

    [[nodiscard]] const SurfaceCell& cell(int u, int v) const noexcept {
        return cells[static_cast<std::size_t>(u + v * resolution)];
    }

    [[nodiscard]] const GrassAnchor& grassAnchor(std::size_t index) const noexcept {
        return grassAnchors[index];
    }

    [[nodiscard]] const RootSegment& rootSegment(std::size_t index) const noexcept {
        return rootSegments[index];
    }
};

struct SurfaceReliefBudget {
    int cellStride{1};
    std::uint16_t maxTopReliefCells{};
    std::uint16_t maxSideReliefCells{};
    std::uint16_t maxVegetationCells{};
    std::uint16_t maxRootSegments{};
    bool emitTopRelief{true};
    bool emitSideRelief{true};
    bool emitRoots{true};
    bool emitSideWalls{true};
};

class SurfaceRelief {
public:
    // 16 visual cells per block face remain the stable address density and map exactly 2x2 onto
    // each existing 8x8 physical micro voxel. Visible grass placement is intentionally independent
    // from this lattice, while every anchor still records the cell that owns it for later damage.
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

    // Hero side relief uses a complete connected micro-prism field. Standard keeps the same world
    // truth but draws only a reduced front-shell representation; distant detail is macro-only.
    [[nodiscard]] static constexpr SurfaceReliefBudget heroBudget() noexcept {
        return {1, 256, 256, 128, 28, true, true, true, true};
    }
    [[nodiscard]] static constexpr SurfaceReliefBudget standardBudget() noexcept {
        return {2, 64, 64, 44, 10, true, true, true, false};
    }
    [[nodiscard]] static constexpr SurfaceReliefBudget distantBudget() noexcept {
        return {4, 0, 0, 0, 0, false, false, false, false};
    }
};

} // namespace rf::world::surface
