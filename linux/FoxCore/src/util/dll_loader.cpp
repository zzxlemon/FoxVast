#include "dll_loader.hpp"
#include "../interpreter/library_manager.hpp"
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>

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

#else // POSIX (Linux)

#include <dlfcn.h>
#include <dirent.h>
#include <unistd.h>
#include <climits>
#include <cerrno>

static std::string getExeDir() {
    char buf[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) return "./";
    buf[n] = '\0';
    std::string path(buf);
    size_t sep = path.find_last_of('/');
    if (sep != std::string::npos) return path.substr(0, sep + 1);
    return "./";
}

bool LoadFoxLibs(LibraryManager& libMgr) {
    std::string dir = getExeDir();

    // Look next to the exe, then in dlls/ subfolder
    const char* subdirs[] = { "", "dlls/", nullptr };

    bool anyLoaded = false;
    for (int s = 0; subdirs[s] != nullptr; s++) {
        DIR* d = opendir((dir + subdirs[s]).c_str());
        if (!d) continue;

        struct dirent* e;
        while ((e = readdir(d)) != nullptr) {
            std::string name = e->d_name;
            if (name.rfind("fox.", 0) != 0) continue;
            if (name.size() < 6 || name.compare(name.size() - 3, 3, ".so") != 0) continue;

            std::string fullPath = dir + subdirs[s] + name;
            void* h = dlopen(fullPath.c_str(), RTLD_NOW | RTLD_GLOBAL);
            if (!h) continue;

            auto registerFunc = reinterpret_cast<void(*)(LibraryManager*)>(
                dlsym(h, "register_fox_library"));
            if (!registerFunc) {
                dlclose(h);
                continue;
            }

            registerFunc(&libMgr);
            anyLoaded = true;
        }
        closedir(d);
    }
    return anyLoaded;
}

#endif
