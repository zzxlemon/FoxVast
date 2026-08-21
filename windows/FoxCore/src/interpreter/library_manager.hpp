#pragma once
#include "../frontend/value.hpp"
#include "../frontend/function.hpp"
#include "interpreter.hpp"
#include "../util/common.hpp"
#include <unordered_map>
#include <string>
#include <functional>
#include <vector>
#include <iostream>
#include <fstream>
#include <unordered_set>
#include <cstdio>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define access _access
#else
#include <unistd.h>
#endif

inline bool fileExists(const std::string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

class LibraryManager {
public:
    static LibraryManager& getInstance() {
        static LibraryManager instance;
        return instance;
    }

    void registerSystemFunction(const std::string& libName,
        const std::string& funcName,
        std::function<Value(const std::vector<Value>&)> func) {
        systemLibraries[libName][funcName] = func;
    }

    void registerLibrary(const std::string& libName) {
        if (systemLibraries.find(libName) == systemLibraries.end()) {
            systemLibraries[libName] = {};
        }
    }

    void registerLibraryName(const std::string& internalName, const std::string& externalPath) {
        m_externalPaths[externalPath] = internalName;
        m_hiddenInternalNames.insert(internalName);
    }

    bool isSystemLibrary(const std::string& libName) {
        if (m_externalPaths.find(libName) != m_externalPaths.end()) return true;
        if (systemLibraries.find(libName) != systemLibraries.end()) {
            return m_hiddenInternalNames.find(libName) == m_hiddenInternalNames.end();
        }
        return false;
    }

    std::string resolveExternalPath(const std::string& name) const {
        auto it = m_externalPaths.find(name);
        if (it != m_externalPaths.end()) return it->second;
        return name;
    }

    bool isExternalPath(const std::string& name) const {
        return m_externalPaths.find(name) != m_externalPaths.end();
    }

    bool hasLibrary(const std::string& libName) const {
        return systemLibraries.find(libName) != systemLibraries.end()
            || externalLibraries.find(libName) != externalLibraries.end();
    }

    Value callSystemFunction(const std::string& libName,
        const std::string& funcName,
        const std::vector<Value>& args) {
        auto libIt = systemLibraries.find(libName);
        if (libIt == systemLibraries.end()) {
            throw std::runtime_error("Library not found: " + libName);
        }

        auto funcIt = libIt->second.find(funcName);
        if (funcIt == libIt->second.end()) {
            throw std::runtime_error("Function not found in library " + libName + ": " + funcName);
        }

        return funcIt->second(args);
    }

    void addExternalLibrary(const std::string& libName, const std::string& libPath) {
        externalLibraryPaths[libName] = libPath;
        externalLibsToLoad[libName] = true;
    }

    void addExternalLibraryFunction(const std::string& libName,
        const std::string& funcName,
        std::function<Value(const std::vector<Value>&)> func) {
        externalLibraries[libName][funcName] = func;
        loadedExternalLibs.insert(libName);
    }

    void loadExternalLibrary(const std::string& libName) {
        if (loadedExternalLibs.find(libName) != loadedExternalLibs.end()) {
            return;
        }

        if (externalLibraries.find(libName) != externalLibraries.end()) {
            loadedExternalLibs.insert(libName);
            return;
        }

        std::vector<std::string> searchPaths = {
            "C:\\FoxLibs\\",
            "C:\\Program Files\\FoxVast\\Libs\\",
            ".\\libs\\",
            ".\\"
        };

        for (const auto& basePath : searchPaths) {
            std::string libFilePath = basePath + libName + ".foxlib";
            if (fileExists(libFilePath)) {
                loadLibraryFromFile(libName, libFilePath);
                loadedExternalLibs.insert(libName);
                return;
            }
        }

        auto pathIt = externalLibraryPaths.find(libName);
        if (pathIt != externalLibraryPaths.end() && fileExists(pathIt->second)) {
            loadLibraryFromFile(libName, pathIt->second);
            loadedExternalLibs.insert(libName);
            return;
        }

        throw std::runtime_error("Cannot find external library: " + libName);
    }

    bool isLibraryAvailable(const std::string& libName) {
        if (isSystemLibrary(libName)) return true;
        if (loadedExternalLibs.find(libName) != loadedExternalLibs.end()) return true;
        if (externalLibraries.find(libName) != externalLibraries.end()) return true;

        std::vector<std::string> searchPaths = {
            "C:\\FoxLibs\\",
            "C:\\Program Files\\FoxVast\\Libs\\",
            ".\\libs\\",
            ".\\"
        };
        for (const auto& basePath : searchPaths) {
            std::string libFilePath = basePath + libName + ".foxlib";
            if (fileExists(libFilePath)) return true;
        }
        auto pathIt = externalLibraryPaths.find(libName);
        if (pathIt != externalLibraryPaths.end()) {
            return fileExists(pathIt->second);
        }
        return false;
    }

    void markImported(const std::string& libName, const std::string& alias = "") {
        m_importedLibraries.insert(libName);
        if (!alias.empty()) {
            m_aliasMap[alias] = libName;
        }
    }

    bool isImported(const std::string& libName) const {
        return m_importedLibraries.find(libName) != m_importedLibraries.end();
    }

    std::string resolveAlias(const std::string& name) const {
        auto it = m_aliasMap.find(name);
        if (it != m_aliasMap.end()) return it->second;
        auto it2 = m_externalPaths.find(name);
        if (it2 != m_externalPaths.end()) return it2->second;
        return name;
    }

    bool isFlatNameBlockedByImport(const std::string& flatName) const {
        for (auto it = systemLibraries.begin(); it != systemLibraries.end(); ++it) {
            const auto& libName = it->first;
            const auto& funcMap = it->second;
            if (funcMap.find(flatName) != funcMap.end()) {
                if (m_importedLibraries.find(libName) != m_importedLibraries.end()) {
                    return true;
                }
            }
        }
        return false;
    }

    // Get all import alias mappings for serialization
    const std::unordered_map<std::string, std::string>& getAliasMap() const {
        return m_aliasMap;
    }

    std::string getBlockedLibName(const std::string& flatName) const {
        for (auto it = systemLibraries.begin(); it != systemLibraries.end(); ++it) {
            const auto& libName = it->first;
            const auto& funcMap = it->second;
            if (funcMap.find(flatName) != funcMap.end()) {
                if (m_importedLibraries.find(libName) != m_importedLibraries.end()) {
                    for (auto& [path, internal] : m_externalPaths) {
                        if (internal == libName) return path;
                    }
                    return libName;
                }
            }
        }
        return "";
    }

    std::string getSystemFuncExternalPath(const std::string& flatName) const {
        for (auto it = systemLibraries.begin(); it != systemLibraries.end(); ++it) {
            const auto& libName = it->first;
            const auto& funcMap = it->second;
            if (funcMap.find(flatName) != funcMap.end()) {
                for (auto& [path, internal] : m_externalPaths) {
                    if (internal == libName) return path;
                }
                return libName;
            }
        }
        return "";
    }

    static std::string getLastSegment(const std::string& path) {
        size_t dot = path.rfind('.');
        if (dot != std::string::npos) return path.substr(dot + 1);
        return path;
    }

    // Check if a function exists in a registered library
    bool hasSystemFunction(const std::string& libName, const std::string& funcName) const {
        auto libIt = systemLibraries.find(libName);
        if (libIt != systemLibraries.end()) {
            return libIt->second.find(funcName) != libIt->second.end();
        }
        auto extIt = externalLibraries.find(libName);
        if (extIt != externalLibraries.end()) {
            return extIt->second.find(funcName) != extIt->second.end();
        }
        return false;
    }

private:
    LibraryManager() = default;

    void loadLibraryFromFile(const std::string& libName, const std::string& filePath) {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open library file: " + filePath);
        }
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            if (line.find("function:") == 0) {
                std::string funcName = line.substr(9);
                funcName.erase(0, funcName.find_first_not_of(" \t"));
                funcName.erase(funcName.find_last_not_of(" \t") + 1);
            }
            else if (line.find("library:") == 0) {
                std::string nameInFile = line.substr(8);
                nameInFile.erase(0, nameInFile.find_first_not_of(" \t"));
                nameInFile.erase(nameInFile.find_last_not_of(" \t") + 1);
                if (nameInFile != libName) {
                    throw std::runtime_error("Library name mismatch: " + nameInFile + " != " + libName);
                }
            }
        }
        file.close();
        loadedExternalLibs.insert(libName);
    }

    std::unordered_map<std::string, std::unordered_map<std::string, std::function<Value(const std::vector<Value>&)>>> systemLibraries;
    std::unordered_map<std::string, std::unordered_map<std::string, std::function<Value(const std::vector<Value>&)>>> externalLibraries;
    std::unordered_map<std::string, std::string> externalLibraryPaths;
    std::unordered_map<std::string, bool> externalLibsToLoad;
    std::unordered_set<std::string> loadedExternalLibs;
    std::unordered_set<std::string> m_importedLibraries;
    std::unordered_map<std::string, std::string> m_aliasMap;
    std::unordered_map<std::string, std::string> m_externalPaths;
    std::unordered_set<std::string> m_hiddenInternalNames;
};

void initSystemLibraries();

inline void AddExternalLibrary(const std::string& libName, const std::string& libPath) {
    LibraryManager::getInstance().addExternalLibrary(libName, libPath);
}

inline void AddExternalFunction(const std::string& libName,
    const std::string& funcName,
    std::function<Value(const std::vector<Value>&)> func) {
    LibraryManager::getInstance().addExternalLibraryFunction(libName, funcName, func);
}

inline bool IsLibraryAvailable(const std::string& libName) {
    return LibraryManager::getInstance().isLibraryAvailable(libName);
}