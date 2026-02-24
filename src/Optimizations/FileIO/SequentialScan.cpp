#include "SequentialScan.h"
#include "Utils/IATHook.h"

namespace SequentialScan {
    using CreateFileW_t = HANDLE(WINAPI*)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
    static CreateFileW_t OriginalCreateFileW = nullptr;

    static bool IsBSAFile(LPCWSTR a_path) {
        if (!a_path) return false;
        auto len = wcslen(a_path);
        if (len < 4) return false;

        auto ext = a_path + len - 4;
        return (_wcsicmp(ext, L".bsa") == 0 || _wcsicmp(ext, L".ba2") == 0);
    }

    static HANDLE WINAPI HookedCreateFileW(
        LPCWSTR a_fileName,
        DWORD a_desiredAccess,
        DWORD a_shareMode,
        LPSECURITY_ATTRIBUTES a_securityAttributes,
        DWORD a_creationDisposition,
        DWORD a_flagsAndAttributes,
        HANDLE a_templateFile
    ) {
        if (IsBSAFile(a_fileName) && (a_desiredAccess & GENERIC_READ)) {
            // Add sequential scan hint for BSA/BA2 files
            a_flagsAndAttributes |= FILE_FLAG_SEQUENTIAL_SCAN;
            // Remove random access flag if present
            a_flagsAndAttributes &= ~FILE_FLAG_RANDOM_ACCESS;
        }

        return OriginalCreateFileW(
            a_fileName, a_desiredAccess, a_shareMode,
            a_securityAttributes, a_creationDisposition,
            a_flagsAndAttributes, a_templateFile);
    }

    void Install() {
        auto original = IATHook::Apply("kernel32.dll", "CreateFileW",
            reinterpret_cast<void*>(&HookedCreateFileW));
        if (original) {
            OriginalCreateFileW = reinterpret_cast<CreateFileW_t>(original);
            spdlog::info("SequentialScan: CreateFileW hooked, BSA/BA2 files will use sequential scan");
        } else {
            spdlog::warn("SequentialScan: failed to hook CreateFileW");
        }
    }
}
