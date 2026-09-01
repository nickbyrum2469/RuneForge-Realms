#include "core/Version.h"

#include <charconv>

namespace rf {

std::string Version::toString() const {
    return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
}

std::optional<Version> Version::parse(std::string_view text) {
    if (!text.empty() && (text.front() == 'v' || text.front() == 'V')) {
        text.remove_prefix(1);
    }

    Version out{};
    int* values[] = {&out.major, &out.minor, &out.patch};
    for (int i = 0; i < 3; ++i) {
        const auto dot = text.find('.');
        const auto token = (i < 2) ? text.substr(0, dot) : text;
        if (token.empty()) return std::nullopt;

        const auto* begin = token.data();
        const auto* end = token.data() + token.size();
        const auto [ptr, ec] = std::from_chars(begin, end, *values[i]);
        if (ec != std::errc{} || ptr != end || *values[i] < 0) return std::nullopt;

        if (i < 2) {
            if (dot == std::string_view::npos) return std::nullopt;
            text.remove_prefix(dot + 1);
        }
    }
    return out;
}

} // namespace rf
