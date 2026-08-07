#pragma once
#include "../util/common.hpp"
#include "../frontend/value.hpp"
#include "../frontend/function.hpp"
#include "../frontend/token.hpp"
#include "../frontend/lexer.hpp"
#include "../frontend/parser.hpp"
#include "../interpreter/interpreter.hpp"
#include "../interpreter/library_manager.hpp"
#include <vector>
#include <string>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <cstring>
#include <cmath>

// ============================================================
// Serialization helpers
// ============================================================
inline void writeUint32(std::vector<uint8_t>& data, uint32_t v) {
    data.push_back(static_cast<uint8_t>(v & 0xFF));
    data.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    data.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}

inline uint32_t readUint32(const uint8_t* data, size_t size, size_t& offset) {
    if (offset + 4 > size) return 0;
    uint32_t v = static_cast<uint32_t>(data[offset]) |
                 (static_cast<uint32_t>(data[offset + 1]) << 8) |
                 (static_cast<uint32_t>(data[offset + 2]) << 16) |
                 (static_cast<uint32_t>(data[offset + 3]) << 24);
    offset += 4;
    return v;
}

// Varint encoding for compact serialization (saves ~40% on .fc files)
inline void writeVarint(std::vector<uint8_t>& data, uint32_t v) {
    while (v >= 0x80) {
        data.push_back(static_cast<uint8_t>((v & 0x7F) | 0x80));
        v >>= 7;
    }
    data.push_back(static_cast<uint8_t>(v));
}

inline uint32_t readVarint(const uint8_t* data, size_t size, size_t& offset) {
    uint32_t v = 0;
    int shift = 0;
    while (true) {
        if (offset >= size) return 0;
        uint8_t b = data[offset++];
        v |= static_cast<uint32_t>(b & 0x7F) << shift;
        if (!(b & 0x80)) break;
        shift += 7;
        if (shift >= 28) return v;
    }
    return v;
}

inline void writeString(std::vector<uint8_t>& data, const std::string& s) {
    writeVarint(data, static_cast<uint32_t>(s.size()));
    for (char c : s) data.push_back(static_cast<uint8_t>(c));
}

inline std::string readString(const uint8_t* data, size_t size, size_t& offset) {
    uint32_t len = readVarint(data, size, offset);
    if (offset + len > size) return "";
    std::string s(reinterpret_cast<const char*>(data + offset), len);
    offset += len;
    return s;
}

// ============================================================
// Opcodes
// ============================================================
enum class OpCode : uint8_t {
    OP_RETURN          = 0x00,
    OP_CONSTANT        = 0x01,
    OP_NEGATE          = 0x02,
    OP_ADD             = 0x03,
    OP_SUB             = 0x04,
    OP_MUL             = 0x05,
    OP_DIV             = 0x06,
    OP_TRUE            = 0x07,
    OP_FALSE           = 0x08,
    OP_NIL             = 0x09,
    OP_NOT             = 0x0A,
    OP_EQ              = 0x0B,
    OP_NE              = 0x0C,
    OP_GT              = 0x0D,
    OP_LT              = 0x0E,
    OP_GE              = 0x0F,
    OP_LE              = 0x10,
    OP_PRINT           = 0x11,
    OP_PRINTLN         = 0x12,
    OP_POP             = 0x13,
    OP_DEF_GLOBAL      = 0x14,
    OP_GET_GLOBAL      = 0x15,
    OP_SET_GLOBAL      = 0x16,
    OP_GET_LOCAL       = 0x17,
    OP_SET_LOCAL       = 0x18,
    OP_JMP             = 0x19,
    OP_JMP_IF_FALSE    = 0x1A,
    OP_LOOP            = 0x1B,
    OP_CALL            = 0x1C,
    OP_INPUT           = 0x1D,
    OP_CAST_INT        = 0x1E,
    OP_CAST_DOUBLE     = 0x1F,
    OP_ARRAY           = 0x20,
    OP_INDEX_GET       = 0x21,
    OP_INDEX_SET       = 0x22,
    OP_AND             = 0x23,
    OP_OR              = 0x24,
    OP_ENDLN           = 0x25,
    OP_EXIT            = 0x26,
    OP_IMPORT          = 0x27,
    OP_NEW             = 0x28,
    OP_UNSET_GLOBAL    = 0x29,
    OP_CLEAR_GLOBALS   = 0x2A,
    OP_HALT            = 0xFF,
};

// ============================================================
// Chunk - a sequence of bytecode instructions
// ============================================================
struct Chunk {
    std::vector<uint8_t> code;
    std::vector<Value> constants;

    void write(uint8_t byte);
    void writeOp(OpCode op);
    void writeInt(uint32_t val);
    void writeShort(uint16_t val);
    void writeByte(uint8_t val);

    uint32_t readInt(size_t offset) const;
    uint16_t readShort(size_t offset) const;
    uint8_t readByte(size_t offset) const;

    int addConstant(const Value& val);
    int addConstantString(const std::string& str);
    size_t addConstantDouble(double val);
    size_t addConstantInt(int val);

    void patchJump(size_t offset, size_t target);
    void patchJumpOffset(size_t jumpInstrOffset, int32_t offset);

    std::vector<uint8_t> serialize() const;
    bool deserialize(const std::vector<uint8_t>& data);
    bool deserialize(const uint8_t* data, size_t size, size_t& offset);
    size_t serializedSize() const;
};

// ============================================================
// CompiledFunction - bytecode for a single function
// ============================================================
struct CompiledFunction {
    std::string name;
    std::string returnType;
    std::vector<Parameter> parameters;
    Chunk chunk;
    int localCount = 0;
    std::unordered_map<std::string, int> stringConstMap;

    int addConstantStringDedup(const std::string& str) {
        auto it = stringConstMap.find(str);
        if (it != stringConstMap.end()) return it->second;
        int idx = chunk.addConstantString(str);
        stringConstMap[str] = idx;
        return idx;
    }
};

// ============================================================
// CompiledProgram - all functions compiled to bytecode
// ============================================================
struct ImportEntry {
    std::string libName;
    std::string alias;
};

struct CompiledProgram {
    std::vector<CompiledFunction> functions;
    std::unordered_map<std::string, size_t> functionIndex;
    std::vector<ImportEntry> imports;

    std::vector<uint8_t> serialize() const;
    static CompiledProgram deserialize(const std::vector<uint8_t>& data);
    static CompiledProgram deserialize(const uint8_t* data, size_t size);
    void restoreImports() const;
};

// ============================================================
// Disassembler - human-readable dump of .fc bytecode
// ============================================================
void disassembleProgram(const CompiledProgram& prog, std::ostream& out);

// ============================================================
// BytecodeCompiler - compiles FoxLang source to bytecode
// ============================================================
class BytecodeCompiler {
public:
    CompiledProgram compile(const std::string& source, const std::string& filename = "");
    Value::Type compileExpr(CompiledFunction& cf, Expr* expr);

    bool validateCall(const std::string& name);

private:
    CompiledProgram program;
    std::unordered_map<std::string, Value::Type> varTypes;
    std::unordered_set<std::string> userFuncNames;

    static std::string typeStr(Value::Type t);
    static void typeError(const std::string& msg);

    void compileFunctionBody(CompiledFunction& cf, const std::vector<std::string>& body);
    static void skipWhitespace(Lexer& lexer, Token& token);
};

// ============================================================
// VM - executes CompiledProgram bytecode
// ============================================================
class VM {
public:
    VM();
    void loadProgram(const CompiledProgram& prog);
    void run();

private:
    struct CallFrame {
        const CompiledFunction* function;
        std::vector<Value> locals;
        std::unordered_map<std::string, Value> savedGlobals;
        std::vector<std::string> newGlobals;
        size_t ip;
        size_t instrStart = 0; // offset of the instruction being executed
        int newAllocBytes = 0; // per-function new() budget (P3-5)
    };

    CompiledProgram program;
    std::vector<Value> stack;
    std::unordered_map<std::string, Value> globals;
    std::vector<CallFrame> frames;
    bool runtimeError;

    Value peek(int distance = 0) {
        if (stack.empty()) {
            runtimeErr("Stack empty");
            return Value();
        }
        return stack[stack.size() - 1 - distance];
    }
    Value pop() {
        if (stack.empty()) {
            runtimeErr("Stack underflow");
            return Value();
        }
        Value val = stack.back();
        stack.pop_back();
        return val;
    }
    void push(const Value& val) {
        stack.push_back(val);
    }
    void resetStack();

    void runtimeErr(const std::string& msg);

    bool callFunction(const std::string& name, int argCount);
    bool callSystemFunction(const std::string& name, int argCount);

    Value executeSystemCall(const std::string& funcName, const std::vector<Value>& args);
};
