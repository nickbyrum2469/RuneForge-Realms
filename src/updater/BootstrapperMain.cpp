#ifdef _WIN32

#include "core/Version.h"
#include "updater/ReleaseClient.h"
#include "updater/UpdateApplier.h"

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <windows.h>

namespace {

std::filesystem::path executableDirectory() {
    std::wstring buffer(32768, L'\0');
    const DWORD size = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    buffer.resize(size);
    return std::filesystem::path(buffer).parent_path();
}

std::optional<rf::Version> installedVersion(const std::filesystem::path& root) {
    std::ifstream input(root / L"runtime" / L"version.txt");
    std::string text;
    std::getline(input, text);
    return input || !text.empty() ? rf::Version::parse(text) : std::nullopt;
}

void showError(const wchar_t* message) {
    MessageBoxW(nullptr, message, L"RuneForge Realms Updater", MB_OK | MB_ICONWARNING);
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    const auto root = executableDirectory();
    rf::updater::ReleaseClient client;
    rf::updater::UpdateApplier applier(root);

    // Update failures never block a known-good installed runtime.
    if (const auto release = client.latest()) {
        const auto remote = rf::Version::parse(std::string(release->tag.begin(), release->tag.end()));
        const auto local = installedVersion(root).value_or(rf::Version{});
        if (remote && *remote > local) {
            const auto archive = root / L"RuneForgeRealms.update.zip";
            if (client.download(release->assetUrl, archive)) {
                if (!applier.expandAndSwap(archive)) {
                    showError(L"RuneForge found an update but could not apply it safely. Your existing runtime was kept.");
                }
                std::error_code ec;
                std::filesystem::remove(archive, ec);
            }
        }
    }

    if (!applier.launchCurrent()) {
        showError(L"RuneForgeRealms.exe was not found in the runtime folder. Download the latest Windows release again and extract the full folder.");
        return 1;
    }
    return 0;
}

#endif
