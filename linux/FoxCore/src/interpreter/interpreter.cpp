#include "interpreter.hpp"
#include "../util/common.hpp"
#include "../util/utils.hpp"
#include "../util/dll_loader.hpp"
#include "../frontend/parser.hpp"
#include "../util/error_reporter.hpp"
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <functional>
#include "library_manager.hpp"   

std::vector<std::string> functions_register_map;
std::unordered_map<std::string, Value>* Interpreter::currentVariables = nullptr;
thread_local std::vector<std::string> interpreter_call_stack;

void Interpreter::parseCode(const std::string& code, const std::string& filename) {
    if (code.empty()) return;
    parse_failed = false; // reset so a reused instance can recover (P3-6)
    g_classRegistry.clear(); // fresh class/struct definitions per program
    try {
        std::unordered_map<std::string, std::string> funcOrigins; // func name -> source file
        std::unordered_map<std::string, std::vector<std::string>> bareNamesByPath;
        std::unordered_map<std::string, std::string> pathNsMap;
        std::unordered_map<std::string, std::pair<size_t, size_t>> nestedByPath;

        auto parseInto = [&](const std::string& src, const std::string& label,
            const std::string& ns, const std::string& alias) {
            std::unordered_map<std::string, Value> fileVars;
            std::unordered_map<std::string, Function> fileFuncs;
            Parser parser(src, fileVars, fileFuncs);
            import_prefix = ns;
        import_alias = alias;
            size_t startIdx = imported_source_files.size();
            parser.parseAllFunctions();
            import_prefix.clear();
        import_alias.clear();
            // Remember which files this file imported (by index range in
            // imported_source_files) so a duplicate import under another alias
            // can propagate the alias to the nested files as well.
            nestedByPath[label] = { startIdx, imported_source_files.size() };
            // Remember the bare (unnamespaced) function names of this file so a
            // later import of the same path under a different alias can still
            // register alias copies without re-parsing (see the loop below).
            {
                std::vector<std::string> names;
                names.reserve(fileFuncs.size());
                for (const auto& [funcName, unused] : fileFuncs) {
                    names.push_back(funcName);
                }
                bareNamesByPath[label] = std::move(names);
                pathNsMap[label] = ns;
            }
            // Namespace the functions of imported files: "namespace.name".
            // They can only be called with the prefix (libname.func(...)),
            // mirroring the system-library call convention. An optional alias
            // registers a second copy under "alias.name".
            if (!ns.empty()) {
                std::unordered_map<std::string, Function> renamed;
                for (auto& [funcName, func] : fileFuncs) {
                    func.name = ns + "." + funcName;
                    renamed[ns + "." + funcName] = std::move(func);
                }
                fileFuncs = std::move(renamed);
                if (!alias.empty() && alias != ns) {
                    std::unordered_map<std::string, Function> aliases;
                    for (const auto& [funcName, func] : fileFuncs) {
                        if (funcName.size() > ns.size()
                            && funcName.compare(0, ns.size() + 1, ns + ".") == 0) {
                            std::string bare = funcName.substr(ns.size() + 1);
                            Function copy = func;
                            copy.name = alias + "." + bare;
                            aliases[alias + "." + bare] = std::move(copy);
                        }
                    }
                    fileFuncs.insert(aliases.begin(), aliases.end());
                }
            }
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
        parseInto(code, filename.empty() ? "<main>" : filename, "", "");

        std::unordered_set<std::string> visited;
        std::vector<std::pair<std::string, std::string>> pendingAliases;
        for (size_t idx = 0; idx < imported_source_files.size(); idx++) {
            // Copy: parsing appends to imported_source_files (nested imports),
            // which may reallocate the vector.
            std::string path = std::get<0>(imported_source_files[idx]);
            std::string ns = std::get<1>(imported_source_files[idx]);
            std::string alias = std::get<2>(imported_source_files[idx]);
            if (!visited.insert(path).second) {
                // Same file imported again: the primary namespace is already
                // registered. Defer the alias copies until every file has been
                // parsed, because the duplicate entry usually appears before
                // the nested imports it depends on.
                if (!alias.empty() && alias != ns) {
                    pendingAliases.push_back({ path, alias });
                }
                continue;
            }
            std::string src = read_file(path);
            if (src.empty()) {
                throw std::runtime_error("Cannot read imported file: " + path);
            }
            import_base_file = path;
            parseInto(src, path, ns, alias);
        }
        import_base_file.clear();

        // Late alias registration: a file imported again under a different
        // alias gets "alias.name" copies of all its functions (including the
        // functions of its nested imports).
        if (!pendingAliases.empty()) {
            std::unordered_set<std::string> seen;
            std::function<void(const std::string&, const std::string&)> regAlias =
                [&](const std::string& p, const std::string& alias) {
                    if (!seen.insert(p + "\n" + alias).second) return;
                    auto namesIt = bareNamesByPath.find(p);
                    if (namesIt != bareNamesByPath.end()) {
                        std::string baseNs = pathNsMap[p];
                        for (const auto& bare : namesIt->second) {
                            auto fIt = functions.find(baseNs + "." + bare);
                            if (fIt == functions.end()) continue;
                            std::string key = alias + "." + bare;
                            if (functions.find(key) != functions.end()) continue;
                            Function copy = fIt->second;
                            copy.name = key;
                            functions[key] = std::move(copy);
                        }
                    }
                    auto nIt = nestedByPath.find(p);
                    if (nIt != nestedByPath.end()) {
                        for (size_t j = nIt->second.first; j < nIt->second.second; j++) {
                            regAlias(std::get<0>(imported_source_files[j]), alias);
                        }
                    }
                };
            for (const auto& [p, a] : pendingAliases) {
                regAlias(p, a);
            }
        }

        // Register class methods (named "<Class>.<method>", implicit 'this'
        // as first parameter) into the function table for the interpreter path.
        for (const auto& [className, def] : g_classRegistry) {
            for (const auto& method : def.methods) {
                functions[method.name] = method;
            }
            if (def.hasInit) {
                functions[className + ".init"] = def.initFunc;
            }
        }
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

void Interpreter::registerGcRoot() {
    if (gcRootId_ >= 0) return;
    gcRootId_ = Gc::instance().addRoot([this](Gc& gc) {
        for (const auto& [name, v] : variables) v.traceGC();
    });
}

void Interpreter::unregisterGcRoot() {
    if (gcRootId_ < 0) return;
    Gc::instance().removeRoot(gcRootId_);
    gcRootId_ = -1;
}

Value Interpreter::executeFunction(const Function& func) {
    Value returnValue;
    Parser::resetNewAllocBytes();
    registerGcRoot();

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
        Gc::instance().checkpoint();
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
        if (returnValue.getType() == Value::Type::Return) {
            throw std::runtime_error("Function " + func.name + " expects to return " + func.returnType
                + " type, but the bare 'ret' statement returns no value");
        }
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
        else if (func.returnType == "array" && returnValue.getType() != Value::Type::Array) {
            throw std::runtime_error("Function " + func.name + " expects to return array type, actually returned other type");
        }
        else if (func.returnType == "dict" && returnValue.getType() != Value::Type::Dict) {
            throw std::runtime_error("Function " + func.name + " expects to return dict type, actually returned other type");
        }
        else if (func.returnType == "bytes" && returnValue.getType() != Value::Type::Bytes) {
            throw std::runtime_error("Function " + func.name + " expects to return bytes type, actually returned other type");
        }
    }
    // A void function must not return a value (P3-8); a bare 'ret' marker is
    // fine and is converted to a plain void result.
    if (func.returnType == "void" && returnValue.getType() == Value::Type::Return) {
        return Value();
    }
    if (func.returnType == "void" && returnValue.getType() != Value::Type::Void) {
        throw std::runtime_error("Function " + func.name + " is declared void but returned a value");
    }

    unregisterGcRoot();
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
            "every FoxVast program must define a 'main' function");
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
        initSystemLibraries(); // no-op: system libraries are DLL-only
        if (!LoadFoxLibs(LibraryManager::getInstance())) {
            ErrorReporter::reportSimple("LibraryError",
                "No FoxVast library DLLs found. Place fox.*.dll next to fox.exe.");
            std::exit(1);
        }
        initialized = true;
    }
}
