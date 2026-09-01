#include "world/micro/MicroVoxelEdit.h"

#include <bitset>

namespace rf::world::micro {

MicroVoxelEdit makeEdit(BlockCoord position, BlockId block, const MicroVoxelState& state) noexcept {
    MicroVoxelEdit edit;
    edit.position = position;
    edit.block = block;
    for (std::size_t word = 0; word < edit.occupancyWords.size(); ++word) {
        std::uint64_t value = 0;
        for (std::size_t bit = 0; bit < 64; ++bit) {
            const std::size_t index = word * 64 + bit;
            if (state.bits().test(index)) value |= (std::uint64_t{1} << bit);
        }
        edit.occupancyWords[word] = value;
    }
    return edit;
}

MicroVoxelState stateFromEdit(const MicroVoxelEdit& edit) noexcept {
    std::bitset<cellCount> bits;
    for (std::size_t word = 0; word < edit.occupancyWords.size(); ++word) {
        const std::uint64_t value = edit.occupancyWords[word];
        for (std::size_t bit = 0; bit < 64; ++bit) {
            if ((value & (std::uint64_t{1} << bit)) != 0) bits.set(word * 64 + bit);
        }
    }
    MicroVoxelState state;
    state.assignBits(bits);
    return state;
}

} // namespace rf::world::micro
