#include "IATHook.h"

#include <Psapi.h>
#pragma comment(lib, "psapi.lib")

namespace IATHook {
    void* Apply(HMODULE a_module, const char* a_importModule, const char* a_functionName, void* a_newFunction) {
        if (!a_module || !a_importModule || !a_functionName || !a_newFunction) {
            spdlog::error("IATHook::Apply - null parameter");
            return nullptr;
        }

        auto dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(a_module);
        if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
            spdlog::error("IATHook::Apply - invalid DOS header");
            return nullptr;
        }

        auto ntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(
            reinterpret_cast<std::uint8_t*>(a_module) + dosHeader->e_lfanew);
        if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
            spdlog::error("IATHook::Apply - invalid NT header");
            return nullptr;
        }

        auto& importDir = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (importDir.Size == 0) {
            return nullptr;
        }

        auto importDesc = reinterpret_cast<PIMAGE_IMPORT_DESCRIPTOR>(
            reinterpret_cast<std::uint8_t*>(a_module) + importDir.VirtualAddress);

        for (; importDesc->Name != 0; ++importDesc) {
            auto moduleName = reinterpret_cast<const char*>(
                reinterpret_cast<std::uint8_t*>(a_module) + importDesc->Name);

            if (_stricmp(moduleName, a_importModule) != 0) {
                continue;
            }

            auto thunk = reinterpret_cast<PIMAGE_THUNK_DATA>(
                reinterpret_cast<std::uint8_t*>(a_module) + importDesc->FirstThunk);
            auto origThunk = reinterpret_cast<PIMAGE_THUNK_DATA>(
                reinterpret_cast<std::uint8_t*>(a_module) + importDesc->OriginalFirstThunk);

            for (; origThunk->u1.AddressOfData != 0; ++origThunk, ++thunk) {
                if (IMAGE_SNAP_BY_ORDINAL(origThunk->u1.Ordinal)) {
                    continue;
                }

                auto importByName = reinterpret_cast<PIMAGE_IMPORT_BY_NAME>(
                    reinterpret_cast<std::uint8_t*>(a_module) + origThunk->u1.AddressOfData);

                if (strcmp(importByName->Name, a_functionName) != 0) {
                    continue;
                }

                // Found the function - patch it
                void* original = reinterpret_cast<void*>(thunk->u1.Function);

                DWORD oldProtect;
                if (!VirtualProtect(&thunk->u1.Function, sizeof(thunk->u1.Function), PAGE_READWRITE, &oldProtect)) {
                    spdlog::error("IATHook::Apply - VirtualProtect failed for {}", a_functionName);
                    return nullptr;
                }

                thunk->u1.Function = reinterpret_cast<ULONG_PTR>(a_newFunction);

                VirtualProtect(&thunk->u1.Function, sizeof(thunk->u1.Function), oldProtect, &oldProtect);

                spdlog::info("IATHook: patched {}!{} in module", a_importModule, a_functionName);
                return original;
            }
        }

        return nullptr;
    }

    void* Apply(const char* a_importModule, const char* a_functionName, void* a_newFunction) {
        return Apply(GetModuleHandleA(nullptr), a_importModule, a_functionName, a_newFunction);
    }

    std::vector<void*> ApplyToAll(const char* a_importModule, const char* a_functionName, void* a_newFunction) {
        std::vector<void*> originals;
        HMODULE modules[1024];
        DWORD needed;

        if (!EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &needed)) {
            spdlog::error("IATHook::ApplyToAll - EnumProcessModules failed");
            return originals;
        }

        auto count = needed / sizeof(HMODULE);
        for (DWORD i = 0; i < count; ++i) {
            if (auto original = Apply(modules[i], a_importModule, a_functionName, a_newFunction)) {
                originals.push_back(original);
            }
        }

        spdlog::info("IATHook::ApplyToAll: patched {} in {} modules", a_functionName, originals.size());
        return originals;
    }
}
