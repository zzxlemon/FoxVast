#include "dll_loader.hpp"
#include "../interpreter/library_manager.hpp"
#include <windows.h>
#include <string>
#include <vector>

static std::string getExeDir() {
    char buf[MAX_PATH];
    DWORD len = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return ".\\";
    std::string path(buf, len);
    size_t sep = path.find_last_of("/\\");
    if (sep != std::string::npos) return path.substr(0, sep + 1);
    return ".\\";
}

bool LoadFoxLibs(LibraryManager& libMgr) {
    std::string dir = getExeDir();

    // Look next to the exe, then in dlls/ subfolder
    const char* subdirs[] = { "", "dlls\\", nullptr };

    bool anyLoaded = false;
    for (int s = 0; subdirs[s] != nullptr; s++) {
        std::string pattern = dir + subdirs[s] + "fox.*.dll";

        WIN32_FIND_DATAA fd;
        HANDLE hFind = FindFirstFileA(pattern.c_str(), &fd);
        if (hFind == INVALID_HANDLE_VALUE) continue;

        do {
            std::string fullPath = dir + subdirs[s] + fd.cFileName;
            HMODULE hModule = LoadLibraryA(fullPath.c_str());
            if (hModule == nullptr) continue;

            auto registerFunc = reinterpret_cast<void(*)(LibraryManager*)>(
                GetProcAddress(hModule, "register_fox_library"));
            if (registerFunc == nullptr) {
                FreeLibrary(hModule);
                continue;
            }

            registerFunc(&libMgr);
            anyLoaded = true;
        } while (FindNextFileA(hFind, &fd));

        FindClose(hFind);
    }
    return anyLoaded;
}
