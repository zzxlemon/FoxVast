#pragma once
#include "../../../src/interpreter/interpreter.hpp"
#include <random>

// random
class Random {
public:
	Value RandomStart(const std::vector<Value>& args);
private:
	Value RandomInt(const int min, const int max);
	Value RandomDouble(const double min, const double max);
	std::mt19937& rng();
};