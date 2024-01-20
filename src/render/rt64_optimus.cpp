//
// RT64
//

#ifdef _WIN32

#include <Windows.h>

extern "C" {
    // Exporting this flag indicates to the NVIDIA driver that it should prefer using the high performance device.
    _declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
}

#endif