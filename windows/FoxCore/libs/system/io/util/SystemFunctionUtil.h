#pragma once

#include "../../../../src/interpreter/interpreter.hpp"

// Util
class Util {
public:
    Value length(const std::vector<Value>& args);
    Value arr_append(const std::vector<Value>& args);
    Value arr_pop(const std::vector<Value>& args);
    Value arr_contains(const std::vector<Value>& args);
    Value arr_slice(const std::vector<Value>& args);
    Value arr_sort(const std::vector<Value>& args);
    Value arr_length(const std::vector<Value>& args);
    Value str_contains(const std::vector<Value>& args);
    Value str_replace(const std::vector<Value>& args);
    Value str_split(const std::vector<Value>& args);
    Value str_trim(const std::vector<Value>& args);
    Value str_lower(const std::vector<Value>& args);
    Value str_upper(const std::vector<Value>& args);
    Value str_substring(const std::vector<Value>& args);
    Value str_length(const std::vector<Value>& args);
    Value IntChangeString(const std::vector<Value>& args);
    Value StringChangeInt(const std::vector<Value>& args);
    Value StringChangeDouble(const std::vector<Value>& args);
    Value DoubleChangeString(const std::vector<Value>& args);
    Value DoubleChangeInt(const std::vector<Value>& args);
    Value IntChangeDouble(const std::vector<Value>& args);
    Value TypeOf(const std::vector<Value>& args);
};
