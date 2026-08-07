#include "interpreter.hpp"
#include "../util/common.hpp"
#include "../util/utils.hpp"
#include "../util/dll_loader.hpp"
#include "../frontend/parser.hpp"
#include "../util/error_reporter.hpp"
#include <iostream>
#include <stdexcept>
#include "../../libs/system/random/SystemFunctionsRandom.h"
#include "../../libs/system/fs/SystemFunctionsFile.h"
#include "library_manager.hpp"   

std::vector<std::string> functions_register_map;
std::unordered_map<std::string, Value>* Interpreter::currentVariables = nullptr;
thread_local std::vector<std::string> interpreter_call_stack;

void Interpreter::parseCode(const std::string& code, const std::string& filename) {
    if (code.empty()) return;
    parse_failed = false; // reset so a reused instance can recover (P3-6)
    try {
        std::unordered_map<std::string, std::string> funcOrigins; // func name -> source file

        auto parseInto = [&](const std::string& src, const std::string& label) {
            std::unordered_map<std::string, Value> fileVars;
            std::unordered_map<std::string, Function> fileFuncs;
            Parser parser(src, fileVars, fileFuncs);
            parser.parseAllFunctions();
            for (const auto& [funcName, func] : fileFuncs) {
                if (funcOrigins.find(funcName) != funcOrigins.end()) {
                    throw std::runtime_error("Duplicate function '" + funcName + "' defined in '"
                        + funcOrigins[funcName] + "' and '" + label + "'");
                }
                funcOrigins[funcName] = label;
                functions[funcName] = func;
            }
        };

        // Parse the main file, then every imported file (import "file.fox").
        // Nested imports are appended to imported_source_files while parsing;
        // 'visited' prevents cycles and duplicate processing.
        imported_source_files.clear();
        import_base_file = filename;
        parseInto(code, filename.empty() ? "<main>" : filename);

        std::unordered_set<std::string> visited;
        for (size_t idx = 0; idx < imported_source_files.size(); idx++) {
            // Copy: parsing appends to imported_source_files (nested imports),
            // which may reallocate the vector.
            std::string path = imported_source_files[idx];
            if (!visited.insert(path).second) continue;
            std::string src = read_file(path);
            if (src.empty()) {
                throw std::runtime_error("Cannot read imported file: " + path);
            }
            import_base_file = path;
            parseInto(src, path);
        }
        import_base_file.clear();
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

    interpreter_call_stack.push_back(func.name);
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
    // A void function must not return a value (P3-8)
    if (func.returnType == "void" && returnValue.getType() != Value::Type::Void) {
        throw std::runtime_error("Function " + func.name + " is declared void but returned a value");
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
    interpreter_call_stack.clear();
    try {
        RegFunc();
        executeFunction(functions["main"]);
    }
    catch (const std::exception& e) {
        if (interpreter_call_stack.empty()) {
            ErrorReporter::reportFromException("RuntimeError", e.what());
        } else {
            ErrorReporter::reportRuntimeError(e.what(), interpreter_call_stack);
        }
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
        // Always register the built-in libraries (fallback if DLLs are missing)
        initSystemLibraries();
        // Load external library DLLs — overwrites built-in entries when found
        LoadFoxLibs(LibraryManager::getInstance());
        initialized = true;
    }
}
