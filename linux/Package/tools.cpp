#include "tools.hpp"
#include <fstream>
#include <vector>
#include <string>
#include <stdexcept>
#include <cstdio>

std::string aaa = "none";

std::string readLine(const std::string& filename, int lineNum) {
    if (lineNum < 1) {
        throw std::out_of_range("琛屽彿蹇呴』浠?1 寮€濮?);
    }

    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("鏃犳硶鎵撳紑鏂囦欢: " + filename);
    }

    std::string line;
    int currentLine = 0;
    while (std::getline(file, line)) {
        ++currentLine;
        if (currentLine == lineNum) {
            return line;
        }
    }

    throw std::out_of_range("鏂囦欢鍙湁 " + std::to_string(currentLine) + " 琛岋紝璇锋眰琛屽彿 " + std::to_string(lineNum));
}

void writeLine(const std::string& filename, int lineNum, const std::string& newContent) {
    if (lineNum < 1) {
        throw std::out_of_range("琛屽彿蹇呴』浠?1 寮€濮?);
    }

    std::ifstream inFile(filename);
    if (!inFile.is_open()) {
        throw std::runtime_error("鏃犳硶鎵撳紑鏂囦欢: " + filename);
    }

    // 璇诲彇鎵€鏈夎鍒?vector
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(inFile, line)) {
        lines.push_back(line);
    }
    inFile.close();

    // 妫€鏌ヨ鍙锋槸鍚︽湁鏁?
    if (lineNum > static_cast<int>(lines.size())) {
        throw std::out_of_range("鏂囦欢鍙湁 " + std::to_string(lines.size()) + " 琛岋紝璇锋眰琛屽彿 " + std::to_string(lineNum));
    }

    // 鏇挎崲鐩爣琛?
    lines[lineNum - 1] = newContent;

    // 鍐欏洖鏂囦欢
    std::ofstream outFile(filename);
    if (!outFile.is_open()) {
        throw std::runtime_error("鏃犳硶鍐欏叆鏂囦欢: " + filename);
    }

    for (size_t i = 0; i < lines.size(); ++i) {
        outFile << lines[i];
        // 闄ゆ渶鍚庝竴琛屽锛岃ˉ鍏呮崲琛岀
        if (i != lines.size() - 1) {
            outFile << '\n';
        }
    }
    outFile.close();
}

size_t countLines(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("鏃犳硶鎵撳紑鏂囦欢: " + filename);
    }
    
    // 鐩存帴鍦ㄦ枃浠剁紦鍐插尯涓粺璁?'\n' 鐨勬暟閲?
    size_t lines = std::count(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>(),
        '\n'
    );
    
    // 娉ㄦ剰锛氬鏋滄枃浠朵笉涓虹┖涓旀渶鍚庝竴琛屾病鏈変互 '\n' 缁撳熬锛岃鏁伴渶瑕佸姞 1
    // 杩欓噷鍙互鍔犱竴涓畝鍗曠殑琛ユ閫昏緫锛堝彲閫夛級
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
