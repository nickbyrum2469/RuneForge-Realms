#pragma once

#include <compare>
#include <optional>
#include <string>
#include <string_view>

namespace rf {

struct Version {
    int major{};
    int minor{};
    int patch{};

    auto operator<=>(const Version&) const = default;

    [[nodiscard]] std::string toString() const;
    [[nodiscard]] static std::optional<Version> parse(std::string_view text);
};

inline constexpr std::string_view kProductName = "RuneForge Realms";
inline constexpr std::string_view kCurrentVersion = "0.1.0";

} // namespace rf
