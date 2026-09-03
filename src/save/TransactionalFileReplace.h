#pragma once

#include <filesystem>

namespace rf::save::detail {

// Replace a fully-written temporary file without deleting the current live file first. This handles
// Windows rename-over-existing semantics transactionally: move live data aside, install the complete
// replacement, and restore the previous file if installation fails. Even a rollback failure leaves
// the previous bytes recoverable at <destination>.bak.
inline bool replaceFileRollbackSafe(const std::filesystem::path& temporary,
                                    const std::filesystem::path& destination) {
    std::error_code ec;
    std::filesystem::rename(temporary, destination, ec);
    if (!ec) return true;

    ec.clear();
    if (!std::filesystem::exists(destination, ec) || ec) return false;

    const std::filesystem::path backup = destination.string() + ".bak";
    ec.clear();
    std::filesystem::remove(backup, ec);
    if (ec) return false;

    ec.clear();
    std::filesystem::rename(destination, backup, ec);
    if (ec) return false;

    ec.clear();
    std::filesystem::rename(temporary, destination, ec);
    if (ec) {
        std::error_code rollbackError;
        std::filesystem::rename(backup, destination, rollbackError);
        return false;
    }

    std::error_code cleanupError;
    std::filesystem::remove(backup, cleanupError);
    return true;
}

} // namespace rf::save::detail
