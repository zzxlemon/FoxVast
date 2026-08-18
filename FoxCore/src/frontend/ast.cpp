#include "ast.hpp"
#include "parser.hpp"
#include "../interpreter/interpreter.hpp"
#include "../interpreter/library_manager.hpp"
#include "../vm/bytecode.hpp"
#include <iostream>   
#include <stdexcept> 
#include <typeinfo>
#include <cmath>

IdentifierExpr::IdentifierExpr(const std::string& n) : name(n) {}
Value IdentifierExpr::evaluate(std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    auto it = variables.find(name);
    if (it != variables.end()) {
        return it->second;
    }
    // Object member read: 'obj.field' (lexer merges dotted identifiers)
    Value member;
    if (readObjectMember(variables, name, member)) {
        return member;
    }
    throw std::runtime_error("Undefined variable: " + name);
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

DictExpr::DictExpr(std::vector<std::pair<std::string, std::unique_ptr<Expr>>>&& e)
    : entries(std::move(e)) {}

Value DictExpr::evaluate(std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    std::unordered_map<std::string, std::shared_ptr<Value>> result;
    for (const auto& entry : entries) {
        result[entry.first] = std::make_shared<Value>(entry.second->evaluate(variables, functions));
    }
    return Value(result);
}

Value::Type DictExpr::compileBytecode(CompiledFunction& cf,
    std::unordered_map<std::string, Value::Type>& varTypes) const {
    for (const auto& entry : entries) {
        entry.second->compileBytecode(cf, varTypes);
        int keyIdx = cf.addConstantStringDedup(entry.first);
        cf.chunk.writeOp(OpCode::OP_CONSTANT);
        cf.chunk.writeShort(static_cast<uint16_t>(keyIdx));
    }
    cf.chunk.writeOp(OpCode::OP_DICT);
    cf.chunk.writeByte(static_cast<uint8_t>(entries.size()));
    return Value::Type::Dict;
}

IndexExpr::IndexExpr(std::unique_ptr<Expr> arr, std::unique_ptr<Expr> idx)
    : arrayExpr(std::move(arr)), indexExpr(std::move(idx)) {}

Value IndexExpr::evaluate(std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    Value arrValue = arrayExpr->evaluate(variables, functions);
    if (arrValue.getType() == Value::Type::Array) {
        int idx = indexExpr->evaluate(variables, functions).asInt();
        const std::vector<Value>& arr = arrValue.asArray();
        if (idx < 0 || idx >= static_cast<int>(arr.size())) {
            throw std::runtime_error("Array index out of bounds: " + std::to_string(idx));
        }
        return arr[idx];
    }
    if (arrValue.getType() == Value::Type::Dict) {
        const Value& keyValue = indexExpr->evaluate(variables, functions);
        if (keyValue.getType() != Value::Type::String) {
            throw std::runtime_error("Dict index must be a string");
        }
        const auto& dict = arrValue.asDict();
        auto it = dict.find(keyValue.asString());
        if (it == dict.end()) {
            throw std::runtime_error("Undefined dict key: " + keyValue.asString());
        }
        return *it->second;
    }
    throw std::runtime_error("Index target is not an array or dict type");
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

    Interpreter::currentVariables = &variables;
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
        Value methodResult;
        if (callObjectMethod(variables, functions, funcName, argVals, methodResult)) {
            return methodResult;
        }
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
        case TOKEN_MUL: result = leftVal.asInt() * rightVal.asInt(); break;
        case TOKEN_DIV:
            if (rightVal.asInt() == 0) throw std::runtime_error("Division by zero");
            result = leftVal.asInt() / rightVal.asInt(); break;
        case TOKEN_MOD:
            if (rightVal.asInt() == 0) throw std::runtime_error("Modulo by zero");
            result = leftVal.asInt() % rightVal.asInt(); break;
        default: throw std::runtime_error("Unsupported operator");
        }
        return Value(result);
    }
    else if (leftVal.getType() == Value::Type::Double && rightVal.getType() == Value::Type::Double) {
        double result;
        switch (op) {
        case TOKEN_PLUS: result = leftVal.asDouble() + rightVal.asDouble(); break;
        case TOKEN_MINUS: result = leftVal.asDouble() - rightVal.asDouble(); break;
        case TOKEN_MUL: result = leftVal.asDouble() * rightVal.asDouble(); break;
        case TOKEN_DIV:
            if (rightVal.asDouble() == 0.0) throw std::runtime_error("Division by zero");
            result = leftVal.asDouble() / rightVal.asDouble(); break;
        case TOKEN_MOD:
            if (rightVal.asDouble() == 0.0) throw std::runtime_error("Modulo by zero");
            result = std::fmod(leftVal.asDouble(), rightVal.asDouble()); break;
        default: throw std::runtime_error("Unsupported operator");
        }
        return Value(result);
    }
    else if (leftVal.getType() == Value::Type::String && rightVal.getType() == Value::Type::String) {
        // Align with VM OP_ADD: string concatenation (P2-1)
        std::string result;
        switch (op) {
        case TOKEN_PLUS: result = leftVal.asString() + rightVal.asString(); break;
        default: throw std::runtime_error("Unsupported operator for strings");
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

ObjectNewExpr::ObjectNewExpr(const std::string& cn, std::vector<std::unique_ptr<Expr>>&& a)
    : className(cn), args(std::move(a)) {}

Value ObjectNewExpr::evaluate(std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    std::vector<Value> argVals;
    argVals.reserve(args.size());
    for (const auto& a : args) {
        argVals.push_back(a->evaluate(variables, functions));
    }

    auto classIt = g_classRegistry.find(className);
    if (classIt == g_classRegistry.end()) {
        throw std::runtime_error("Undefined class: " + className);
    }
    const ClassDef& def = classIt->second;

    Value obj = Value::makeObject(className);
    for (const auto& f : def.fields) {
        obj.asObjectDictRef()[f.name] = std::make_shared<Value>(defaultFieldValue(f.type));
    }

    if (def.hasInit) {
        if (argVals.size() != def.initFunc.parameters.size() - 1) {
            throw std::runtime_error("Class " + className + " constructor expects " +
                std::to_string(def.initFunc.parameters.size() - 1) + " arguments, got " +
                std::to_string(argVals.size()));
        }
        Interpreter funcInterp;
        funcInterp.variables = variables;
        funcInterp.functions = functions;
        funcInterp.variables[def.initFunc.parameters[0].name] = obj;
        for (size_t i = 0; i < argVals.size(); ++i) {
            funcInterp.variables[def.initFunc.parameters[i + 1].name] = argVals[i];
        }
        funcInterp.executeFunction(def.initFunc);
    }
    else if (!argVals.empty()) {
        if (argVals.size() != def.fields.size()) {
            throw std::runtime_error("Class " + className + " has no constructor; expected " +
                std::to_string(def.fields.size()) + " positional field arguments, got " +
                std::to_string(argVals.size()));
        }
        for (size_t i = 0; i < def.fields.size(); ++i) {
            obj.asObjectDictRef()[def.fields[i].name] = std::make_shared<Value>(argVals[i]);
        }
    }
    return obj;
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
    else if (leftVal.getType() == Value::Type::String && rightVal.getType() == Value::Type::String) {
        // Align with VM OP_EQ/OP_NE: string equality (P2-1)
        std::string l = leftVal.asString(), r = rightVal.asString();
        switch (op) {
        case CompareType::EQ: result = (l == r); break;
        case CompareType::NE: result = (l != r); break;
        default: throw std::runtime_error("Comparison error: only == and != supported for strings");
        }
    }
    else if (leftVal.getType() == rightVal.getType() &&
             (leftVal.getType() == Value::Type::Array ||
              leftVal.getType() == Value::Type::Dict ||
              leftVal.getType() == Value::Type::Object ||
              leftVal.getType() == Value::Type::Bytes)) {
        bool equal = valuesEqual(leftVal, rightVal);
        switch (op) {
        case CompareType::EQ: result = equal; break;
        case CompareType::NE: result = !equal; break;
        default: throw std::runtime_error("Comparison error: only == and != supported for this type");
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
// Bytecode compilation (virtual dispatch, no dynamic_cast)
// ============================================================
Value::Type IdentifierExpr::compileBytecode(CompiledFunction& cf,
    std::unordered_map<std::string, Value::Type>& varTypes) const {
    size_t dot = name.find('.');
    if (dot != std::string::npos) {
        // Object member read: OP_GET_GLOBAL obj, OP_OBJ_FIELD_GET member
        int objIdx = cf.addConstantStringDedup(name.substr(0, dot));
        cf.chunk.writeOp(OpCode::OP_GET_GLOBAL);
        cf.chunk.writeShort(static_cast<uint16_t>(objIdx));
        int memberIdx = cf.addConstantStringDedup(name.substr(dot + 1));
        cf.chunk.writeOp(OpCode::OP_OBJ_FIELD_GET);
        cf.chunk.writeShort(static_cast<uint16_t>(memberIdx));
        return Value::Type::Object;
    }
    int nameIdx = cf.addConstantStringDedup(name);
    cf.chunk.writeOp(OpCode::OP_GET_GLOBAL);
    cf.chunk.writeShort(static_cast<uint16_t>(nameIdx));
    auto it = varTypes.find(name);
    if (it != varTypes.end()) return it->second;
    return Value::Type::Unknown;
}

Value::Type NumberExpr::compileBytecode(CompiledFunction& cf,
    std::unordered_map<std::string, Value::Type>& varTypes) const {
    int idx = cf.chunk.addConstant(Value(value));
    cf.chunk.writeOp(OpCode::OP_CONSTANT);
    cf.chunk.writeShort(static_cast<uint16_t>(idx));
    return Value::Type::Int;
}

Value::Type DoubleExpr::compileBytecode(CompiledFunction& cf,
    std::unordered_map<std::string, Value::Type>& varTypes) const {
    int idx = cf.chunk.addConstant(Value(value));
    cf.chunk.writeOp(OpCode::OP_CONSTANT);
    cf.chunk.writeShort(static_cast<uint16_t>(idx));
    return Value::Type::Double;
}

Value::Type StringExpr::compileBytecode(CompiledFunction& cf,
    std::unordered_map<std::string, Value::Type>& varTypes) const {
    int idx = cf.addConstantStringDedup(value);
    cf.chunk.writeOp(OpCode::OP_CONSTANT);
    cf.chunk.writeShort(static_cast<uint16_t>(idx));
    return Value::Type::String;
}

Value::Type ArrayExpr::compileBytecode(CompiledFunction& cf,
    std::unordered_map<std::string, Value::Type>& varTypes) const {
    int count = 0;
    for (const auto& elem : elements) {
        elem->compileBytecode(cf, varTypes);
        count++;
    }
    cf.chunk.writeOp(OpCode::OP_ARRAY);
    cf.chunk.writeByte(static_cast<uint8_t>(count));
    return Value::Type::Array;
}

Value::Type IndexExpr::compileBytecode(CompiledFunction& cf,
    std::unordered_map<std::string, Value::Type>& varTypes) const {
    arrayExpr->compileBytecode(cf, varTypes);
    indexExpr->compileBytecode(cf, varTypes);
    cf.chunk.writeOp(OpCode::OP_INDEX_GET);
    return Value::Type::Unknown;
}

Value::Type CallExpr::compileBytecode(CompiledFunction& cf,
    std::unordered_map<std::string, Value::Type>& varTypes) const {
    size_t dot = funcName.find('.');
    if (dot != std::string::npos) {
        std::string prefix = funcName.substr(0, dot);
        // Library calls keep the OP_CONSTANT+OP_CALL form; every other dotted
        // name is compiled as an object method call on 'prefix'.
        auto& libMgr = LibraryManager::getInstance();
        std::string resolvedLib = libMgr.resolveAlias(prefix);
        if (!(libMgr.hasLibrary(resolvedLib) && libMgr.isImported(resolvedLib))) {
            int objIdx = cf.addConstantStringDedup(prefix);
            cf.chunk.writeOp(OpCode::OP_GET_GLOBAL);
            cf.chunk.writeShort(static_cast<uint16_t>(objIdx));
            for (const auto& arg : args) {
                arg->compileBytecode(cf, varTypes);
            }
            int memberIdx = cf.addConstantStringDedup(funcName.substr(dot + 1));
            cf.chunk.writeOp(OpCode::OP_OBJ_CALL);
            cf.chunk.writeShort(static_cast<uint16_t>(memberIdx));
            cf.chunk.writeByte(static_cast<uint8_t>(args.size()));
            return Value::Type::Object;
        }
    }
    for (const auto& arg : args) {
        arg->compileBytecode(cf, varTypes);
    }
    int nameIdx = cf.addConstantStringDedup(funcName);
    cf.chunk.writeOp(OpCode::OP_CONSTANT);
    cf.chunk.writeShort(static_cast<uint16_t>(nameIdx));
    cf.chunk.writeOp(OpCode::OP_CALL);
    cf.chunk.writeByte(static_cast<uint8_t>(args.size()));
    return Value::Type::Unknown;
}

Value::Type BinaryExpr::compileBytecode(CompiledFunction& cf,
    std::unordered_map<std::string, Value::Type>& varTypes) const {
    Value::Type leftType = this->left->compileBytecode(cf, varTypes);
    Value::Type rightType = this->right->compileBytecode(cf, varTypes);
    if (leftType != Value::Type::Unknown && rightType != Value::Type::Unknown &&
        leftType != Value::Type::Void && rightType != Value::Type::Void) {
        if ((leftType == Value::Type::Double && rightType == Value::Type::Int) ||
            (leftType == Value::Type::Int && rightType == Value::Type::Double)) {
            std::string opName;
            switch (op) {
            case TOKEN_PLUS: opName = "add"; break;
            case TOKEN_MINUS: opName = "subtract"; break;
            case TOKEN_MUL: opName = "multiply"; break;
            case TOKEN_DIV: opName = "divide"; break;
            case TOKEN_MOD: opName = "modulo"; break;
            default: opName = "operate on"; break;
            }
            throw std::runtime_error("TypeError: Cannot " + opName + " int and double without explicit cast");
        }
        if (leftType == Value::Type::String && rightType != Value::Type::String) {
            throw std::runtime_error("TypeError: Cannot add string and non-string (func="
                + cf.name + " op=" + std::to_string(op) + ")");
        }
        if (leftType != Value::Type::String && rightType == Value::Type::String) {
            throw std::runtime_error("TypeError: Cannot add non-string and string");
        }
    }
    OpCode opcode;
    switch (op) {
    case TOKEN_PLUS: opcode = OpCode::OP_ADD; break;
    case TOKEN_MINUS: opcode = OpCode::OP_SUB; break;
    case TOKEN_MUL: opcode = OpCode::OP_MUL; break;
    case TOKEN_DIV: opcode = OpCode::OP_DIV; break;
    case TOKEN_MOD: opcode = OpCode::OP_MOD; break;
    default: throw std::runtime_error("BytecodeCompiler: unsupported binary operator");
    }
    cf.chunk.writeOp(opcode);
    if (leftType == Value::Type::Double || rightType == Value::Type::Double)
        return Value::Type::Double;
    if (leftType == Value::Type::Int && rightType == Value::Type::Int)
        return Value::Type::Int;
    if (leftType == Value::Type::String && rightType == Value::Type::String)
        return Value::Type::String;
    return Value::Type::Unknown;
}

Value::Type InputExpr::compileBytecode(CompiledFunction& cf,
    std::unordered_map<std::string, Value::Type>& varTypes) const {
    cf.chunk.writeOp(OpCode::OP_INPUT);
    return Value::Type::Unknown;
}

Value::Type NewExpr::compileBytecode(CompiledFunction& cf,
    std::unordered_map<std::string, Value::Type>& varTypes) const {
    sizeExpr->compileBytecode(cf, varTypes);
    cf.chunk.writeOp(OpCode::OP_NEW);
    return Value::Type::Bytes;
}

Value::Type ObjectNewExpr::compileBytecode(CompiledFunction& cf,
    std::unordered_map<std::string, Value::Type>& varTypes) const {
    for (const auto& arg : args) {
        arg->compileBytecode(cf, varTypes);
    }
    int classIdx = cf.addConstantStringDedup(className);
    cf.chunk.writeOp(OpCode::OP_NEW_OBJ);
    cf.chunk.writeShort(static_cast<uint16_t>(classIdx));
    cf.chunk.writeByte(static_cast<uint8_t>(args.size()));
    return Value::Type::Object;
}

Value::Type UnaryExpr::compileBytecode(CompiledFunction& cf,
    std::unordered_map<std::string, Value::Type>& varTypes) const {
    operand->compileBytecode(cf, varTypes);
    if (op == TOKEN_NOT) {
        cf.chunk.writeOp(OpCode::OP_NOT);
        return Value::Type::Int;
    }
    if (op == TOKEN_MINUS) {
        cf.chunk.writeOp(OpCode::OP_NEGATE);
        return Value::Type::Unknown;
    }
    throw std::runtime_error("BytecodeCompiler: unsupported unary operator");
}

Value::Type CastExpr::compileBytecode(CompiledFunction& cf,
    std::unordered_map<std::string, Value::Type>& varTypes) const {
    expr->compileBytecode(cf, varTypes);
    cf.chunk.writeOp((castType == CastType::Int) ? OpCode::OP_CAST_INT : OpCode::OP_CAST_DOUBLE);
    return (castType == CastType::Int) ? Value::Type::Int : Value::Type::Double;
}

Value::Type CompareExpr::compileBytecode(CompiledFunction& cf,
    std::unordered_map<std::string, Value::Type>& varTypes) const {
    Value::Type leftType = left->compileBytecode(cf, varTypes);
    Value::Type rightType = right->compileBytecode(cf, varTypes);
    if (leftType != Value::Type::Unknown && rightType != Value::Type::Unknown &&
        leftType != Value::Type::Void && rightType != Value::Type::Void) {
        if ((leftType == Value::Type::Double && rightType == Value::Type::Int) ||
            (leftType == Value::Type::Int && rightType == Value::Type::Double)) {
            throw std::runtime_error("TypeError: Cannot compare int and double without explicit cast");
        }
    }
    OpCode opcode;
    switch (op) {
    case CompareType::EQ: opcode = OpCode::OP_EQ; break;
    case CompareType::NE: opcode = OpCode::OP_NE; break;
    case CompareType::GT: opcode = OpCode::OP_GT; break;
    case CompareType::LT: opcode = OpCode::OP_LT; break;
    case CompareType::GE: opcode = OpCode::OP_GE; break;
    case CompareType::LE: opcode = OpCode::OP_LE; break;
    }
    cf.chunk.writeOp(opcode);
    return Value::Type::Int;
}

Value::Type ConditionExpr::compileBytecode(CompiledFunction& cf,
    std::unordered_map<std::string, Value::Type>& varTypes) const {
    left->compileBytecode(cf, varTypes);
    if (op == TOKEN_AND) {
        size_t jumpInstr = cf.chunk.code.size();
        cf.chunk.writeOp(OpCode::OP_JMP_IF_FALSE);
        cf.chunk.writeShort(0);
        right->compileBytecode(cf, varTypes);
        size_t endJump = cf.chunk.code.size();
        cf.chunk.writeOp(OpCode::OP_JMP);
        cf.chunk.writeShort(0);
        size_t afterJump = cf.chunk.code.size();
        cf.chunk.writeOp(OpCode::OP_FALSE);
        cf.chunk.patchJump(jumpInstr + 1, afterJump);
        cf.chunk.patchJump(endJump + 1, cf.chunk.code.size());
    } else {
        size_t jumpInstr = cf.chunk.code.size();
        cf.chunk.writeOp(OpCode::OP_JMP_IF_FALSE);
        cf.chunk.writeShort(0);
        cf.chunk.writeOp(OpCode::OP_TRUE);
        size_t endJump = cf.chunk.code.size();
        cf.chunk.writeOp(OpCode::OP_JMP);
        cf.chunk.writeShort(0);
        size_t afterJump = cf.chunk.code.size();
        cf.chunk.patchJump(jumpInstr + 1, afterJump);
        right->compileBytecode(cf, varTypes);
        cf.chunk.patchJump(endJump + 1, cf.chunk.code.size());
    }
    return Value::Type::Int;
}

// ============================================================
// Stmt implementations
// ============================================================

PrintStmt::PrintStmt(std::unique_ptr<Expr> a) : arg(std::move(a)) {}
Value PrintStmt::execute(std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    // Use Value::toString() to match the VM's OP_PRINT formatting (P3-4)
    std::cout << arg->evaluate(variables, functions).toString();
    return Value();
}

PrintlnStmt::PrintlnStmt(std::unique_ptr<Expr> a) : arg(std::move(a)) {}
Value PrintlnStmt::execute(std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    // Use Value::toString() to match the VM's OP_PRINTLN formatting (P3-4)
    std::cout << arg->evaluate(variables, functions).toString() << std::endl;
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

FreeStmt::FreeStmt(const std::string& name) : varName(name) {}
Value FreeStmt::execute(std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    variables.erase(varName);
    return Value();
}

Value FreeAllStmt::execute(std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    variables.clear();
    return Value();
}

RetStmt::RetStmt(std::unique_ptr<Expr> a) : arg(std::move(a)), hasArg(arg != nullptr) {}
Value RetStmt::execute(std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    if (hasArg) {
        return arg->evaluate(variables, functions);
    }
    // Bare "ret" in a void function must still stop execution; the Return
    // marker signals "function is returning" without carrying a value.
    return Value::makeReturnMarker();
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
    Interpreter::currentVariables = &variables;
    Interpreter sys;
    if (sys.isSystemFunction(funcName)) {
        // Statement-level call: discard the result (P0-1) so it does not
        // leak into the enclosing function's return value.
        sys.SystemFunctionBuildIn(funcName, argVals);
        return Value();
    }
    if (functions.find(funcName) == functions.end()) {
        Value methodResult;
        if (callObjectMethod(variables, functions, funcName, argVals, methodResult)) {
            return Value();
        }
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
    // Statement-level call: discard the result (P0-1) so it does not
    // leak into the enclosing function's return value.
    funcInterp.executeFunction(func);
    return Value();
}

AssignStmt::AssignStmt(const std::string& name, std::unique_ptr<Expr> e)
    : varName(name), expr(std::move(e)) {}
Value AssignStmt::execute(std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    Value val = expr->evaluate(variables, functions);
    if (assignObjectMember(variables, varName, val)) {
        return Value();
    }
    variables[varName] = val;
    if (variables[varName].getType() == Value::Type::Bytes) {
        int sz = static_cast<int>(variables[varName].asBytes().size());
        Parser::checkNewAllocBytes(sz); // same limit as the interpreter path (P3-5)
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
        // Object field index write: 'obj.field[idx] = value'
        size_t dot = varName.find('.');
        if (dot != std::string::npos) {
            std::string objName = varName.substr(0, dot);
            std::string fieldName = varName.substr(dot + 1);
            auto varIt = variables.find(objName);
            if (varIt != variables.end() && varIt->second.getType() == Value::Type::Object) {
                std::shared_ptr<Value> member;
                {
                    const auto& members = varIt->second.asObjectDict();
                    auto it = members.find(fieldName);
                    if (it == members.end()) {
                        throw std::runtime_error("Undefined field '" + fieldName + "' in class '" +
                            varIt->second.asObjectClass() + "'");
                    }
                    member = it->second;
                }
                if (member->getType() == Value::Type::Dict) {
                    if (idxVal.getType() != Value::Type::String) {
                        throw std::runtime_error("Dict index must be a string");
                    }
                    member->asDictRef()[idxVal.asString()] = std::make_shared<Value>(val);
                    return Value();
                }
                std::vector<Value>& arr = member->asArrayRef();
                int idx = idxVal.asInt();
                if (idx < 0 || idx >= static_cast<int>(arr.size())) {
                    throw std::runtime_error("Array index out of bounds: " + std::to_string(idx));
                }
                arr[idx] = val;
                return Value();
            }
        }
        throw std::runtime_error("Undefined array or dict variable: " + varName);
    }
    if (variables[varName].getType() == Value::Type::Dict) {
        if (idxVal.getType() != Value::Type::String) {
            throw std::runtime_error("Dict index must be a string");
        }
        variables[varName].asDictRef()[idxVal.asString()] = std::make_shared<Value>(val);
        return Value();
    }
    std::vector<Value>& arr = variables[varName].asArrayRef();
    int idx = idxVal.asInt();
    if (idx < 0 || idx >= static_cast<int>(arr.size())) {
        throw std::runtime_error("Array index out of bounds: " + std::to_string(idx));
    }
    arr[idx] = val;
    return Value();
}

IfStmt::IfStmt(const std::string& cond, std::vector<std::unique_ptr<Stmt>>&& b,
               std::vector<std::unique_ptr<Stmt>>&& eb)
    : condition(cond), body(std::move(b)), elseBody(std::move(eb)) {}
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
    } else {
        for (const auto& stmt : elseBody) {
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
        try {
            for (const auto& stmt : body) {
                if (!stmt) continue;
                Value val = stmt->execute(variables, functions);
                if (val.getType() != Value::Type::Void) {
                    return val;
                }
            }
        } catch (const BreakException&) {
            break;
        } catch (const ContinueException&) {
            // fall through to condition re-evaluation
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
        }
        // Empty condition = infinite loop (C semantics, aligns with bytecode OP_TRUE)
        try {
            for (const auto& stmt : body) {
                if (!stmt) continue;
                Value val = stmt->execute(variables, functions);
                if (val.getType() != Value::Type::Void) {
                    return val;
                }
            }
        } catch (const BreakException&) {
            break;
        } catch (const ContinueException&) {
            // execute iterator below
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

Value BreakStmt::execute(std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    throw BreakException();
}

Value ContinueStmt::execute(std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    throw ContinueException();
}

ErrorStmt::ErrorStmt(std::unique_ptr<Expr> m) : message(std::move(m)) {}
Value ErrorStmt::execute(std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    Value msg = message->evaluate(variables, functions);
    if (msg.getType() != Value::Type::String) {
        throw std::runtime_error("error() message must be a string");
    }
    throw LangErrorException(msg.asString());
}

TryStmt::TryStmt(const std::string& var, std::vector<std::unique_ptr<Stmt>>&& b,
                 std::vector<std::unique_ptr<Stmt>>&& cb)
    : errorVar(var), body(std::move(b)), catchBody(std::move(cb)) {}
Value TryStmt::execute(std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    try {
        for (const auto& stmt : body) {
            if (!stmt) continue;
            Value val = stmt->execute(variables, functions);
            if (val.getType() != Value::Type::Void) {
                return val;
            }
        }
    } catch (const BreakException&) {
        throw;
    } catch (const ContinueException&) {
        throw;
    } catch (const std::exception& e) {
        variables[errorVar] = Value(e.what());
        for (const auto& stmt : catchBody) {
            if (!stmt) continue;
            Value val = stmt->execute(variables, functions);
            if (val.getType() != Value::Type::Void) {
                return val;
            }
        }
    }
    return Value();
}
