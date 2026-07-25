#include "ast.hpp"
#include "parser.hpp"
#include "../interpreter/interpreter.hpp"
#include "../interpreter/library_manager.hpp"
#include <iostream>   
#include <stdexcept> 

IdentifierExpr::IdentifierExpr(const std::string& n) : name(n) {}
Value IdentifierExpr::evaluate(std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    if (variables.find(name) == variables.end()) {
        throw std::runtime_error("Undefined variable: " + name);
    }
    return variables[name];
}

NumberExpr::NumberExpr(int v) : value(v) {}
Value NumberExpr::evaluate(std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    return Value(value);
}

DoubleExpr::DoubleExpr(double v) : value(v) {}
Value DoubleExpr::evaluate(std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    return Value(value);
}

StringExpr::StringExpr(const std::string& v) : value(v) {}
Value StringExpr::evaluate(std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    return Value(value);
}

ArrayExpr::ArrayExpr(std::vector<std::unique_ptr<Expr>>&& elems) : elements(std::move(elems)) {}

Value ArrayExpr::evaluate(std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    std::vector<Value> values;
    for (const auto& elem : elements) {
        values.push_back(elem->evaluate(variables, functions));
    }
    return Value(values);
}

IndexExpr::IndexExpr(std::unique_ptr<Expr> arr, std::unique_ptr<Expr> idx)
    : arrayExpr(std::move(arr)), indexExpr(std::move(idx)) {}

Value IndexExpr::evaluate(std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    Value arrValue = arrayExpr->evaluate(variables, functions);
    if (arrValue.getType() != Value::Type::Array) {
        throw std::runtime_error("Index target is not an array type");
    }
    int idx = indexExpr->evaluate(variables, functions).asInt();
    const std::vector<Value>& arr = arrValue.asArray();
    if (idx < 0 || idx >= static_cast<int>(arr.size())) {
        throw std::runtime_error("Array index out of bounds: " + std::to_string(idx));
    }
    return arr[idx];
}

CallExpr::CallExpr(const std::string& name) : funcName(name) {}
CallExpr::CallExpr(const std::string& name, std::vector<std::unique_ptr<Expr>>&& arguments)
    : funcName(name), args(std::move(arguments)) {
}
Value CallExpr::evaluate(std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    std::vector<Value> argVals;
    argVals.reserve(args.size());
    for (const auto& a : args) {
        argVals.push_back(a->evaluate(variables, functions));
    }

    Interpreter sys;
    if (sys.isSystemFunction(funcName)) {
        return sys.SystemFunctionBuildIn(funcName, argVals);
    }

    /* 
    * @Abandon
    if (!functions_register_map.empty()) {
        auto it = std::find(functions_register_map.begin(), functions_register_map.end(), funcName);
        if (it != functions_register_map.end()) {
            Interpreter sys;
            return sys.SystemFunctionBuildIn(funcName, argVals);
        }
    }
    */

    if (functions.find(funcName) == functions.end()) {
        auto& libMgr = LibraryManager::getInstance();
        std::string libName = libMgr.getBlockedLibName(funcName);
        if (!libName.empty()) {
            std::string shortName = LibraryManager::getLastSegment(libName);
            throw std::runtime_error("Function '" + funcName + "' is from the '" + libName
                + "' library. You must call it with the library prefix: '" + shortName + "." + funcName + "(...)'.");
        }
        std::string sysLibPath = libMgr.getSystemFuncExternalPath(funcName);
        if (!sysLibPath.empty()) {
            std::string shortName = LibraryManager::getLastSegment(sysLibPath);
            throw std::runtime_error("Function '" + funcName + "' requires importing a library first.\n"
                "  Use: import " + sysLibPath + "\n"
                "  Then: " + shortName + "." + funcName + "(...)\n"
                "  Or with alias: import " + sysLibPath + " -> my_alias\n"
                "  Then: my_alias." + funcName + "(...)");
        }
        throw std::runtime_error("Undefined function: " + funcName);
    }

    const Function& func = functions[funcName];

    if (argVals.size() != func.parameters.size()) {
        throw std::runtime_error("Function " + funcName + " expects " +
            std::to_string(func.parameters.size()) + " arguments, got " +
            std::to_string(argVals.size()));
    }

    Interpreter funcInterp;
    funcInterp.variables = variables;
    funcInterp.functions = functions;

    for (size_t i = 0; i < argVals.size(); ++i) {
        funcInterp.variables[func.parameters[i].name] = argVals[i];
    }

    Value result = funcInterp.executeFunction(func);

    return result;
}

BinaryExpr::BinaryExpr(std::unique_ptr<Expr> l, TokenT o, std::unique_ptr<Expr> r)
    : left(std::move(l)), op(o), right(std::move(r)) {
}

Value BinaryExpr::evaluate(std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    Value leftVal = left->evaluate(variables, functions);
    Value rightVal = right->evaluate(variables, functions);

    if (leftVal.getType() == Value::Type::Int && rightVal.getType() == Value::Type::Int) {
        int result;
        switch (op) {
        case TOKEN_PLUS: result = leftVal.asInt() + rightVal.asInt(); break;
        case TOKEN_MINUS: result = leftVal.asInt() - rightVal.asInt(); break;
        default: throw std::runtime_error("Unsupported operator");
        }
        return Value(result);
    }
    else if (leftVal.getType() == Value::Type::Double && rightVal.getType() == Value::Type::Double) {
        double result;
        switch (op) {
        case TOKEN_PLUS: result = leftVal.asDouble() + rightVal.asDouble(); break;
        case TOKEN_MINUS: result = leftVal.asDouble() - rightVal.asDouble(); break;
        default: throw std::runtime_error("Unsupported operator");
        }
        return Value(result);
    }
    else {
        throw std::runtime_error("Operation error: type mismatch (only int/double of same type supported)");
    }
}

Value InputExpr::evaluate(std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    std::string userInput;
    std::getline(std::cin, userInput);
    return Value(userInput);
}

Value UnaryExpr::evaluate(std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    Value val = operand->evaluate(variables, functions);
    if (op == TOKEN_NOT) {
        return Value(val.asBool() ? 0 : 1);
    }
    if (op == TOKEN_MINUS) {
        if (val.getType() == Value::Type::Int) {
            return Value(-val.asInt());
        } else if (val.getType() == Value::Type::Double) {
            return Value(-val.asDouble());
        }
        throw std::runtime_error("Unary minus requires a numeric operand");
    }
    throw std::runtime_error("Unsupported unary operator");
}

Value NewExpr::evaluate(std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    Value sizeVal = sizeExpr->evaluate(variables, functions);
    int size = sizeVal.asInt();
    if (size < 0) {
        throw std::runtime_error("new() size must be non-negative");
    }
    std::vector<uint8_t> bytes(size, 0);
    return Value(bytes);
}

Value CastExpr::evaluate(std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    Value val = expr->evaluate(variables, functions);
    switch (val.getType()) {
    case Value::Type::Int:
        if (castType == CastType::Int) {
            return Value(val.asInt());
        }
        else if (castType == CastType::Double) {
            return Value(static_cast<double>(val.asInt()));
        }
        break;
    case Value::Type::Double:
        if (castType == CastType::Int) {
            return Value(static_cast<int>(val.asDouble()));
        }
        else if (castType == CastType::Double) {
            return Value(val.asDouble());
        }
        break;
    case Value::Type::String: {
        std::string strVal = val.asString();
        try {
            if (castType == CastType::Int) {
                int intVal = std::stoi(strVal);
                return Value(intVal);
            }
            else if (castType == CastType::Double) {
                double doubleVal = std::stod(strVal);
                return Value(doubleVal);
            }
        }
        catch (const std::invalid_argument& e) {
            throw std::runtime_error("Cast error: cannot convert \"" + strVal + "\" to numeric value");
        }
        catch (const std::out_of_range& e) {
            throw std::runtime_error("Cast error: \"" + strVal + "\" out of numeric range");
        }
        break;
    }
    default:
        break;
    }

    throw std::runtime_error("Cast error: unsupported source type for cast");
}

Value CompareExpr::evaluate(std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    Value leftVal = left->evaluate(variables, functions);
    Value rightVal = right->evaluate(variables, functions);
    bool result = false;

    if (leftVal.getType() == Value::Type::Int && rightVal.getType() == Value::Type::Int) {
        int l = leftVal.asInt(), r = rightVal.asInt();
        switch (op) {
        case CompareType::EQ: result = (l == r); break;
        case CompareType::NE: result = (l != r); break;
        case CompareType::GT: result = (l > r); break;
        case CompareType::LT: result = (l < r); break;
        case CompareType::GE: result = (l >= r); break;
        case CompareType::LE: result = (l <= r); break;
        }
    }
    else if (leftVal.getType() == Value::Type::Double && rightVal.getType() == Value::Type::Double) {
        double l = leftVal.asDouble(), r = rightVal.asDouble();
        switch (op) {
        case CompareType::EQ: result = (l == r); break;
        case CompareType::NE: result = (l != r); break;
        case CompareType::GT: result = (l > r); break;
        case CompareType::LT: result = (l < r); break;
        case CompareType::GE: result = (l >= r); break;
        case CompareType::LE: result = (l <= r); break;
        }
    }
    else {
        throw std::runtime_error("Comparison error: only int/double of same type supported");
    }

    return Value(result ? 1 : 0);
}

Value ConditionExpr::evaluate(std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    Value leftVal = left->evaluate(variables, functions);
    bool leftBool = leftVal.asBool();

    if (op == TOKEN_OR) {
        if (leftBool) return Value(1);
        Value rightVal = right->evaluate(variables, functions);
        return Value(rightVal.asBool() ? 1 : 0);
    }
    else if (op == TOKEN_AND) {
        if (!leftBool) return Value(0);
        Value rightVal = right->evaluate(variables, functions);
        return Value(rightVal.asBool() ? 1 : 0);
    }
    else {
        throw std::runtime_error("Unsupported condition operator");
    }
}

// ============================================================
// Stmt implementations
// ============================================================

PrintStmt::PrintStmt(std::unique_ptr<Expr> a) : arg(std::move(a)) {}
Value PrintStmt::execute(std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    Value val = arg->evaluate(variables, functions);
    switch (val.getType()) {
    case Value::Type::Int: std::cout << val.asInt(); break;
    case Value::Type::Double: std::cout << val.asDouble(); break;
    case Value::Type::String: std::cout << val.asString(); break;
    case Value::Type::Void: break;
    case Value::Type::Array: std::cout << "[array]"; break;
    default: break;
    }
    return Value();
}

PrintlnStmt::PrintlnStmt(std::unique_ptr<Expr> a) : arg(std::move(a)) {}
Value PrintlnStmt::execute(std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    Value val = arg->evaluate(variables, functions);
    switch (val.getType()) {
    case Value::Type::Int: std::cout << val.asInt(); break;
    case Value::Type::Double: std::cout << val.asDouble(); break;
    case Value::Type::String: std::cout << val.asString(); break;
    case Value::Type::Void: break;
    case Value::Type::Array: std::cout << "[array]"; break;
    default: break;
    }
    std::cout << std::endl;
    return Value();
}

ExitStmt::ExitStmt(std::unique_ptr<Expr> a) : arg(std::move(a)) {}
Value ExitStmt::execute(std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    Value val = arg->evaluate(variables, functions);
    if (val.getType() == Value::Type::Int) std::exit(val.asInt());
    std::exit(0);
    return Value();
}

RetStmt::RetStmt(std::unique_ptr<Expr> a) : arg(std::move(a)), hasArg(a != nullptr) {}
Value RetStmt::execute(std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    if (hasArg) {
        return arg->evaluate(variables, functions);
    }
    return Value();
}

Value EndlStmt::execute(std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    std::cout << std::endl;
    return Value();
}

InputStmt::InputStmt(const std::string& name) : varName(name) {}
Value InputStmt::execute(std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    std::string userInput;
    std::getline(std::cin, userInput);
    variables[varName] = Value(userInput);
    return Value();
}

CallStmt::CallStmt(const std::string& name, std::vector<std::unique_ptr<Expr>> arguments)
    : funcName(name), args(std::move(arguments)) {}
Value CallStmt::execute(std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    std::vector<Value> argVals;
    argVals.reserve(args.size());
    for (const auto& a : args) {
        argVals.push_back(a->evaluate(variables, functions));
    }
    Interpreter sys;
    if (sys.isSystemFunction(funcName)) {
        return sys.SystemFunctionBuildIn(funcName, argVals);
    }
    if (functions.find(funcName) == functions.end()) {
        auto& libMgr = LibraryManager::getInstance();
        std::string libName = libMgr.getBlockedLibName(funcName);
        if (!libName.empty()) {
            std::string shortName = LibraryManager::getLastSegment(libName);
            throw std::runtime_error("Function '" + funcName + "' is from the '" + libName
                + "' library. You must call it with the library prefix: '" + shortName + "." + funcName + "(...)'.");
        }
        std::string sysLibPath = libMgr.getSystemFuncExternalPath(funcName);
        if (!sysLibPath.empty()) {
            std::string shortName = LibraryManager::getLastSegment(sysLibPath);
            throw std::runtime_error("Function '" + funcName + "' requires importing a library first.\n"
                "  Use: import " + sysLibPath + "\n"
                "  Then: " + shortName + "." + funcName + "(...)\n"
                "  Or with alias: import " + sysLibPath + " -> my_alias\n"
                "  Then: my_alias." + funcName + "(...)");
        }
        throw std::runtime_error("Undefined function: " + funcName);
    }
    const Function& func = functions[funcName];
    if (argVals.size() != func.parameters.size()) {
        throw std::runtime_error("Function " + funcName + " expects " +
            std::to_string(func.parameters.size()) + " arguments, got " +
            std::to_string(argVals.size()));
    }
    Interpreter funcInterp;
    funcInterp.variables = variables;
    funcInterp.functions = functions;
    for (size_t i = 0; i < argVals.size(); ++i) {
        funcInterp.variables[func.parameters[i].name] = argVals[i];
    }
    Value result = funcInterp.executeFunction(func);
    return result;
}

AssignStmt::AssignStmt(const std::string& name, std::unique_ptr<Expr> e)
    : varName(name), expr(std::move(e)) {}
Value AssignStmt::execute(std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    variables[varName] = expr->evaluate(variables, functions);
    if (variables[varName].getType() == Value::Type::Bytes) {
        int sz = static_cast<int>(variables[varName].asBytes().size());
        // checkNewAllocBytes is static in Parser
    }
    return Value();
}

IndexAssignStmt::IndexAssignStmt(const std::string& name, std::unique_ptr<Expr> idx, std::unique_ptr<Expr> val)
    : varName(name), index(std::move(idx)), value(std::move(val)) {}
Value IndexAssignStmt::execute(std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    Value idxVal = index->evaluate(variables, functions);
    Value val = value->evaluate(variables, functions);
    if (variables.find(varName) == variables.end()) {
        throw std::runtime_error("Undefined array variable: " + varName);
    }
    std::vector<Value>& arr = variables[varName].asArrayRef();
    int idx = idxVal.asInt();
    if (idx < 0 || idx >= static_cast<int>(arr.size())) {
        throw std::runtime_error("Array index out of bounds: " + std::to_string(idx));
    }
    arr[idx] = val;
    return Value();
}

IfStmt::IfStmt(const std::string& cond, std::vector<std::unique_ptr<Stmt>>&& b)
    : condition(cond), body(std::move(b)) {}
Value IfStmt::execute(std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    if (condition.empty()) return Value();
    Lexer condLexer(condition);
    Token condToken = condLexer.nextToken();
    Parser::skipWhitespace(condLexer, condToken);
    auto condExpr = Parser::parseExpr(condLexer, condToken);
    Value condResult = condExpr->evaluate(variables, functions);
    if (condResult.asBool()) {
        for (const auto& stmt : body) {
            if (!stmt) continue;
            Value val = stmt->execute(variables, functions);
            if (val.getType() != Value::Type::Void) {
                return val;
            }
        }
    }
    return Value();
}

WhileStmt::WhileStmt(const std::string& cond, std::vector<std::unique_ptr<Stmt>>&& b)
    : condition(cond), body(std::move(b)) {}
Value WhileStmt::execute(std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    if (condition.empty()) return Value();
    Lexer condLexer(condition);
    Token condToken = condLexer.nextToken();
    Parser::skipWhitespace(condLexer, condToken);
    auto condExpr = Parser::parseExpr(condLexer, condToken);
    Value condResult = condExpr->evaluate(variables, functions);
    while (condResult.asBool()) {
        for (const auto& stmt : body) {
            if (!stmt) continue;
            Value val = stmt->execute(variables, functions);
            if (val.getType() != Value::Type::Void) {
                return val;
            }
        }
        condLexer = Lexer(condition);
        condToken = condLexer.nextToken();
        Parser::skipWhitespace(condLexer, condToken);
        condExpr = Parser::parseExpr(condLexer, condToken);
        condResult = condExpr->evaluate(variables, functions);
    }
    return Value();
}

ForStmt::ForStmt(const std::string& i, const std::string& c, const std::string& it,
    std::vector<std::unique_ptr<Stmt>>&& b)
    : init(i), condition(c), iter(it), body(std::move(b)) {}
Value ForStmt::execute(std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    if (!init.empty()) {
        Parser::parseLine(init, variables, functions);
    }
    while (true) {
        if (!condition.empty()) {
            Lexer condLexer(condition);
            Token condToken = condLexer.nextToken();
            Parser::skipWhitespace(condLexer, condToken);
            auto condExpr = Parser::parseExpr(condLexer, condToken);
            Value condResult = condExpr->evaluate(variables, functions);
            if (!condResult.asBool()) break;
        } else {
            break;
        }
        for (const auto& stmt : body) {
            if (!stmt) continue;
            Value val = stmt->execute(variables, functions);
            if (val.getType() != Value::Type::Void) {
                return val;
            }
        }
        if (!iter.empty()) {
            Parser::parseLine(iter, variables, functions);
        }
    }
    return Value();
}

LabelStmt::LabelStmt(const std::string& n) : name(n) {}
Value LabelStmt::execute(std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    return Value(); // no-op
}

GotoStmt::GotoStmt(const std::string& l) : label(l) {}
Value GotoStmt::execute(std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    throw GotoException(label);
}