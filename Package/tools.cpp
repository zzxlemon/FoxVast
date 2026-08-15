#include "tools.hpp"
#include <fstream>

std::string aaa = "none";

std::string readLine(const std::string& filename, int lineNum) {
    if (lineNum < 1) {
        throw std::out_of_range("行号必须从 1 开始");
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
            return line;  // 返回此行内容
        }
    }

    // 如果遍历完仍没找到，说明行号过大
    throw std::out_of_range("文件只有 " + std::to_string(currentLine) + " 行，请求行号 " + std::to_string(lineNum));
}

void writeLine(const std::string& filename, int lineNum, const std::string& newContent) {
    if (lineNum < 1) {
        throw std::out_of_range("行号必须从 1 开始");
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
        // 除最后一行外，补充换行符
        if (i != lines.size() - 1) {
            outFile << '\n';
        }
    }
    outFile.close();
}

void set_color(HANDLE h, WORD color) {
    SetConsoleTextAttribute(h, color);
}

void out_color(WORD color) {
    set_color(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

void err_color(WORD color) {
    set_color(GetStdHandle(STD_ERROR_HANDLE), color);
}
