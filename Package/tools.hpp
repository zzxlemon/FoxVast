#pragma once
#include <string>
#include <vector>
#include <windows.h>
#include <fstream>
#include <algorithm>
#include <stdexcept>

extern std::string aaa;

std::string readLine(const std::string& filename, int lineNum);

void writeLine(const std::string& filename, int lineNum, const std::string& newContent);

size_t countLines(const std::string& filename);

void set_color(HANDLE h, WORD color);

void out_color(WORD color);

void err_color(WORD color);
