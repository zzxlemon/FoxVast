#pragma once
#include <string>
#include <vector>
#include <windows.h>

extern std::string aaa;

/**
 * 读取文本文件中指定行号的内容（行号从 1 开始）
 * filename  文件路径
 * lineNum   目标行号（>= 1）
 * 该行的内容（不含换行符）
 * std::out_of_range 如果行号超出文件总行数
 */
std::string readLine(const std::string& filename, int lineNum);

/**
 * 将文本文件中指定行号的内容替换为新内容（行号从 1 开始）
 * filename  文件路径
 * lineNum   目标行号（>= 1）
 * newContent 要写入的新内容（不含换行符）
 * 如果行号超出文件总行数
 * std::runtime_error 文件操作失败
 */
void writeLine(const std::string& filename, int lineNum, const std::string& newContent);

void set_color(HANDLE h, WORD color);

void out_color(WORD color);

void err_color(WORD color);
