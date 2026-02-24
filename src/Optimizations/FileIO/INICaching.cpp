#include "INICaching.h"
#include "Utils/IATHook.h"

#include <shared_mutex>

namespace INICaching {
    // INI file cache: filename -> (section -> (key -> value))
    using IniData = std::unordered_map<std::string, std::unordered_map<std::string, std::string>>;
    static std::unordered_map<std::string, IniData> g_cache;
    static std::shared_mutex g_cacheMutex;

    // Original function pointers
    using GetPrivateProfileStringA_t = DWORD(WINAPI*)(LPCSTR, LPCSTR, LPCSTR, LPSTR, DWORD, LPCSTR);
    using GetPrivateProfileStringW_t = DWORD(WINAPI*)(LPCWSTR, LPCWSTR, LPCWSTR, LPWSTR, DWORD, LPCWSTR);
    using GetPrivateProfileIntA_t = UINT(WINAPI*)(LPCSTR, LPCSTR, INT, LPCSTR);
    using GetPrivateProfileSectionA_t = DWORD(WINAPI*)(LPCSTR, LPSTR, DWORD, LPCSTR);
    using WritePrivateProfileStringA_t = BOOL(WINAPI*)(LPCSTR, LPCSTR, LPCSTR, LPCSTR);

    static GetPrivateProfileStringA_t OriginalGetPrivateProfileStringA = nullptr;
    static GetPrivateProfileStringW_t OriginalGetPrivateProfileStringW = nullptr;
    static GetPrivateProfileIntA_t OriginalGetPrivateProfileIntA = nullptr;
    static GetPrivateProfileSectionA_t OriginalGetPrivateProfileSectionA = nullptr;
    static WritePrivateProfileStringA_t OriginalWritePrivateProfileStringA = nullptr;

    static std::string ToLower(std::string_view a_str) {
        std::string result(a_str);
        std::transform(result.begin(), result.end(), result.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return result;
    }

    static bool IsIniFile(const char* a_path) {
        if (!a_path) return false;
        auto len = strlen(a_path);
        return len > 4 && _stricmp(a_path + len - 4, ".ini") == 0;
    }

    static bool LoadIniToCache(const char* a_filePath) {
        auto key = ToLower(a_filePath);

        {
            std::shared_lock lock(g_cacheMutex);
            if (g_cache.contains(key)) {
                return true;
            }
        }

        // Read the INI file and parse it
        std::ifstream file(a_filePath);
        if (!file.is_open()) {
            return false;
        }

        IniData data;
        std::string currentSection;
        std::string line;

        while (std::getline(file, line)) {
            // Trim whitespace
            auto start = line.find_first_not_of(" \t\r\n");
            if (start == std::string::npos) continue;
            line = line.substr(start);

            // Skip comments
            if (line[0] == ';' || line[0] == '#') continue;

            // Section header
            if (line[0] == '[') {
                auto end = line.find(']');
                if (end != std::string::npos) {
                    currentSection = ToLower(line.substr(1, end - 1));
                }
                continue;
            }

            // Key=Value
            auto eq = line.find('=');
            if (eq != std::string::npos) {
                auto rawKey = line.substr(0, eq);
                auto rawValue = line.substr(eq + 1);

                // Trim key
                auto kEnd = rawKey.find_last_not_of(" \t");
                if (kEnd != std::string::npos) rawKey = rawKey.substr(0, kEnd + 1);

                // Trim value (leading whitespace only, preserve trailing for INI compat)
                auto vStart = rawValue.find_first_not_of(" \t");
                if (vStart != std::string::npos) rawValue = rawValue.substr(vStart);
                else rawValue.clear();

                // Strip inline comments
                auto commentPos = rawValue.find(';');
                if (commentPos != std::string::npos) {
                    rawValue = rawValue.substr(0, commentPos);
                    auto vEnd = rawValue.find_last_not_of(" \t");
                    if (vEnd != std::string::npos) rawValue = rawValue.substr(0, vEnd + 1);
                    else rawValue.clear();
                }

                data[currentSection][ToLower(rawKey)] = rawValue;
            }
        }

        std::unique_lock lock(g_cacheMutex);
        g_cache[key] = std::move(data);

        spdlog::trace("INICaching: cached {}", a_filePath);
        return true;
    }

    static DWORD WINAPI HookedGetPrivateProfileStringA(
        LPCSTR a_section, LPCSTR a_key, LPCSTR a_default,
        LPSTR a_buffer, DWORD a_size, LPCSTR a_filePath
    ) {
        // Only cache .ini files
        if (!a_filePath || !IsIniFile(a_filePath) || !a_section || !a_key) {
            return OriginalGetPrivateProfileStringA(a_section, a_key, a_default, a_buffer, a_size, a_filePath);
        }

        if (!LoadIniToCache(a_filePath)) {
            return OriginalGetPrivateProfileStringA(a_section, a_key, a_default, a_buffer, a_size, a_filePath);
        }

        auto fileKey = ToLower(a_filePath);
        auto sectionKey = ToLower(a_section);
        auto keyKey = ToLower(a_key);

        {
            std::shared_lock lock(g_cacheMutex);
            auto fileIt = g_cache.find(fileKey);
            if (fileIt != g_cache.end()) {
                auto secIt = fileIt->second.find(sectionKey);
                if (secIt != fileIt->second.end()) {
                    auto keyIt = secIt->second.find(keyKey);
                    if (keyIt != secIt->second.end()) {
                        auto& value = keyIt->second;
                        auto copyLen = std::min(static_cast<DWORD>(value.size()), a_size - 1);
                        memcpy(a_buffer, value.c_str(), copyLen);
                        a_buffer[copyLen] = '\0';
                        return copyLen;
                    }
                }
            }
        }

        // Key not found in cache, return default
        if (a_default) {
            auto defLen = strlen(a_default);
            auto copyLen = std::min(static_cast<DWORD>(defLen), a_size - 1);
            memcpy(a_buffer, a_default, copyLen);
            a_buffer[copyLen] = '\0';
            return static_cast<DWORD>(copyLen);
        }

        if (a_size > 0) a_buffer[0] = '\0';
        return 0;
    }

    static UINT WINAPI HookedGetPrivateProfileIntA(
        LPCSTR a_section, LPCSTR a_key, INT a_default, LPCSTR a_filePath
    ) {
        if (!a_filePath || !IsIniFile(a_filePath) || !a_section || !a_key) {
            return OriginalGetPrivateProfileIntA(a_section, a_key, a_default, a_filePath);
        }

        char buffer[64];
        auto len = HookedGetPrivateProfileStringA(a_section, a_key, nullptr, buffer, sizeof(buffer), a_filePath);
        if (len == 0) {
            return static_cast<UINT>(a_default);
        }

        try {
            return static_cast<UINT>(std::stoi(buffer));
        } catch (...) {
            return static_cast<UINT>(a_default);
        }
    }

    static BOOL WINAPI HookedWritePrivateProfileStringA(
        LPCSTR a_section, LPCSTR a_key, LPCSTR a_value, LPCSTR a_filePath
    ) {
        // Invalidate cache on write
        if (a_filePath && IsIniFile(a_filePath)) {
            auto fileKey = ToLower(a_filePath);
            std::unique_lock lock(g_cacheMutex);
            g_cache.erase(fileKey);
        }

        return OriginalWritePrivateProfileStringA(a_section, a_key, a_value, a_filePath);
    }

    void Install() {
        auto orig = IATHook::Apply("kernel32.dll", "GetPrivateProfileStringA",
            reinterpret_cast<void*>(&HookedGetPrivateProfileStringA));
        if (orig) {
            OriginalGetPrivateProfileStringA = reinterpret_cast<GetPrivateProfileStringA_t>(orig);
        }

        orig = IATHook::Apply("kernel32.dll", "GetPrivateProfileIntA",
            reinterpret_cast<void*>(&HookedGetPrivateProfileIntA));
        if (orig) {
            OriginalGetPrivateProfileIntA = reinterpret_cast<GetPrivateProfileIntA_t>(orig);
        }

        orig = IATHook::Apply("kernel32.dll", "WritePrivateProfileStringA",
            reinterpret_cast<void*>(&HookedWritePrivateProfileStringA));
        if (orig) {
            OriginalWritePrivateProfileStringA = reinterpret_cast<WritePrivateProfileStringA_t>(orig);
        }

        if (OriginalGetPrivateProfileStringA) {
            spdlog::info("INICaching: GetPrivateProfileString* hooked, INI reads will be cached");
        } else {
            spdlog::warn("INICaching: failed to hook GetPrivateProfileStringA");
        }
    }
}
