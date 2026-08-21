#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <fstream>
#include <iostream>

struct FarEntry {
    std::string name;
    std::vector<uint8_t> data;
};

struct FarArchive {
    std::vector<FarEntry> entries;

    bool load(const std::string& path);
    bool save(const std::string& path) const;

    void addFile(const std::string& name, const std::vector<uint8_t>& data);
    bool extractFile(const std::string& name, std::vector<uint8_t>& data) const;
    bool extractAll(const std::string& outputDir) const;
    bool hasFile(const std::string& name) const;
};

// CLI helpers
bool farPackage(const std::string& outputPath, const std::vector<std::string>& inputFiles);
bool farUnpack(const std::string& archivePath, const std::string& outputDir);
bool farRun(const std::string& archivePath);
