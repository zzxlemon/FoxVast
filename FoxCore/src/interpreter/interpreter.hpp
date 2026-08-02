#pragma once
#include "../frontend/value.hpp"
#include "../frontend/function.hpp"
#include "../frontend/parser.hpp"
#include <unordered_map>
#include <string>
#include <vector>

extern std::vector<std::string> functions_register_map;

class Interpreter {
public:
    static std::unordered_map<std::string, Value>* currentVariables;

    std::unordered_map<std::string, Value> variables;
    std::unordered_map<std::string, Function> functions;
    bool parse_failed = false;
    int funcNewBytes = 0;

    void parseCode(const std::string& code, const std::string& filename = "");
    Value execute(const std::string& line);
    Value executeFunction(const Function& func);
    void runMainFunc();

	//register system functions api

    bool isSystemFunction(const std::string& funcName);
	Value SystemFunctionBuildIn(const std::string& funcName, const std::vector<Value>& args);
};

void RegFunc();