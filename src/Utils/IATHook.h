#pragma once

namespace IATHook {
    // Patches the Import Address Table of the given module, replacing the imported function
    // a_originalFunction with a_newFunction. Returns the original function pointer, or nullptr on failure.
    void* Apply(HMODULE a_module, const char* a_importModule, const char* a_functionName, void* a_newFunction);

    // Convenience: patches the main executable module
    void* Apply(const char* a_importModule, const char* a_functionName, void* a_newFunction);

    // Patches ALL loaded modules' IATs for the given import
    std::vector<void*> ApplyToAll(const char* a_importModule, const char* a_functionName, void* a_newFunction);
}
