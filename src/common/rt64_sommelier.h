//
// RT64
//

#pragma once

#if defined(_WIN32)

#include <Windows.h>

namespace RT64 {
    struct Sommelier {
        static bool detectWine() {
            static bool wineCheckPerformed = false;
            static bool isWine = false;
            if (!wineCheckPerformed) {
                wineCheckPerformed = true;

                HMODULE dllHandle = GetModuleHandle("ntdll.dll");
                if (dllHandle != NULL) {
                    isWine = GetProcAddress(dllHandle, "wine_get_version") != NULL;
                }
                else {
                    isWine = false;
                }
            }

            return isWine;
        }
    };
};

#endif