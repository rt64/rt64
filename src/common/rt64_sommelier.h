//
// RT64
//

#pragma once

#if defined(_WIN32)

namespace RT64 {
    struct Sommelier {
        static bool detectWine() {
            HMODULE dllHandle = GetModuleHandle("ntdll.dll");
            if (dllHandle != NULL) {
                return GetProcAddress(dllHandle, "wine_get_version") != NULL;
            }
            else {
                return false;
            }
        }
    };
};

#endif