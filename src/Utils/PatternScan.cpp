#include "PatternScan.h"

namespace PatternScan {
    struct PatternByte {
        bool wildcard;
        std::uint8_t value;
    };

    static std::vector<PatternByte> ParsePattern(const char* a_pattern) {
        std::vector<PatternByte> bytes;
        const char* p = a_pattern;

        while (*p) {
            // Skip whitespace
            while (*p == ' ') ++p;
            if (!*p) break;

            if (*p == '?') {
                bytes.push_back({true, 0});
                ++p;
                if (*p == '?') ++p;  // Handle "??" as single wildcard
            } else {
                // Parse hex byte
                char high = *p++;
                char low = *p ? *p++ : '0';

                auto hexVal = [](char c) -> std::uint8_t {
                    if (c >= '0' && c <= '9') return c - '0';
                    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                    return 0;
                };

                bytes.push_back({false, static_cast<std::uint8_t>((hexVal(high) << 4) | hexVal(low))});
            }
        }

        return bytes;
    }

    static bool MatchPattern(const std::uint8_t* a_data, const std::vector<PatternByte>& a_pattern) {
        for (size_t i = 0; i < a_pattern.size(); ++i) {
            if (!a_pattern[i].wildcard && a_data[i] != a_pattern[i].value) {
                return false;
            }
        }
        return true;
    }

    std::uintptr_t Find(std::uintptr_t a_base, size_t a_size, const char* a_pattern) {
        auto pattern = ParsePattern(a_pattern);
        if (pattern.empty()) return 0;

        auto* data = reinterpret_cast<const std::uint8_t*>(a_base);
        auto scanSize = a_size - pattern.size();

        for (size_t i = 0; i < scanSize; ++i) {
            if (MatchPattern(data + i, pattern)) {
                return a_base + i;
            }
        }

        return 0;
    }

    std::uintptr_t Find(const char* a_pattern) {
        auto base = REL::Module::get().base();
        auto* dosHeader = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        auto* ntHeaders = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dosHeader->e_lfanew);

        // Scan .text section for code patterns
        auto* section = IMAGE_FIRST_SECTION(ntHeaders);
        for (WORD i = 0; i < ntHeaders->FileHeader.NumberOfSections; ++i, ++section) {
            if (std::memcmp(section->Name, ".text", 5) == 0 ||
                (section->Characteristics & IMAGE_SCN_MEM_EXECUTE)) {
                auto sectionBase = base + section->VirtualAddress;
                auto sectionSize = static_cast<size_t>(section->Misc.VirtualSize);

                auto result = Find(sectionBase, sectionSize, a_pattern);
                if (result) return result;
            }
        }

        return 0;
    }

    std::vector<std::uintptr_t> FindAll(const char* a_pattern) {
        std::vector<std::uintptr_t> results;
        auto pattern = ParsePattern(a_pattern);
        if (pattern.empty()) return results;

        auto base = REL::Module::get().base();
        auto* dosHeader = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        auto* ntHeaders = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dosHeader->e_lfanew);

        auto* section = IMAGE_FIRST_SECTION(ntHeaders);
        for (WORD i = 0; i < ntHeaders->FileHeader.NumberOfSections; ++i, ++section) {
            if (section->Characteristics & IMAGE_SCN_MEM_EXECUTE) {
                auto sectionBase = base + section->VirtualAddress;
                auto sectionSize = static_cast<size_t>(section->Misc.VirtualSize);
                auto* data = reinterpret_cast<const std::uint8_t*>(sectionBase);
                auto scanSize = sectionSize - pattern.size();

                for (size_t j = 0; j < scanSize; ++j) {
                    if (MatchPattern(data + j, pattern)) {
                        results.push_back(sectionBase + j);
                    }
                }
            }
        }

        return results;
    }

    std::uintptr_t ResolveRIP(std::uintptr_t a_address, int a_offset, int a_instructionSize) {
        auto rip = a_address + a_instructionSize;
        auto rel = *reinterpret_cast<const std::int32_t*>(a_address + a_offset);
        return rip + rel;
    }
}
