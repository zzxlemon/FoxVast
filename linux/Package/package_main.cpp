#include <cstdio>
#include <cstring>
#include <string>
#include <iostream>
#include <sys/stat.h>
#include "version.hpp"
#include "tools.hpp"
#include "install/install.hpp"
#include "uninstall/uninstall.hpp"

std::string package_name = "";

std::string package_dir = "./packages"; // Default package directory
std::string package_file = "./packages/list.txt"; // Default package list file

void show_help_list(){
    std::printf("foxpkg %s\n", FOXPKG_VERSION);
    std::printf("Usage:\n");
    std::printf("  foxpkg install <package_name>   Install a package\n");
    std::printf("  foxpkg uninstall <package_name> Uninstall a package\n");
    std::printf("  foxpkg update <package_name>    Update a package\n");
    std::printf("  foxpkg list                     List installed packages\n");
    std::printf("  foxpkg path <path>              Set package directory path\n");
    std::printf("  foxpkg file <file>              Set package list file path\n");
    std::printf("  foxpkg name <name>              Set package name for install/uninstall/update\n");
    std::printf("  foxpkg help                     Show this help message\n");
}

int main(int argc, char** argv) {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    if (argc == 1) {
        show_help_list();
        return 0;
    }

    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "-version") == 0 || std::strcmp(argv[i], "-v") == 0) {
            std::printf("foxpkg %s\n", FOXPKG_VERSION);
            return 0;
        }
        else if (std::strcmp(argv[i], "install") == 0 || std::strcmp(argv[i], "-i") == 0) {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "Error: 'install' requires a package name\n");
                return 1;
            }
            package_name = argv[i + 1];
            i++;
            install_package(package_name, package_dir);
            record_install_package_in_file(package_name, package_dir);
        }
        else if (std::strcmp(argv[i], "uninstall") == 0 || std::strcmp(argv[i], "-u") == 0) {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "Error: 'uninstall' requires a package name\n");
                return 1;
            }
            package_name = argv[i + 1];
            i++;
            uninstall_package(package_name, package_dir);
        }
        else if (std::strcmp(argv[i], "update") == 0 || std::strcmp(argv[i], "-up") == 0) {
            
        }
        else if (std::strcmp(argv[i], "list") == 0 || std::strcmp(argv[i], "-l") == 0) {
            
        }
        else if (std::strcmp(argv[i], "path") == 0 || std::strcmp(argv[i], "-p") == 0) {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "Error: '-p' requires a path argument\n");
                return 1;
            }
            package_dir = argv[i + 1];
            i++;
            {
                struct stat st;
                if (stat(package_dir.c_str(), &st) != 0) {
                    std::cout << "Package:" << package_dir << " Path does not exist or is invalid.\n";
                    return 1;
                }
            }
        }
        else if (std::strcmp(argv[i], "name") == 0 || std::strcmp(argv[i], "-n") == 0) {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "Error: '-n' requires a package name argument\n");
                return 1;
            }
            package_name = argv[i + 1];
            i++;
        }
        else if (std::strcmp(argv[i], "from") == 0 || std::strcmp(argv[i], "-fm") == 0) {
            break; 
            if (i + 1 >= argc){
                std::fprintf(stderr, "Error: '-fm' requires a URL parameter\n");
                return 1;
            }
            std::string url = argv[i + 1];
            i++;
            if (url.find("http://") != 0 && url.find("https://") != 0) {
                std::fprintf(stderr, "Error: URL must start with 'http://' or 'https://'\n");
                return 1;
            }
            aaa = url;
        }
        else if (std::strcmp(argv[i], "update") == 0 || std::strcmp(argv[i], "-up") == 0) {
            
        }
        else if (std::strcmp(argv[i], "file") == 0 || std::strcmp(argv[i], "-f") == 0) {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "Error: '-f' requires a file argument\n");
                return 1;
            }
            package_file = argv[i + 1];
            i++;
        }
        else if (std::strcmp(argv[i], "name") == 0 || std::strcmp(argv[i], "-n") == 0) {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "Error: '-n' requires a package name argument\n");
                return 1;
            }
            package_name = argv[i + 1];
            i++;
        }
        else if (std::strcmp(argv[i], "help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            show_help_list();
            return 0;
        }
        else {
            std::fprintf(stderr, "Error: unknown parameter '%s'\n", argv[i]);
            return 1;
        }
    } 
    return 0;
}
