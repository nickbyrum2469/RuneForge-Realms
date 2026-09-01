#pragma once

#ifdef _WIN32

#include <filesystem>
#include <optional>
#include <string>

namespace rf::updater {

struct ReleaseInfo {
    std::wstring tag;
    std::wstring assetUrl;
};

class ReleaseClient {
public:
    [[nodiscard]] std::optional<ReleaseInfo> latest() const;
    [[nodiscard]] bool download(const std::wstring& url, const std::filesystem::path& destination) const;

private:
    [[nodiscard]] std::optional<std::string> getUtf8(const std::wstring& host, const std::wstring& path) const;
};

} // namespace rf::updater

#endif
