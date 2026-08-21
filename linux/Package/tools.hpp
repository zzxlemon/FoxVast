#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>
#include <stdexcept>

extern std::string aaa;

std::string readLine(const std::string& filename, int lineNum);

void writeLine(const std::string& filename, int lineNum, const std::string& newContent);

size_t countLines(const std::string& filename);

// ANSI 256-color helpers (16/256-color terminals; no-op on non-color output).
void set_color(int color);

void out_color(int color);

void err_color(int color);
