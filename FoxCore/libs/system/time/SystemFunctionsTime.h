#pragma once
#include "../../../src/interpreter/interpreter.hpp"

// time
class SystemFunctionsTime {
public:
    Value now(const std::vector<Value>& args);
    Value format(const std::vector<Value>& args);
    Value field(const std::vector<Value>& args);
};