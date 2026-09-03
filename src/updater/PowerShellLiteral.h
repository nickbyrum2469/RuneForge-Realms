#pragma once

#include <string>
#include <string_view>

namespace rf::updater {

inline std::wstring powerShellSingleQuotedLiteral(std::wstring_view value) {
    std::wstring escaped;
    escaped.reserve(value.size() + 2);
    escaped.push_back(L'\'');
    for (const wchar_t ch : value) {
        if (ch == L'\'') escaped.push_back(L'\'');
        escaped.push_back(ch);
    }
    escaped.push_back(L'\'');
    return escaped;
}

} // namespace rf::updater
