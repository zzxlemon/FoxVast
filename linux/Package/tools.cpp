#include "tools.hpp"
#include <fstream>
#include <vector>
#include <string>
#include <stdexcept>
#include <cstdio>

std::string aaa = "none";

std::string readLine(const std::string& filename, int lineNum) {
    if (lineNum < 1) {
        throw std::out_of_range("行号必须从1开始");
    }

    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("无法打开文件: " + filename);
    }

    std::string line;
    int currentLine = 0;
    while (std::getline(file, line)) {
        ++currentLine;
        if (currentLine == lineNum) {
            return line;
        }
    }

    throw std::out_of_range("文件只有 " + std::to_string(currentLine) + " 行，请求行号 " + std::to_string(lineNum));
}

void writeLine(const std::string& filename, int lineNum, const std::string& newContent) {
    if (lineNum < 1) {
        throw std::out_of_range("行号必须从1开始");
    }

    std::ifstream inFile(filename);
    if (!inFile.is_open()) {
        throw std::runtime_error("无法打开文件: " + filename);
    }

    // 读取所有行到 vector
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(inFile, line)) {
        lines.push_back(line);
    }
    inFile.close();

    // 检查行号是否有效
    if (lineNum > static_cast<int>(lines.size())) {
        throw std::out_of_range("文件只有 " + std::to_string(lines.size()) + " 行，请求行号 " + std::to_string(lineNum));
    }

    // 替换目标行
    lines[lineNum - 1] = newContent;

    // 写回文件
    std::ofstream outFile(filename);
    if (!outFile.is_open()) {
        throw std::runtime_error("无法写入文件: " + filename);
    }

    for (size_t i = 0; i < lines.size(); ++i) {
        outFile << lines[i];
        // 除了最后一行外，补充换行符
        if (i != lines.size() - 1) {
            outFile << '\n';
        }
    }
    outFile.close();
}

size_t countLines(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("无法打开文件: " + filename);
    }
    
    // 直接在文件缓冲区中统计 '\n' 的数量
    size_t lines = std::count(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>(),
        '\n'
    );
    
    // 注意：如果文件不为空且最后一行没有以 '\n' 结尾，行数需要加 1
    // 这里可以加一个简单的补充逻辑（可选）
    return lines;
}

void set_color(int color) {
    std::fprintf(stdout, "\x1b[38;5;%dm", color);
    std::fflush(stdout);
}

void out_color(int color) {
    set_color(color);
}

void err_color(int color) {
    std::fprintf(stderr, "\x1b[38;5;%dm", color);
    std::fflush(stderr);
}
