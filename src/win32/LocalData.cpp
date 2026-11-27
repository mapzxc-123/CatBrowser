#include "LocalData.h"

#include "../common/Text.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string_view>

namespace astb3ast::localdata {
namespace {

constexpr size_t kMaxHistoryEntries = 100;
constexpr const char* kSettingsFile = "AstB3ast.ini";
constexpr const char* kHistoryFile = "history.txt";
constexpr const char* kDictionaryFile = "dict.txt";

std::wstring exeDirectory() {
    wchar_t path[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return L".";
    }
    std::wstring full(path, length);
    const size_t separator = full.find_last_of(L"\\/");
    if (separator == std::wstring::npos) {
        return L".";
    }
    return full.substr(0, separator);
}

std::filesystem::path dataPath(const char* name) {
    return std::filesystem::path(exeDirectory()) / name;
}

std::vector<std::string> readLines(const char* name) {
    std::ifstream file(dataPath(name), std::ios::binary);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(std::move(line));
    }
    return lines;
}

void writeLines(const char* name, const std::vector<std::string>& lines) {
    std::ofstream file(dataPath(name), std::ios::binary | std::ios::trunc);
    for (const auto& line : lines) {
        file << line << '\n';
    }
}

std::string_view trim(std::string_view value) {
    const auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
    while (!value.empty() && isSpace(static_cast<unsigned char>(value.front()))) {
        value.remove_prefix(1);
    }
    while (!value.empty() && isSpace(static_cast<unsigned char>(value.back()))) {
        value.remove_suffix(1);
    }
    return value;
}

std::string lower(std::string_view value) {
    std::string out(value);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

} // namespace

Settings loadSettings() {
    Settings settings;
    for (const std::string& raw : readLines(kSettingsFile)) {
        const size_t equals = raw.find('=');
        if (equals == std::string::npos) {
            continue;
        }
        const std::string key = lower(trim(std::string_view(raw).substr(0, equals)));
        const std::string value = std::string(trim(std::string_view(raw).substr(equals + 1)));
        if (key == "engine") {
            settings.engine = search::engineFromName(value);
        } else if (key == "theme") {
            const std::string theme = lower(value);
            settings.darkTheme = theme != "light" && theme != "false";
        } else if (key == "downloaddir" || key == "download") {
            settings.downloadDir = text::utf8ToWide(value);
        }
    }
    return settings;
}

void saveSettings(const Settings& settings) {
    std::vector<std::string> lines;
    lines.push_back(std::string("engine=") + search::engineName(settings.engine));
    lines.push_back(std::string("theme=") + (settings.darkTheme ? "dark" : "light"));
    lines.push_back("downloaddir=" + text::wideToUtf8(settings.downloadDir));
    writeLines(kSettingsFile, lines);
}

std::vector<std::wstring> loadHistory() {
    std::vector<std::wstring> history;
    for (const std::string& line : readLines(kHistoryFile)) {
        if (trim(line).empty()) {
            continue;
        }
        const std::wstring entry = text::utf8ToWide(line);
        if (entry.empty() ||
            std::find(history.begin(), history.end(), entry) != history.end()) {
            continue;
        }
        history.push_back(entry);
        if (history.size() >= kMaxHistoryEntries) {
            break;
        }
    }
    return history;
}

std::vector<std::wstring> addHistory(const std::string& entry) {
    std::vector<std::wstring> history = loadHistory();
    const std::wstring converted = text::utf8ToWide(entry);
    if (converted.empty()) {
        return history;
    }
    history.erase(std::remove(history.begin(), history.end(), converted),
                  history.end());
    history.insert(history.begin(), converted);
    if (history.size() > kMaxHistoryEntries) {
        history.resize(kMaxHistoryEntries);
    }

    std::vector<std::string> lines;
    lines.reserve(history.size());
    for (const auto& item : history) {
        lines.push_back(text::wideToUtf8(item));
    }
    writeLines(kHistoryFile, lines);
    return history;
}

std::vector<std::pair<std::string, std::string>> loadUserDictionary() {
    std::vector<std::pair<std::string, std::string>> entries;
    for (const std::string& raw : readLines(kDictionaryFile)) {
        const std::string_view line = trim(raw);
        if (line.empty() || line.front() == '#') {
            continue;
        }
        size_t separator = line.find('\t');
        if (separator == std::string_view::npos) {
            separator = line.find('=');
        }
        if (separator == std::string_view::npos) {
            continue;
        }
        const std::string english(trim(line.substr(0, separator)));
        const std::string japanese(trim(line.substr(separator + 1)));
        if (english.empty() || japanese.empty()) {
            continue;
        }
        entries.emplace_back(english, japanese);
    }
    return entries;
}

} // namespace astb3ast::localdata
