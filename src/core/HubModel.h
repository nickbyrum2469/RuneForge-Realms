#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace rf {

struct ModeCard {
    std::string id;
    std::string title;
    std::string subtitle;
    std::string tag;
    std::string description;
};

class HubModel {
public:
    HubModel();

    [[nodiscard]] const std::vector<ModeCard>& modes() const noexcept { return modes_; }
    [[nodiscard]] const ModeCard& selectedMode() const noexcept;
    [[nodiscard]] std::size_t selectedModeIndex() const noexcept { return selectedMode_; }
    [[nodiscard]] std::size_t selectedNavIndex() const noexcept { return selectedNav_; }

    void selectMode(std::size_t index) noexcept;
    void selectNav(std::size_t index) noexcept;

private:
    std::vector<ModeCard> modes_;
    std::size_t selectedMode_{0};
    std::size_t selectedNav_{0};
};

} // namespace rf
