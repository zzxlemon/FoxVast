#pragma once

#include "../../../src/interpreter/interpreter.hpp"

// Math
class Math {
public:
	Value sin(const std::vector<Value>& args);
	Value cos(const std::vector<Value>& args);
	Value tan(const std::vector<Value>& args);
};