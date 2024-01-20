//
// RT64
//

#include "rt64_file_dialog.h"

#include <cassert>

#if defined(_WIN32)
#   include <Windows.h>
#endif

namespace RT64 {
    std::filesystem::path openFileDialog(RenderWindow window, const std::wstring &filter, bool save) {
#   if defined(_WIN32)
        HWND hwnd = window;
        assert(IsWindow(hwnd));

        std::wstring retPath;
        retPath.resize(_MAX_PATH);

        OPENFILENAMEW of = { 0 };
        const std::wstring Title = L"Select file";
        of.lStructSize = sizeof(OPENFILENAME);
        of.hInstance = GetModuleHandle(0);
        of.hwndOwner = hwnd;
        of.lpstrFilter = filter.data();
        of.lpstrCustomFilter = 0;
        of.nMaxCustFilter = 0;
        of.nFilterIndex = 0;
        of.lpstrFile = retPath.data();
        of.nMaxFile = DWORD(retPath.size());
        of.nMaxFileTitle = _MAX_FNAME + _MAX_EXT;
        of.lpstrFileTitle = 0;
        of.lpstrInitialDir = 0;
        of.lpstrTitle = Title.data();
        of.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR;
        of.lpstrDefExt = 0;
        of.nFileOffset = 0;
        of.nFileExtension = 0;
        of.lCustData = 0L;
        of.lpfnHook = 0;
        of.lpTemplateName = 0;

        bool result = false;
        if (save) {
            result = GetSaveFileNameW(&of);
        }
        else {
            of.Flags |= OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
            result = GetOpenFileNameW(&of);
        }

        DWORD dlgError = CommDlgExtendedError();
        if (dlgError != 0) {
            fprintf(stderr, "Dialog failed with error code 0x%X\n", dlgError);
        }

        if (result) {
            return retPath;
        }
        else {
            return {};
        }
#   else
        return {};
#   endif
    }

    // FileDialog

    std::filesystem::path FileDialog::getOpenFilename(RenderWindow window, const std::wstring &filter) {
        return openFileDialog(window, filter, false);
    }

    std::filesystem::path FileDialog::getSaveFilename(RenderWindow window, const std::wstring &filter) {
        return openFileDialog(window, filter, true);
    }
};