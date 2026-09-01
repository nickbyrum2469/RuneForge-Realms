#ifdef _WIN32

#include "updater/ReleaseClient.h"

#include <fstream>
#include <string_view>
#include <vector>
#include <winhttp.h>

namespace rf::updater {
namespace {

std::optional<std::string> jsonString(std::string_view json, std::string_view key) {
    const std::string needle = "\"" + std::string(key) + "\"";
    auto pos = json.find(needle);
    if (pos == std::string_view::npos) return std::nullopt;
    pos = json.find(':', pos + needle.size());
    if (pos == std::string_view::npos) return std::nullopt;
    pos = json.find('"', pos + 1);
    if (pos == std::string_view::npos) return std::nullopt;
    const auto end = json.find('"', pos + 1);
    if (end == std::string_view::npos) return std::nullopt;
    return std::string(json.substr(pos + 1, end - pos - 1));
}

std::wstring widen(std::string_view value) { return std::wstring(value.begin(), value.end()); }

bool parseHttpsUrl(const std::wstring& url, std::wstring& host, std::wstring& path) {
    constexpr std::wstring_view prefix = L"https://";
    if (!url.starts_with(prefix)) return false;
    const auto hostStart = prefix.size();
    const auto slash = url.find(L'/', hostStart);
    if (slash == std::wstring::npos) {
        host = url.substr(hostStart);
        path = L"/";
    } else {
        host = url.substr(hostStart, slash - hostStart);
        path = url.substr(slash);
    }
    return !host.empty();
}

} // namespace

std::optional<std::string> ReleaseClient::getUtf8(const std::wstring& host, const std::wstring& path) const {
    HINTERNET session = WinHttpOpen(L"RuneForgeBootstrap/0.1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) return std::nullopt;
    HINTERNET connect = WinHttpConnect(session, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!connect) { WinHttpCloseHandle(session); return std::nullopt; }
    HINTERNET request = WinHttpOpenRequest(connect, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!request) { WinHttpCloseHandle(connect); WinHttpCloseHandle(session); return std::nullopt; }

    const wchar_t headers[] = L"Accept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2022-11-28\r\n";
    bool ok = WinHttpSendRequest(request, headers, static_cast<DWORD>(-1L), WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
              WinHttpReceiveResponse(request, nullptr);
    std::string result;
    if (ok) {
        DWORD available = 0;
        while (WinHttpQueryDataAvailable(request, &available) && available > 0) {
            std::vector<char> buffer(available);
            DWORD read = 0;
            if (!WinHttpReadData(request, buffer.data(), available, &read)) { ok = false; break; }
            result.append(buffer.data(), read);
        }
    }
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return ok ? std::optional<std::string>(std::move(result)) : std::nullopt;
}

std::optional<ReleaseInfo> ReleaseClient::latest() const {
    const auto body = getUtf8(L"api.github.com", L"/repos/nickbyrum2469/RuneForge-Realms/releases/latest");
    if (!body) return std::nullopt;
    const auto tag = jsonString(*body, "tag_name");
    if (!tag) return std::nullopt;
    ReleaseInfo info;
    info.tag = widen(*tag);
    info.assetUrl = L"https://github.com/nickbyrum2469/RuneForge-Realms/releases/download/" + info.tag +
                    L"/RuneForgeRealms-Windows-x64.zip";
    return info;
}

bool ReleaseClient::download(const std::wstring& url, const std::filesystem::path& destination) const {
    std::wstring current = url;
    for (int redirect = 0; redirect < 8; ++redirect) {
        std::wstring host, path;
        if (!parseHttpsUrl(current, host, path)) return false;
        HINTERNET session = WinHttpOpen(L"RuneForgeBootstrap/0.1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!session) return false;
        HINTERNET connect = WinHttpConnect(session, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
        const DWORD flags = WINHTTP_FLAG_SECURE;
        HINTERNET request = connect ? WinHttpOpenRequest(connect, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                                         WINHTTP_DEFAULT_ACCEPT_TYPES, flags) : nullptr;
        bool ok = request && WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                                WINHTTP_NO_REQUEST_DATA, 0, 0, 0) && WinHttpReceiveResponse(request, nullptr);
        DWORD status = 0, statusSize = sizeof(status);
        if (ok) WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                    WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);
        if (ok && status >= 300 && status < 400) {
            wchar_t location[4096]{};
            DWORD locationSize = sizeof(location);
            if (WinHttpQueryHeaders(request, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX,
                                    location, &locationSize, WINHTTP_NO_HEADER_INDEX)) {
                current = location;
                WinHttpCloseHandle(request); WinHttpCloseHandle(connect); WinHttpCloseHandle(session);
                continue;
            }
        }
        if (!ok || status != 200) {
            if (request) WinHttpCloseHandle(request);
            if (connect) WinHttpCloseHandle(connect);
            WinHttpCloseHandle(session);
            return false;
        }

        std::ofstream out(destination, std::ios::binary | std::ios::trunc);
        DWORD available = 0;
        while (out && WinHttpQueryDataAvailable(request, &available) && available > 0) {
            std::vector<char> buffer(available);
            DWORD read = 0;
            if (!WinHttpReadData(request, buffer.data(), available, &read)) { ok = false; break; }
            out.write(buffer.data(), read);
        }
        out.close();
        WinHttpCloseHandle(request); WinHttpCloseHandle(connect); WinHttpCloseHandle(session);
        return ok && std::filesystem::exists(destination) && std::filesystem::file_size(destination) > 0;
    }
    return false;
}

} // namespace rf::updater

#endif
