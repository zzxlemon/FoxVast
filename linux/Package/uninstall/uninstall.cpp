#include "uninstall.hpp"
#include <iostream>
#include <cstdio>
#include "../tools.hpp"

#ifdef _WIN32
#include <windows.h>

static bool delete_directory_recursive(const std::string& path) {
    std::string search = path + "\\*";
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(search.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return false;

    do {
        std::string name = fd.cFileName;
        if (name == "." || name == "..") continue;
        std::string full = path + "\\" + name;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            delete_directory_recursive(full);
        } else {
            DeleteFileA(full.c_str());
        }
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);
    return RemoveDirectoryA(path.c_str()) != 0;
}
#else
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

static bool delete_directory_recursive(const std::string& path) {
    DIR* d = opendir(path.c_str());
    if (!d) return false;

    struct dirent* e;
    while ((e = readdir(d)) != nullptr) {
        std::string name = e->d_name;
        if (name == "." || name == "..") continue;
        std::string full = path + "/" + name;
        struct stat st;
        if (stat(full.c_str(), &st) == 0 && (st.st_mode & S_IFDIR)) {
            delete_directory_recursive(full);
        } else {
            remove(full.c_str());
        }
    }
    closedir(d);
    return rmdir(path.c_str()) == 0;
}
#endif

void uninstall_package(const std::string &package_name, const std::string &package_dir) {
    std::string base = package_dir;
    for (char &c : base) {
        if (c == '\\') c = '/';
    }
    if (!base.empty() && base.back() != '/') base += '/';

    std::string dir_path = base + package_name;
    std::string zip_path = base + package_name + ".zip";

#ifdef _WIN32
    DWORD attr = GetFileAttributesA(dir_path.c_str());
    bool dir_removed = true;
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
        dir_removed = delete_directory_recursive(dir_path);
    }
#else
    struct stat st;
    bool dir_removed = true;
    if (stat(dir_path.c_str(), &st) == 0 && (st.st_mode & S_IFDIR)) {
        dir_removed = delete_directory_recursive(dir_path);
    }
#endif

#ifdef _WIN32
    attr = GetFileAttributesA(zip_path.c_str());
    bool zip_removed = attr == INVALID_FILE_ATTRIBUTES;
    if (attr != INVALID_FILE_ATTRIBUTES) {
        zip_removed = DeleteFileA(zip_path.c_str()) != 0;
    }
#else
    struct stat zst;
    bool zip_removed = stat(zip_path.c_str(), &zst) != 0;
    if (!zip_removed) {
        zip_removed = remove(zip_path.c_str()) == 0;
    }
#endif

    if (dir_removed && zip_removed) {
        out_color(10);
        std::cout << "Package " << package_name << " uninstalled successfully." << std::endl;
        out_color(7);
    } else {
        err_color(12);
        std::cerr << "Failed to uninstall package: " << package_name << std::endl;
        err_color(7);
    }
}
