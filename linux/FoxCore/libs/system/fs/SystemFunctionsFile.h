#pragma once

#include "../../../src/interpreter/interpreter.hpp"
#include <fstream>

// file
extern std::vector<std::fstream> FileKey;
extern std::fstream outfiles[];

struct FileInfo;

class File {
public:
	Value FileOpen(const std::vector<Value>& args);
	Value FileRead(const std::vector<Value>& args);
	Value FileWrite(const std::vector<Value>& args);
	Value FileClose(const std::vector<Value>& args);
	Value FileDelete(const std::vector<Value>& args);	
};