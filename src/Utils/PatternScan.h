#pragma once

namespace PatternScan {
    // Scans the main module (SkyrimSE.exe) for a byte pattern.
    // Pattern format: "48 8B 05 ?? ?? ?? ?? 48 85 C0" where ?? is a wildcard byte.
    // Returns the address of the first match, or 0 if not found.
    std::uintptr_t Find(const char* a_pattern);

    // Scans a specific memory range
    std::uintptr_t Find(std::uintptr_t a_base, size_t a_size, const char* a_pattern);

    // Find all matches
    std::vector<std::uintptr_t> FindAll(const char* a_pattern);

    // Resolve a RIP-relative address at offset within the found pattern
    // Useful for patterns like "E8 ?? ?? ?? ??" (call) or "48 8D 0D ?? ?? ?? ??" (lea)
    std::uintptr_t ResolveRIP(std::uintptr_t a_address, int a_offset, int a_instructionSize);
}
