#include "interpreter.hpp"
#include "../util/common.hpp"
#include "../frontend/parser.hpp"
#include "../util/error_reporter.hpp"
#include <iostream>
#include <stdexcept>
#include "../../libs/system/random/SystemFunctionsRandom.h"
#include "../../libs/system/fs/SystemFunctionsFile.h"
#include "library_manager.hpp"   

std::vector<std::string> functions_register_map;

void Interpreter::parseCode(const std::string& code, const std::string& filename) {
    if (code.empty()) return;
    try {
        Parser parser(code, variables, functions);
        parser.parseAllFunctions();
    }
    catch (const std::exception& e) {
        parse_failed = true;
        ErrorReporter::reportParseError(e.what());
    }
}

Value Interpreter::execute(const std::string& line) {
    if (line.empty()) return Value();
    return Parser::parseLine(line, variables, functions);
}

Value Interpreter::executeFunction(const Function& func) {
    Value returnValue;
    Parser::resetNewAllocBytes();

    // Pre-scan labels for goto
    std::unordered_map<std::string, size_t> labels;
    for (size_t i = 0; i < func.compiledBody.size(); i++) {
        if (auto* labelStmt = dynamic_cast<LabelStmt*>(func.compiledBody[i].get())) {
            labels[labelStmt->name] = i;
        }
    }

    size_t i = 0;
    while (i < func.compiledBody.size()) {
        try {
            const auto& stmt = func.compiledBody[i];
            if (!stmt) {
                i++;
                continue;
            }

            if (dynamic_cast<LabelStmt*>(stmt.get())) {
                i++;
                continue;
            }

            Value val = stmt->execute(variables, functions);
            if (val.getType() != Value::Type::Void) {
                returnValue = val;
                break;
            }
            i++;
        } catch (const GotoException& e) {
            auto it = labels.find(e.label);
            if (it == labels.end()) {
                throw std::runtime_error("Undefined goto label: " + e.label);
            }
            i = it->second;
        }
    }

    if (func.returnType != "void") {
        if (func.returnType == "int" && returnValue.getType() != Value::Type::Int) {
            throw std::runtime_error("Function " + func.name + " expects to return int type, actually returned " +
                (returnValue.getType() == Value::Type::Void ? "void" : "other type"));
        }
        else if (func.returnType == "string" && returnValue.getType() != Value::Type::String) {
            throw std::runtime_error("Function " + func.name + " expects to return string type, actually returned other type");
        }
        else if (func.returnType == "double" && returnValue.getType() != Value::Type::Double) {
            throw std::runtime_error("Function " + func.name + " expects to return double type, actually returned other type");
        }
    }

    return returnValue;
}

void Interpreter::runMainFunc() {
    if (isOutInfo) {
        printf("==========RUN==========\n");
    }
    if (parse_failed) {
        return;
    }
    if (functions.find("main") == functions.end()) {
        ErrorReporter::reportSimple("RuntimeError", "main function not found",
            "every FoxLang program must define a 'main' function");
        return;
    }
    try {
        RegFunc();
        executeFunction(functions["main"]);
    }
    catch (const std::exception& e) {
        ErrorReporter::reportFromException("RuntimeError", e.what());
    }
}

// System function recognition: ONLY dot-notation (lib.func or alias.func)
// Flat/bare names like "random" or "cos" are NEVER recognized as system functions.
// Users MUST use "import lib" and call with "lib.func(...)" (or alias prefix).
// The library must be imported first, otherwise the call is rejected.
bool Interpreter::isSystemFunction(const std::string& funcName) {
    auto& libMgr = LibraryManager::getInstance();

    size_t dotPos = funcName.rfind('.');
    if (dotPos != std::string::npos) {
        std::string libPrefix = funcName.substr(0, dotPos);
        std::string resolvedLib = libMgr.resolveAlias(libPrefix);
        if (libMgr.hasLibrary(resolvedLib) && libMgr.isImported(resolvedLib)) {
            return true;
        }
    }

    return false;
}

Value Interpreter::SystemFunctionBuildIn(const std::string& funcName, const std::vector<Value>& args) {
    auto& libMgr = LibraryManager::getInstance();

    size_t dotPos = funcName.rfind('.');
    if (dotPos != std::string::npos) {
        std::string libName = funcName.substr(0, dotPos);
        std::string funcOnly = funcName.substr(dotPos + 1);
        libName = libMgr.resolveAlias(libName);
        return libMgr.callSystemFunction(libName, funcOnly, args);
    }
    throw std::runtime_error("Unimplemented system function: " + funcName);
}

void RegFunc() {
    static bool initialized = false;
    if (!initialized) {
        initSystemLibraries();
        initialized = true;
    }
}
