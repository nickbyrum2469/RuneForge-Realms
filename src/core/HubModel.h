#pragma once

#include <cstddef>

namespace rf {

class HubModel {
public:
    [[nodiscard]] bool hasSave() const noexcept { return hasSave_; }
    [[nodiscard]] std::size_t selectedNavIndex() const noexcept { return selectedNav_; }

    void setHasSave(bool value) noexcept { hasSave_ = value; }
    void selectNav(std::size_t index) noexcept;

private:
    bool hasSave_{false};
    std::size_t selectedNav_{0};
};

} // namespace rf
