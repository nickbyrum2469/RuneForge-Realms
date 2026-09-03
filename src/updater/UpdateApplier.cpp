#ifdef _WIN32

#include "updater/UpdateApplier.h"
#include "updater/PowerShellLiteral.h"

#include <windows.h>
#include <shellapi.h>

#include <string>

namespace rf::updater {

UpdateApplier::UpdateApplier(std::filesystem::path installRoot) : installRoot_(std::move(installRoot)) {}

bool UpdateApplier::runPowerShellExpand(const std::filesystem::path& archive,
                                        const std::filesystem::path& destination) const {
    const std::wstring archiveLiteral = powerShellSingleQuotedLiteral(archive.wstring());
    const std::wstring destinationLiteral = powerShellSingleQuotedLiteral(destination.wstring());
    std::wstring command = L"powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -Command \"Expand-Archive -LiteralPath " +
                           archiveLiteral + L" -DestinationPath " + destinationLiteral + L" -Force\"";
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                        installRoot_.c_str(), &si, &pi)) return false;
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return exitCode == 0;
}

bool UpdateApplier::expandAndSwap(const std::filesystem::path& archive) const {
    const auto stage = installRoot_ / L".rf_update";
    const auto previous = installRoot_ / L"runtime.previous";
    const auto runtime = installRoot_ / L"runtime";
    std::error_code ec;
    std::filesystem::remove_all(stage, ec);
    std::filesystem::create_directories(stage, ec);
    if (ec || !runPowerShellExpand(archive, stage)) return false;

    const auto stagedRuntime = stage / L"RuneForgeRealms" / L"runtime";
    if (!std::filesystem::exists(stagedRuntime / L"RuneForgeRealms.exe")) return false;

    std::filesystem::remove_all(previous, ec);
    if (std::filesystem::exists(runtime)) {
        std::filesystem::rename(runtime, previous, ec);
        if (ec) return false;
    }
    std::filesystem::rename(stagedRuntime, runtime, ec);
    if (ec) {
        if (std::filesystem::exists(previous)) {
            std::error_code rollbackError;
            std::filesystem::rename(previous, runtime, rollbackError);
        }
        return false;
    }
    std::filesystem::remove_all(stage, ec);
    return true;
}

bool UpdateApplier::launchCurrent() const {
    const auto exe = installRoot_ / L"runtime" / L"RuneForgeRealms.exe";
    if (!std::filesystem::exists(exe)) return false;
    const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", exe.c_str(), nullptr,
                                                               (installRoot_ / L"runtime").c_str(), SW_SHOWNORMAL));
    return result > 32;
}

} // namespace rf::updater

#endif
