#pragma once

#ifdef _WIN32

#include <filesystem>

namespace rf::updater {

class UpdateApplier {
public:
    explicit UpdateApplier(std::filesystem::path installRoot);

    [[nodiscard]] bool expandAndSwap(const std::filesystem::path& archive) const;
    [[nodiscard]] bool launchCurrent() const;

private:
    [[nodiscard]] bool runPowerShellExpand(const std::filesystem::path& archive,
                                           const std::filesystem::path& destination) const;
    std::filesystem::path installRoot_;
};

} // namespace rf::updater

#endif
