//
// RT64
//

#include "rt64_file_dialog.h"

#include <cassert>

#include <nfd.h>

#if defined(__APPLE__)
#   include "apple/rt64_apple.h"
#endif

namespace RT64 {
    // FileDialog

    std::atomic<bool> FileDialog::isOpen = false;

    void FileDialog::initialize() {
        NFD_Init();
    }

    void FileDialog::finish() {
        NFD_Quit();
    }

    static std::vector<nfdnfilteritem_t> convertFilters(const std::vector<FileFilter> &filters) {
        std::vector<nfdnfilteritem_t> nfdFilters;
        for (const FileFilter &filter : filters) {
            nfdFilters.emplace_back(nfdnfilteritem_t{ filter.description.c_str(), filter.extensions.c_str() });
        }

        return nfdFilters;
    }

    void FileDialog::getDirectoryPath(const Callback &callback) {
        isOpen = true;
#ifdef __APPLE__
        apple::dispatchOnMainThread([callback]() {
#endif
        std::filesystem::path path;
        nfdnchar_t *nfdPath = nullptr;
        nfdresult_t res = NFD_PickFolderN(&nfdPath, nullptr);
        if (res == NFD_OKAY) {
            path = std::filesystem::path(nfdPath);
            NFD_FreePathN(nfdPath);
        }

        callback(path);
#ifdef __APPLE__
        });
#endif
        isOpen = false;

    }

    void FileDialog::getOpenFilename(const std::vector<FileFilter> &filters, const Callback &callback) {
        isOpen = true;
#ifdef __APPLE__
        apple::dispatchOnMainThread([filters, callback]() {
#endif
        std::filesystem::path path;
        nfdnchar_t *nfdPath = nullptr;
        std::vector<nfdnfilteritem_t> nfdFilters = convertFilters(filters);
        nfdresult_t res = NFD_OpenDialogN(&nfdPath, nfdFilters.data(), nfdFilters.size(), nullptr);
        if (res == NFD_OKAY) {
            path = std::filesystem::path(nfdPath);
            NFD_FreePathN(nfdPath);
        }

        callback(path);
#ifdef __APPLE__
        });
#endif
        isOpen = false;

    }

    void FileDialog::getSaveFilename(const std::vector<FileFilter> &filters, const Callback &callback) {
        isOpen = true;
#ifdef __APPLE__
        apple::dispatchOnMainThread([filters, callback]() {
#endif
        std::filesystem::path path;
        nfdnchar_t *nfdPath = nullptr;
        std::vector<nfdnfilteritem_t> nfdFilters = convertFilters(filters);
        nfdresult_t res = NFD_SaveDialogN(&nfdPath, nfdFilters.data(), nfdFilters.size(), nullptr, nullptr);
        if (res == NFD_OKAY) {
            path = std::filesystem::path(nfdPath);
            NFD_FreePathN(nfdPath);
        }

        callback(path);
#ifdef __APPLE__
        });
#endif
        isOpen = false;

    }
};