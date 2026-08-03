#include "bytecode.hpp"
#include <typeinfo>
#include "../util/error_reporter.hpp"

// This VM name is FVM

// ============================================================
// Chunk implementation
// ============================================================
void Chunk::write(uint8_t byte) {
    code.push_back(byte);
}

void Chunk::writeOp(OpCode op) {
    write(static_cast<uint8_t>(op));
}

void Chunk::writeInt(uint32_t val) {
    write(static_cast<uint8_t>(val & 0xFF));
    write(static_cast<uint8_t>((val >> 8) & 0xFF));
    write(static_cast<uint8_t>((val >> 16) & 0xFF));
    write(static_cast<uint8_t>((val >> 24) & 0xFF));
}

void Chunk::writeShort(uint16_t val) {
    write(static_cast<uint8_t>(val & 0xFF));
    write(static_cast<uint8_t>((val >> 8) & 0xFF));
}

void Chunk::writeByte(uint8_t val) {
    write(val);
}

uint32_t Chunk::readInt(size_t offset) const {
    if (offset + 4 > code.size()) return 0;
    return static_cast<uint32_t>(code[offset]) |
           (static_cast<uint32_t>(code[offset + 1]) << 8) |
           (static_cast<uint32_t>(code[offset + 2]) << 16) |
           (static_cast<uint32_t>(code[offset + 3]) << 24);
}

uint16_t Chunk::readShort(size_t offset) const {
    if (offset + 2 > code.size()) return 0;
    return static_cast<uint16_t>(code[offset]) |
           (static_cast<uint16_t>(code[offset + 1]) << 8);
}

uint8_t Chunk::readByte(size_t offset) const {
    if (offset >= code.size()) return 0;
    return code[offset];
}

int Chunk::addConstant(const Value& val) {
    constants.push_back(val);
    return static_cast<int>(constants.size() - 1);
}

int Chunk::addConstantString(const std::string& str) {
    return addConstant(Value(str));
}

size_t Chunk::addConstantDouble(double val) {
    constants.push_back(Value(val));
    return constants.size() - 1;
}

size_t Chunk::addConstantInt(int val) {
    constants.push_back(Value(val));
    return constants.size() - 1;
}

void Chunk::patchJump(size_t offset, size_t target) {
    int32_t jumpOffset = static_cast<int32_t>(target - offset - 2);
    code[offset]     = static_cast<uint8_t>(jumpOffset & 0xFF);
    code[offset + 1] = static_cast<uint8_t>((jumpOffset >> 8) & 0xFF);
}

void Chunk::patchJumpOffset(size_t jumpInstrOffset, int32_t offset) {
    code[jumpInstrOffset]     = static_cast<uint8_t>(offset & 0xFF);
    code[jumpInstrOffset + 1] = static_cast<uint8_t>((offset >> 8) & 0xFF);
}

// ============================================================
// Value serialization helpers (recursive; arrays embed elements)
// ============================================================
namespace {
void writeValue(std::vector<uint8_t>& data, const Value& v) {
    switch (v.getType()) {
    case Value::Type::Int: {
        data.push_back(0);
        writeVarint(data, static_cast<uint32_t>(v.asInt()));
        break;
    }
    case Value::Type::Double: {
        data.push_back(1);
        double dv = v.asDouble();
        uint64_t bits;
        std::memcpy(&bits, &dv, sizeof(bits));
        for (int i = 0; i < 8; i++) {
            data.push_back(static_cast<uint8_t>((bits >> (i * 8)) & 0xFF));
        }
        break;
    }
    case Value::Type::String: {
        data.push_back(2);
        writeString(data, v.asString());
        break;
    }
    case Value::Type::Array: {
        data.push_back(3);
        const auto& arr = v.asArray();
        writeVarint(data, static_cast<uint32_t>(arr.size()));
        for (const auto& elem : arr) {
            writeValue(data, elem);
        }
        break;
    }
    case Value::Type::Void: {
        data.push_back(4);
        break;
    }
    case Value::Type::Bytes: {
        data.push_back(5);
        const auto& bv = v.asBytes();
        writeVarint(data, static_cast<uint32_t>(bv.size()));
        for (uint8_t byte : bv) data.push_back(byte);
        break;
    }
    default:
        data.push_back(4);
        break;
    }
}

bool readValue(const uint8_t* data, size_t size, size_t& offset, Value& out) {
    if (offset >= size) return false;
    uint8_t type = data[offset++];
    switch (type) {
    case 0: { // int
        out = Value(static_cast<int32_t>(readVarint(data, size, offset)));
        return true;
    }
    case 1: { // double
        if (offset + 8 > size) return false;
        uint64_t bits = 0;
        for (int j = 0; j < 8; j++) {
            bits |= (static_cast<uint64_t>(data[offset + j]) << (j * 8));
        }
        offset += 8;
        double dv;
        std::memcpy(&dv, &bits, sizeof(dv));
        out = Value(dv);
        return true;
    }
    case 2: { // string
        out = Value(readString(data, size, offset));
        return true;
    }
    case 3: { // array
        uint32_t arrLen = readVarint(data, size, offset);
        std::vector<Value> arr;
        arr.reserve(arrLen);
        for (uint32_t j = 0; j < arrLen; j++) {
            Value elem;
            if (!readValue(data, size, offset, elem)) return false;
            arr.push_back(std::move(elem));
        }
        out = Value(arr);
        return true;
    }
    case 4: { // void
        out = Value();
        return true;
    }
    case 5: { // bytes
        uint32_t bytesLen = readVarint(data, size, offset);
        if (offset + bytesLen > size) return false;
        std::vector<uint8_t> bv(data + offset, data + offset + bytesLen);
        offset += bytesLen;
        out = Value(bv);
        return true;
    }
    default:
        out = Value();
        return true;
    }
}
} // namespace

std::vector<uint8_t> Chunk::serialize() const {
    std::vector<uint8_t> data;

    // Constants
    writeVarint(data, static_cast<uint32_t>(constants.size()));
    for (const auto& v : constants) {
        writeValue(data, v);
    }

    // Code
    writeVarint(data, static_cast<uint32_t>(code.size()));
    data.insert(data.end(), code.begin(), code.end());

    return data;
}

bool Chunk::deserialize(const std::vector<uint8_t>& data) {
    size_t offset = 0;
    return deserialize(data.data(), data.size(), offset);
}

bool Chunk::deserialize(const uint8_t* data, size_t size, size_t& offset) {
    // Constants
    uint32_t constCount = readVarint(data, size, offset);
    constants.clear();
    for (uint32_t i = 0; i < constCount; i++) {
        Value v;
        if (!readValue(data, size, offset, v)) return false;
        constants.push_back(std::move(v));
    }

    // Code
    uint32_t codeSize = readVarint(data, size, offset);
    code.clear();
    if (offset + codeSize > size) return false;
    code.insert(code.end(), data + offset, data + offset + codeSize);
    offset += codeSize;

    return true;
}

size_t Chunk::serializedSize() const {
    std::vector<uint8_t> dummy;
    writeVarint(dummy, static_cast<uint32_t>(constants.size()));
    for (const auto& v : constants) {
        writeValue(dummy, v);
    }
    writeVarint(dummy, static_cast<uint32_t>(code.size()));
    dummy.insert(dummy.end(), code.begin(), code.end());
    return dummy.size();
}

// ============================================================
// CompiledProgram serialization
// ============================================================
std::vector<uint8_t> CompiledProgram::serialize() const {
    std::vector<uint8_t> data;

    // Magic "FOXC"
    data.push_back('F'); data.push_back('O'); data.push_back('X'); data.push_back('C');
    // Version
    writeUint32(data, 1);

    // Import entries
    writeVarint(data, static_cast<uint32_t>(imports.size()));
    for (const auto& imp : imports) {
        writeString(data, imp.libName);
        writeString(data, imp.alias);
    }

    // Number of functions
    writeVarint(data, static_cast<uint32_t>(functions.size()));

    for (const auto& func : functions) {
        writeString(data, func.name);
        writeString(data, func.returnType);

        // Parameters
        writeVarint(data, static_cast<uint32_t>(func.parameters.size()));
        for (const auto& param : func.parameters) {
            writeString(data, param.name);
            writeString(data, param.type);
        }

        // Local count
        writeVarint(data, static_cast<uint32_t>(func.localCount));

        // Chunk
        std::vector<uint8_t> chunkData = func.chunk.serialize();
        data.insert(data.end(), chunkData.begin(), chunkData.end());
    }

    return data;
}

static CompiledProgram deserializeRaw(const uint8_t* data, size_t size) {
    CompiledProgram prog;
    size_t offset = 0;

    if (offset + 4 > size) {
        throw std::runtime_error("Invalid .fc file: too short");
    }
    if (data[offset] != 'F' || data[offset+1] != 'O' || data[offset+2] != 'X' || data[offset+3] != 'C') {
        throw std::runtime_error("Invalid .fc file: bad magic");
    }
    offset += 4;

    uint32_t version = readUint32(data, size, offset);
    if (version != 1) {
        throw std::runtime_error("Unsupported .fc version: " + std::to_string(version));
    }

    uint32_t importCount = readVarint(data, size, offset);
    for (uint32_t i = 0; i < importCount; i++) {
        ImportEntry ie;
        ie.libName = readString(data, size, offset);
        ie.alias = readString(data, size, offset);
        prog.imports.push_back(ie);
    }
    prog.restoreImports();

    uint32_t funcCount = readVarint(data, size, offset);

    for (uint32_t i = 0; i < funcCount; i++) {
        CompiledFunction cf;
        cf.name = readString(data, size, offset);
        cf.returnType = readString(data, size, offset);

        uint32_t paramCount = readVarint(data, size, offset);
        for (uint32_t j = 0; j < paramCount; j++) {
            Parameter p;
            p.name = readString(data, size, offset);
            p.type = readString(data, size, offset);
            cf.parameters.push_back(p);
        }

        cf.localCount = static_cast<int>(readVarint(data, size, offset));

        if (!cf.chunk.deserialize(data, size, offset)) {
            throw std::runtime_error("Corrupt .fc: chunk deserialization failed");
        }

        prog.functions.push_back(cf);
        prog.functionIndex[cf.name] = i;
    }

    return prog;
}

CompiledProgram CompiledProgram::deserialize(const std::vector<uint8_t>& data) {
    return deserializeRaw(data.data(), data.size());
}

CompiledProgram CompiledProgram::deserialize(const uint8_t* data, size_t size) {
    return deserializeRaw(data, size);
}

void CompiledProgram::restoreImports() const {
    auto& libMgr = LibraryManager::getInstance();
    for (const auto& imp : imports) {
        libMgr.markImported(imp.libName, imp.alias);
    }
}

// ============================================================
// BytecodeCompiler implementation
// ============================================================

void BytecodeCompiler::skipWhitespace(Lexer& lexer, Token& token) {
    while (token.type != TOKEN_EOF && !token.value.empty()
        && token.type != TOKEN_STRING
        && isspace(static_cast<unsigned char>(token.value[0]))
        && token.value[0] != '\n') {
        token = lexer.nextToken();
    }
}

CompiledProgram BytecodeCompiler::compile(const std::string& source, const std::string& filename) {
    program = CompiledProgram();

    // Initialize system libraries before parsing (needed for import resolution)
    RegFunc();

    // Use existing parser to get function definitions
    std::unordered_map<std::string, Value> dummyVars;
    std::unordered_map<std::string, Function> parsedFunctions;

    Parser parser(source, dummyVars, parsedFunctions);
    parser.parseAllFunctions();

    // Record import aliases from LibraryManager (populated by Parser's parseImportStatement)
    auto& libMgr = LibraryManager::getInstance();
    for (const auto& [alias, libName] : libMgr.getAliasMap()) {
        ImportEntry ie;
        ie.libName = libName;
        ie.alias = alias;
        program.imports.push_back(ie);
    }

    // Collect user-defined function names for call validation
    for (const auto& [funcName, func] : parsedFunctions) {
        userFuncNames.insert(funcName);
    }

    // Compile each function
    for (const auto& [funcName, func] : parsedFunctions) {
        CompiledFunction cf;
        cf.name = func.name;
        cf.returnType = func.returnType;
        cf.parameters = func.parameters;
        cf.localCount = static_cast<int>(func.parameters.size()); // params are locals

        // Reserve slots for parameters as locals
        for (size_t i = 0; i < func.parameters.size(); i++) {
            // Opcodes for locals use slot indices 0..N-1
        }

        compileFunctionBody(cf, func.body);

        // Ensure every function ends with a return instruction.
        // NOTE: cannot use code.back() here: the operand byte of a 0-arg OP_CALL
        // is 0x00, which collides with the OP_RETURN opcode (P0-2). Appending
        // unconditionally is safe — an already-emitted OP_RETURN pops the frame,
        // so a trailing extra OP_RETURN is dead code.
        cf.chunk.writeOp(OpCode::OP_RETURN);

        program.functions.push_back(cf);
        program.functionIndex[cf.name] = program.functions.size() - 1;
    }

    return program;
}

void BytecodeCompiler::compileFunctionBody(CompiledFunction& cf, const std::vector<std::string>& body) {
    std::unordered_map<std::string, size_t> labelAddresses;
    std::vector<std::pair<std::string, size_t>> gotoFixups;

    class BytecodeStmtHandler : public StmtHandler {
    public:
        CompiledFunction& cf;
        std::unordered_map<std::string, Value::Type>& varTypes;
        BytecodeCompiler& compiler;
        std::unordered_map<std::string, size_t>& labelAddresses;
        std::vector<std::pair<std::string, size_t>>& gotoFixups;
        BytecodeStmtHandler(CompiledFunction& c, std::unordered_map<std::string, Value::Type>& vt, BytecodeCompiler& comp,
            std::unordered_map<std::string, size_t>& labels,
            std::vector<std::pair<std::string, size_t>>& fixups)
            : cf(c), varTypes(vt), compiler(comp), labelAddresses(labels), gotoFixups(fixups) {}

        void onPrint(std::unique_ptr<Expr> arg) override {
            arg->compileBytecode(cf, varTypes);
            cf.chunk.writeOp(OpCode::OP_PRINT);
        }
        void onPrintln(std::unique_ptr<Expr> arg) override {
            arg->compileBytecode(cf, varTypes);
            cf.chunk.writeOp(OpCode::OP_PRINTLN);
        }
        void onExit(std::unique_ptr<Expr> arg) override {
            arg->compileBytecode(cf, varTypes);
            cf.chunk.writeOp(OpCode::OP_EXIT);
        }
        void onFree(const std::string& varName) override {
            int nameIdx = cf.addConstantStringDedup(varName);
            cf.chunk.writeOp(OpCode::OP_UNSET_GLOBAL);
            cf.chunk.writeShort(static_cast<uint16_t>(nameIdx));
        }
        void onFreeAll() override {
            cf.chunk.writeOp(OpCode::OP_CLEAR_GLOBALS);
        }
        Value onRet(std::unique_ptr<Expr> arg) override {
            if (arg) arg->compileBytecode(cf, varTypes);
            cf.chunk.writeOp(OpCode::OP_RETURN);
            return Value();
        }
        void onEndl() override {
            cf.chunk.writeOp(OpCode::OP_ENDLN);
        }
        void onInput(const std::string& varName) override {
            cf.chunk.writeOp(OpCode::OP_INPUT);
            int nameIdx = cf.addConstantStringDedup(varName);
            cf.chunk.writeOp(OpCode::OP_DEF_GLOBAL);
            cf.chunk.writeShort(static_cast<uint16_t>(nameIdx));
        }
        void onCall(const std::string& name, std::vector<std::unique_ptr<Expr>> args) override {
            if (!compiler.validateCall(name)) {
                throw std::runtime_error(""); // Error already reported via ErrorReporter
            }
            for (auto& arg : args) {
                arg->compileBytecode(cf, varTypes);
            }
            int nameIdx = cf.addConstantStringDedup(name);
            cf.chunk.writeOp(OpCode::OP_CONSTANT);
            cf.chunk.writeShort(static_cast<uint16_t>(nameIdx));
            cf.chunk.writeOp(OpCode::OP_CALL);
            cf.chunk.writeByte(static_cast<uint8_t>(args.size()));
            cf.chunk.writeOp(OpCode::OP_POP);
        }
        void onAssign(const std::string& name, std::unique_ptr<Expr> expr) override {
            Value::Type rhsType = expr->compileBytecode(cf, varTypes);
            varTypes[name] = rhsType;
            int nameIdx = cf.addConstantStringDedup(name);
            cf.chunk.writeOp(OpCode::OP_DEF_GLOBAL);
            cf.chunk.writeShort(static_cast<uint16_t>(nameIdx));
        }
        void onIndexAssign(const std::string& name, std::unique_ptr<Expr> index, std::unique_ptr<Expr> value) override {
            value->compileBytecode(cf, varTypes);
            int nameIdx = cf.addConstantStringDedup(name);
            cf.chunk.writeOp(OpCode::OP_GET_GLOBAL);
            cf.chunk.writeShort(static_cast<uint16_t>(nameIdx));
            index->compileBytecode(cf, varTypes);
            cf.chunk.writeOp(OpCode::OP_INDEX_SET);
            cf.chunk.writeOp(OpCode::OP_DEF_GLOBAL);
            cf.chunk.writeShort(static_cast<uint16_t>(nameIdx));
        }
        void onIf(IfStatement ifStmt) override {
            Lexer condLexer(ifStmt.condition);
            Token condToken = condLexer.nextToken();
            BytecodeCompiler::skipWhitespace(condLexer, condToken);
            auto condExpr = Parser::parseExpr(condLexer, condToken);
            condExpr->compileBytecode(cf, varTypes);

            size_t jumpInstr = cf.chunk.code.size();
            cf.chunk.writeOp(OpCode::OP_JMP_IF_FALSE);
            cf.chunk.writeShort(0);

            for (const auto& stmt : ifStmt.body) {
                Parser::parseLine(stmt, *this);
            }

            size_t afterBody = cf.chunk.code.size();
            cf.chunk.patchJump(jumpInstr + 1, afterBody);
        }
        void onWhile(WhileStatement whileStmt) override {
            size_t loopStart = cf.chunk.code.size();

            Lexer condLexer(whileStmt.condition);
            Token condToken = condLexer.nextToken();
            BytecodeCompiler::skipWhitespace(condLexer, condToken);
            auto condExpr = Parser::parseExpr(condLexer, condToken);
            condExpr->compileBytecode(cf, varTypes);

            size_t exitJump = cf.chunk.code.size();
            cf.chunk.writeOp(OpCode::OP_JMP_IF_FALSE);
            cf.chunk.writeShort(0);

            for (const auto& stmt : whileStmt.body) {
                Parser::parseLine(stmt, *this);
            }

            size_t afterBody = cf.chunk.code.size();
            cf.chunk.writeOp(OpCode::OP_LOOP);
            int32_t loopOffset = static_cast<int32_t>(loopStart) - static_cast<int32_t>(afterBody + 3);
            cf.chunk.writeShort(static_cast<uint16_t>(static_cast<int32_t>(loopOffset)));
            cf.chunk.patchJump(exitJump + 1, afterBody + 3);
        }
        void onFor(ForStatement forStmt) override {
            if (!forStmt.init.empty()) {
                Parser::parseLine(forStmt.init, *this);
            }

            size_t loopStart = cf.chunk.code.size();

            if (!forStmt.condition.empty()) {
                Lexer condLexer(forStmt.condition);
                Token condToken = condLexer.nextToken();
                BytecodeCompiler::skipWhitespace(condLexer, condToken);
                auto condExpr = Parser::parseExpr(condLexer, condToken);
                condExpr->compileBytecode(cf, varTypes);
            } else {
                cf.chunk.writeOp(OpCode::OP_TRUE);
            }

            size_t exitJump = cf.chunk.code.size();
            cf.chunk.writeOp(OpCode::OP_JMP_IF_FALSE);
            cf.chunk.writeShort(0);

            for (const auto& stmt : forStmt.body) {
                Parser::parseLine(stmt, *this);
            }

            if (!forStmt.iter.empty()) {
                Parser::parseLine(forStmt.iter, *this);
            }

            size_t afterBody = cf.chunk.code.size();
            cf.chunk.writeOp(OpCode::OP_LOOP);
            int32_t loopOffset = static_cast<int32_t>(loopStart) - static_cast<int32_t>(afterBody + 3);
            cf.chunk.writeShort(static_cast<uint16_t>(static_cast<int32_t>(loopOffset)));
            cf.chunk.patchJump(exitJump + 1, afterBody + 3);
        }
        void onFnLabel(const std::string& name) override {
            labelAddresses[name] = cf.chunk.code.size();
        }
        void onGoto(const std::string& name) override {
            size_t jumpAddrPos = cf.chunk.code.size() + 1;
            cf.chunk.writeOp(OpCode::OP_JMP);
            cf.chunk.writeShort(0);
            gotoFixups.push_back({name, jumpAddrPos});
        }
    };

    BytecodeStmtHandler handler(cf, varTypes, *this, labelAddresses, gotoFixups);

    for (const auto& line : body) {
        if (line.empty()) continue;

        // Check for type declaration (int/double/string var = expr)
        // These are not handled by parseLine (interpreter ignores them)
        std::string trimmed = line;
        size_t start = trimmed.find_first_not_of(" \t");
        if (start != std::string::npos) trimmed = trimmed.substr(start);
        if (trimmed.rfind("int ", 0) == 0 || trimmed.rfind("double ", 0) == 0 || trimmed.rfind("string ", 0) == 0) {
            std::string typeEnd;
            if (trimmed.rfind("int ", 0) == 0) typeEnd = trimmed.substr(4);
            else if (trimmed.rfind("double ", 0) == 0) typeEnd = trimmed.substr(7);
            else typeEnd = trimmed.substr(7);

            size_t eqPos = typeEnd.find('=');
            std::string varName = typeEnd.substr(0, typeEnd.find_first_of(" ="));
            size_t nonSpace = varName.find_last_not_of(" \t");
            if (nonSpace != std::string::npos) varName = varName.substr(0, nonSpace + 1);

            if (eqPos != std::string::npos) {
                std::string exprStr = typeEnd.substr(eqPos + 1);
                Lexer exprLexer(exprStr);
                Token exprToken = exprLexer.nextToken();
                skipWhitespace(exprLexer, exprToken);
                auto expr = Parser::parseExpr(exprLexer, exprToken);
                Value::Type rhsType = expr->compileBytecode(cf, varTypes);
                varTypes[varName] = rhsType;
                int nameIdx = cf.addConstantStringDedup(varName);
                cf.chunk.writeOp(OpCode::OP_DEF_GLOBAL);
                cf.chunk.writeShort(static_cast<uint16_t>(nameIdx));
            }
        } else {
            Parser::parseLine(line, handler);
        }
    }

    // Patch goto fixups
    for (auto& fixup : gotoFixups) {
        auto it = labelAddresses.find(fixup.first);
        if (it == labelAddresses.end())
            throw std::runtime_error("Undefined goto label: " + fixup.first);
        cf.chunk.patchJump(fixup.second, it->second);
    }
}

bool BytecodeCompiler::validateCall(const std::string& name) {
    size_t dotPos = name.rfind('.');
    auto& libMgr = LibraryManager::getInstance();

    if (dotPos != std::string::npos) {
        std::string libPrefix = name.substr(0, dotPos);
        std::string funcOnly = name.substr(dotPos + 1);
        std::string resolvedLib = libMgr.resolveAlias(libPrefix);

        if (!libMgr.hasLibrary(resolvedLib)) {
            ErrorReporter::reportSimple("CompileError",
                "Unknown library prefix '" + libPrefix + "' in call '" + name + "'",
                "Use 'import ...' to import the library first, or check the library name");
            return false;
        }
        if (!libMgr.isImported(resolvedLib)) {
            std::string extPath = libMgr.getSystemFuncExternalPath(funcOnly);
            if (extPath.empty()) extPath = resolvedLib;
            std::string shortName = LibraryManager::getLastSegment(extPath);
            ErrorReporter::reportSimple("CompileError",
                "Library '" + resolvedLib + "' is not imported. Call: '" + name + "'",
                "Use: import " + extPath + "\n"
                "  Then: " + shortName + "." + funcOnly + "(...)");
            return false;
        }
        if (!libMgr.hasSystemFunction(resolvedLib, funcOnly)) {
            ErrorReporter::reportSimple("CompileError",
                "Function '" + funcOnly + "' not found in library '" + resolvedLib + "'",
                "Check the function name or import the correct library");
            return false;
        }
        return true;
    }

    if (userFuncNames.find(name) != userFuncNames.end()) {
        return true;
    }

    std::string blockedLib = libMgr.getBlockedLibName(name);
    if (!blockedLib.empty()) {
        std::string shortName = LibraryManager::getLastSegment(blockedLib);
        ErrorReporter::reportSimple("CompileError",
            "Function '" + name + "' is from the '" + blockedLib + "' library",
            "You must call it with the library prefix: '" + shortName + "." + name + "(...)'");
        return false;
    }

    std::string sysLibPath = libMgr.getSystemFuncExternalPath(name);
    if (!sysLibPath.empty()) {
        std::string shortName = LibraryManager::getLastSegment(sysLibPath);
        ErrorReporter::reportSimple("CompileError",
            "Function '" + name + "' requires importing a library first",
            "Use: import " + sysLibPath + "\n"
            "  Then: " + shortName + "." + name + "(...)\n"
            "  Or with alias: import " + sysLibPath + " -> my_alias\n"
            "  Then: my_alias." + name + "(...)");
        return false;
    }

    ErrorReporter::reportSimple("CompileError",
        "Undefined function: " + name,
        "Make sure the function is defined or the required library is imported");
    return false;
}

std::string BytecodeCompiler::typeStr(Value::Type t) {
    switch (t) {
    case Value::Type::Int: return "int";
    case Value::Type::Double: return "double";
    case Value::Type::String: return "string";
    case Value::Type::Array: return "array";
    default: return "void";
    }
}

void BytecodeCompiler::typeError(const std::string& msg) {
    throw std::runtime_error("TypeError: " + msg);
}

Value::Type BytecodeCompiler::compileExpr(CompiledFunction& cf, Expr* expr) {
    return expr->compileBytecode(cf, varTypes);
}

// ============================================================
// VM implementation
// ============================================================

VM::VM() : runtimeError(false) {}

void VM::loadProgram(const CompiledProgram& prog) {
    program = prog;
    globals.clear();
    stack.clear();
    frames.clear();
    runtimeError = false;
}

void VM::resetStack() {
    stack.clear();
}

void VM::runtimeErr(const std::string& msg) {
    if (!runtimeError) {
        ErrorReporter::reportSimple("RuntimeError", msg, "");
        runtimeError = true;
    }
}

bool VM::callSystemFunction(const std::string& name, int argCount) {
    // Stack: [arg0, arg1, ..., argN, funcName] (funcName on top)
    // Pop function name first
    pop();

    std::vector<Value> args(argCount);
    for (int i = argCount - 1; i >= 0; i--) {
        args[i] = pop();
    }

    Value result = executeSystemCall(name, args);

    if (!runtimeError) {
        push(result);
    }
    return true;
}

Value VM::executeSystemCall(const std::string& funcName, const std::vector<Value>& args) {
    Interpreter::currentVariables = &globals;
    Interpreter sys;
    if (sys.isSystemFunction(funcName)) {
        return sys.SystemFunctionBuildIn(funcName, args);
    }
    runtimeErr("Unknown system function: " + funcName);
    return Value();
}

bool VM::callFunction(const std::string& name, int argCount) {
    auto it = program.functionIndex.find(name);
    if (it == program.functionIndex.end()) {
        return callSystemFunction(name, argCount);
    }

    const CompiledFunction& func = program.functions[it->second];

    // Check arg count
    if (argCount != static_cast<int>(func.parameters.size())) {
        runtimeErr("Function " + name + " expects " + std::to_string(func.parameters.size())
            + " arguments, got " + std::to_string(argCount));
        return false;
    }

    // Pop function name
    pop();

    // Create new frame
    CallFrame frame;
    frame.function = &func;
    frame.ip = 0;

    // Store arguments as locals
    frame.locals.resize(func.localCount);
    for (int i = argCount - 1; i >= 0; i--) {
        frame.locals[i] = pop();
    }

    frames.push_back(frame);
    return true;
}

void VM::run() {
    if (program.functions.empty()) {
        runtimeErr("No functions to execute");
        return;
    }

    // Find main function
    auto it = program.functionIndex.find("main");
    if (it == program.functionIndex.end()) {
        runtimeErr("No 'main' function found");
        return;
    }

    resetStack();
    frames.clear();
    runtimeError = false;

    // Initialize system libraries
    RegFunc();

    // Start with main function
    CallFrame frame;
    frame.function = &program.functions[it->second];
    frame.ip = 0;
    frame.locals.resize(frame.function->localCount);
    frames.push_back(frame);

    // Execution loop
    while (!frames.empty() && !runtimeError) {
        CallFrame& cf = frames.back();
        const Chunk& chunk = cf.function->chunk;

        if (cf.ip >= chunk.code.size()) {
            frames.pop_back();
            if (!frames.empty()) {
                push(Value()); // void return value
            }
            continue;
        }

        uint8_t instruction = chunk.code[cf.ip++];

        switch (static_cast<OpCode>(instruction)) {
        case OpCode::OP_RETURN: {
            Value retVal = Value();
            if (!stack.empty()) {
                retVal = pop();
            }
            // Restore globals shadowed by this frame's parameters, and
            // remove parameters that did not exist before the call (P1-1)
            if (!frames.empty()) {
                CallFrame& curFrame = frames.back();
                for (const auto& [name, val] : curFrame.savedGlobals) {
                    globals[name] = val;
                }
                for (const auto& name : curFrame.newGlobals) {
                    globals.erase(name);
                }
            }
            frames.pop_back();
            if (!frames.empty()) {
                push(retVal);
            }
            break;
        }
        case OpCode::OP_CONSTANT: {
            uint16_t idx = chunk.readShort(cf.ip);
            cf.ip += 2;
            if (idx < chunk.constants.size()) {
                push(chunk.constants[idx]);
            } else {
                runtimeErr("Constant index out of bounds");
            }
            break;
        }
        case OpCode::OP_NEGATE: {
            Value v = pop();
            if (v.getType() == Value::Type::Int) {
                push(Value(-v.asInt()));
            } else if (v.getType() == Value::Type::Double) {
                push(Value(-v.asDouble()));
            } else {
                runtimeErr("Cannot negate non-numeric value");
            }
            break;
        }
        case OpCode::OP_ADD: {
            Value b = pop();
            Value a = pop();
            if (a.getType() == Value::Type::Int && b.getType() == Value::Type::Int) {
                push(Value(a.asInt() + b.asInt()));
            } else if (a.getType() == Value::Type::Double && b.getType() == Value::Type::Double) {
                push(Value(a.asDouble() + b.asDouble()));
            } else if (a.getType() == Value::Type::String && b.getType() == Value::Type::String) {
                push(Value(a.asString() + b.asString()));
            } else if (a.getType() == Value::Type::Int && b.getType() == Value::Type::Double) {
                push(Value(static_cast<double>(a.asInt()) + b.asDouble()));
            } else if (a.getType() == Value::Type::Double && b.getType() == Value::Type::Int) {
                push(Value(a.asDouble() + static_cast<double>(b.asInt())));
            } else {
                runtimeErr("Type mismatch in addition");
            }
            break;
        }
        case OpCode::OP_SUB: {
            Value b = pop();
            Value a = pop();
            if (a.getType() == Value::Type::Int && b.getType() == Value::Type::Int) {
                push(Value(a.asInt() - b.asInt()));
            } else if (a.getType() == Value::Type::Double && b.getType() == Value::Type::Double) {
                push(Value(a.asDouble() - b.asDouble()));
            } else if (a.getType() == Value::Type::Int && b.getType() == Value::Type::Double) {
                push(Value(static_cast<double>(a.asInt()) - b.asDouble()));
            } else if (a.getType() == Value::Type::Double && b.getType() == Value::Type::Int) {
                push(Value(a.asDouble() - static_cast<double>(b.asInt())));
            } else {
                runtimeErr("Type mismatch in subtraction");
            }
            break;
        }
        case OpCode::OP_MUL: {
            Value b = pop();
            Value a = pop();
            if (a.getType() == Value::Type::Int && b.getType() == Value::Type::Int) {
                push(Value(a.asInt() * b.asInt()));
            } else if (a.getType() == Value::Type::Double && b.getType() == Value::Type::Double) {
                push(Value(a.asDouble() * b.asDouble()));
            } else if (a.getType() == Value::Type::Int && b.getType() == Value::Type::Double) {
                push(Value(static_cast<double>(a.asInt()) * b.asDouble()));
            } else if (a.getType() == Value::Type::Double && b.getType() == Value::Type::Int) {
                push(Value(a.asDouble() * static_cast<double>(b.asInt())));
            } else {
                runtimeErr("Type mismatch in multiplication");
            }
            break;
        }
        case OpCode::OP_DIV: {
            Value b = pop();
            Value a = pop();
            if (a.getType() == Value::Type::Int && b.getType() == Value::Type::Int) {
                if (b.asInt() == 0) { runtimeErr("Division by zero"); break; }
                push(Value(a.asInt() / b.asInt()));
            } else if (a.getType() == Value::Type::Double && b.getType() == Value::Type::Double) {
                if (b.asDouble() == 0.0) { runtimeErr("Division by zero"); break; }
                push(Value(a.asDouble() / b.asDouble()));
            } else if (a.getType() == Value::Type::Int && b.getType() == Value::Type::Double) {
                if (b.asDouble() == 0.0) { runtimeErr("Division by zero"); break; }
                push(Value(static_cast<double>(a.asInt()) / b.asDouble()));
            } else if (a.getType() == Value::Type::Double && b.getType() == Value::Type::Int) {
                if (b.asInt() == 0) { runtimeErr("Division by zero"); break; }
                push(Value(a.asDouble() / static_cast<double>(b.asInt())));
            } else {
                runtimeErr("Type mismatch in division");
            }
            break;
        }

        case OpCode::OP_TRUE: push(Value(1)); break;
        case OpCode::OP_FALSE: push(Value(0)); break;
        case OpCode::OP_NIL: push(Value()); break;
        case OpCode::OP_NOT: {
            Value v = pop();
            push(Value(v.asBool() ? 0 : 1));
            break;
        }
        case OpCode::OP_EQ: {
            Value b = pop();
            Value a = pop();
            bool result = false;
            if (a.getType() == Value::Type::Int && b.getType() == Value::Type::Int) {
                result = (a.asInt() == b.asInt());
            } else if (a.getType() == Value::Type::Double && b.getType() == Value::Type::Double) {
                result = (a.asDouble() == b.asDouble());
            } else if (a.getType() == Value::Type::String && b.getType() == Value::Type::String) {
                result = (a.asString() == b.asString());
            } else if ((a.getType() == Value::Type::Int || a.getType() == Value::Type::Double) &&
                       (b.getType() == Value::Type::Int || b.getType() == Value::Type::Double)) {
                double al = (a.getType() == Value::Type::Int) ? static_cast<double>(a.asInt()) : a.asDouble();
                double bl = (b.getType() == Value::Type::Int) ? static_cast<double>(b.asInt()) : b.asDouble();
                result = (al == bl);
            } else {
                runtimeErr("Type mismatch in equality comparison");
            }
            push(Value(result ? 1 : 0));
            break;
        }
        case OpCode::OP_NE: {
            Value b = pop();
            Value a = pop();
            bool result = false;
            if (a.getType() == Value::Type::Int && b.getType() == Value::Type::Int) {
                result = (a.asInt() != b.asInt());
            } else if (a.getType() == Value::Type::Double && b.getType() == Value::Type::Double) {
                result = (a.asDouble() != b.asDouble());
            } else if (a.getType() == Value::Type::String && b.getType() == Value::Type::String) {
                result = (a.asString() != b.asString());
            } else if ((a.getType() == Value::Type::Int || a.getType() == Value::Type::Double) &&
                       (b.getType() == Value::Type::Int || b.getType() == Value::Type::Double)) {
                double al = (a.getType() == Value::Type::Int) ? static_cast<double>(a.asInt()) : a.asDouble();
                double bl = (b.getType() == Value::Type::Int) ? static_cast<double>(b.asInt()) : b.asDouble();
                result = (al != bl);
            } else {
                runtimeErr("Type mismatch in inequality comparison");
            }
            push(Value(result ? 1 : 0));
            break;
        }
        case OpCode::OP_GT:
        case OpCode::OP_LT:
        case OpCode::OP_GE:
        case OpCode::OP_LE: {
            Value b = pop();
            Value a = pop();
            double al = (a.getType() == Value::Type::Int) ? static_cast<double>(a.asInt()) : a.asDouble();
            double bl = (b.getType() == Value::Type::Int) ? static_cast<double>(b.asInt()) : b.asDouble();
            bool result = false;
            switch (static_cast<OpCode>(instruction)) {
            case OpCode::OP_GT: result = (al > bl); break;
            case OpCode::OP_LT: result = (al < bl); break;
            case OpCode::OP_GE: result = (al >= bl); break;
            case OpCode::OP_LE: result = (al <= bl); break;
            default: break;
            }
            push(Value(result ? 1 : 0));
            break;
        }
        case OpCode::OP_PRINT: {
            std::cout << pop().toString();
            break;
        }
        case OpCode::OP_PRINTLN: {
            std::cout << pop().toString() << std::endl;
            break;
        }
        case OpCode::OP_ENDLN: {
            std::cout << std::endl;
            break;
        }
        case OpCode::OP_POP: {
            pop();
            break;
        }
        case OpCode::OP_DEF_GLOBAL: {
            uint16_t nameIdx = chunk.readShort(cf.ip);
            cf.ip += 2;
            if (nameIdx >= chunk.constants.size() ||
                chunk.constants[nameIdx].getType() != Value::Type::String) {
                runtimeErr("Invalid global variable name constant");
                break;
            }
            std::string name = chunk.constants[nameIdx].asString();
            Value val = pop();
            globals[name] = val;
            break;
        }
        case OpCode::OP_GET_GLOBAL: {
            uint16_t nameIdx = chunk.readShort(cf.ip);
            cf.ip += 2;
            if (nameIdx >= chunk.constants.size() ||
                chunk.constants[nameIdx].getType() != Value::Type::String) {
                runtimeErr("Invalid global variable name constant");
                break;
            }
            std::string name = chunk.constants[nameIdx].asString();
            auto it = globals.find(name);
            if (it == globals.end()) {
                runtimeErr("Undefined variable: " + name);
                break;
            }
            push(it->second);
            break;
        }
        case OpCode::OP_SET_GLOBAL: {
            uint16_t nameIdx = chunk.readShort(cf.ip);
            cf.ip += 2;
            if (nameIdx >= chunk.constants.size() ||
                chunk.constants[nameIdx].getType() != Value::Type::String) {
                runtimeErr("Invalid global variable name constant");
                break;
            }
            std::string name = chunk.constants[nameIdx].asString();
            Value val = peek();
            auto it = globals.find(name);
            if (it == globals.end()) {
                runtimeErr("Undefined variable: " + name);
                break;
            }
            it->second = val;
            break;
        }
        case OpCode::OP_UNSET_GLOBAL: {
            uint16_t nameIdx = chunk.readShort(cf.ip);
            cf.ip += 2;
            if (nameIdx >= chunk.constants.size() ||
                chunk.constants[nameIdx].getType() != Value::Type::String) {
                runtimeErr("Invalid global variable name constant");
                break;
            }
            std::string name = chunk.constants[nameIdx].asString();
            globals.erase(name);
            break;
        }
        case OpCode::OP_CLEAR_GLOBALS: {
            globals.clear();
            break;
        }
        case OpCode::OP_GET_LOCAL: {
            uint8_t slot = chunk.readByte(cf.ip);
            cf.ip++;
            if (slot >= cf.locals.size()) {
                runtimeErr("Local variable slot out of bounds");
                break;
            }
            push(cf.locals[slot]);
            break;
        }
        case OpCode::OP_SET_LOCAL: {
            uint8_t slot = chunk.readByte(cf.ip);
            cf.ip++;
            if (slot >= cf.locals.size()) {
                runtimeErr("Local variable slot out of bounds");
                break;
            }
            cf.locals[slot] = peek();
            break;
        }
        case OpCode::OP_JMP: {
            int32_t offset = static_cast<int16_t>(chunk.readShort(cf.ip));
            cf.ip += 2;
            cf.ip += offset;
            break;
        }
        case OpCode::OP_JMP_IF_FALSE: {
            int32_t offset = static_cast<int16_t>(chunk.readShort(cf.ip));
            cf.ip += 2;
            Value cond = pop();
            if (!cond.asBool()) {
                cf.ip += offset;
            }
            break;
        }
        case OpCode::OP_LOOP: {
            int32_t offset = static_cast<int16_t>(chunk.readShort(cf.ip));
            cf.ip += 2;
            cf.ip += offset;
            break;
        }
        case OpCode::OP_CALL: {
            uint8_t argCount = chunk.readByte(cf.ip);
            cf.ip++;

            if (static_cast<int>(stack.size()) < argCount + 1) {
                runtimeErr("Stack underflow in function call");
                break;
            }

            // Pop function name (on top of stack, above args)
            Value fnVal = pop();
            if (fnVal.getType() != Value::Type::String) {
                runtimeErr("Function name must be a string");
                break;
            }
            std::string fnName = fnVal.asString();

            auto progIt = program.functionIndex.find(fnName);
            if (progIt != program.functionIndex.end()) {
                const CompiledFunction& func = program.functions[progIt->second];
                if (argCount != static_cast<uint8_t>(func.parameters.size())) {
                    runtimeErr("Function " + fnName + " expects " + std::to_string(func.parameters.size())
                        + " arguments, got " + std::to_string(argCount));
                    break;
                }

                // Pop args (pushed left-to-right, arg0 is bottommost)
                std::vector<Value> args(argCount);
                for (int i = argCount - 1; i >= 0; i--) {
                    args[i] = pop();
                }

                // Create new frame to execute function
                frames.push_back(CallFrame{});
                CallFrame& newFrame = frames.back();
                newFrame.function = &func;
                newFrame.ip = 0;
                newFrame.locals.resize(func.localCount);

                // Save and set parameters as global variables
                for (size_t i = 0; i < func.parameters.size(); i++) {
                    auto it = globals.find(func.parameters[i].name);
                    if (it != globals.end()) {
                        newFrame.savedGlobals[func.parameters[i].name] = it->second;
                    } else {
                        newFrame.newGlobals.push_back(func.parameters[i].name);
                    }
                    globals[func.parameters[i].name] = args[i];
                }
            } else {
                push(fnVal);
                if (!callSystemFunction(fnName, argCount)) {
                    runtimeErr("Undefined function: " + fnName);
                }
            }
            break;
        }
        case OpCode::OP_INPUT: {
            std::string userInput;
            std::getline(std::cin, userInput);
            push(Value(userInput));
            // nameIdx follows but handled by subsequent OP_DEF_GLOBAL
            break;
        }
        case OpCode::OP_CAST_INT: {
            Value v = pop();
            switch (v.getType()) {
            case Value::Type::Int: push(Value(v.asInt())); break;
            case Value::Type::Double: push(Value(static_cast<int>(v.asDouble()))); break;
            case Value::Type::String: {
                try { push(Value(std::stoi(v.asString()))); }
                catch (...) { runtimeErr("Cannot cast string to int"); }
                break;
            }
            default: runtimeErr("Cannot cast to int"); break;
            }
            break;
        }
        case OpCode::OP_CAST_DOUBLE: {
            Value v = pop();
            switch (v.getType()) {
            case Value::Type::Int: push(Value(static_cast<double>(v.asInt()))); break;
            case Value::Type::Double: push(Value(v.asDouble())); break;
            case Value::Type::String: {
                try { push(Value(std::stod(v.asString()))); }
                catch (...) { runtimeErr("Cannot cast string to double"); }
                break;
            }
            default: runtimeErr("Cannot cast to double"); break;
            }
            break;
        }
        case OpCode::OP_ARRAY: {
            uint8_t elemCount = chunk.readByte(cf.ip);
            cf.ip++;
            std::vector<Value> elements;
            for (int i = elemCount - 1; i >= 0; i--) {
                elements.push_back(stack[stack.size() - 1 - i]);
            }
            for (int i = 0; i < elemCount; i++) pop();
            push(Value(elements));
            break;
        }
        case OpCode::OP_INDEX_GET: {
            Value index = pop();
            Value arr = pop();
            if (arr.getType() != Value::Type::Array) {
                runtimeErr("Index target is not an array");
                break;
            }
            if (index.getType() != Value::Type::Int) {
                runtimeErr("Array index must be an integer");
                break;
            }
            int idx = index.asInt();
            const std::vector<Value>& elements = arr.asArray();
            if (idx < 0 || idx >= static_cast<int>(elements.size())) {
                runtimeErr("Array index out of bounds");
                break;
            }
            push(elements[idx]);
            break;
        }
        case OpCode::OP_INDEX_SET: {
            Value index = pop();
            Value arr = pop();
            Value value = pop();
            if (arr.getType() != Value::Type::Array) {
                runtimeErr("Index target is not an array");
                break;
            }
            if (index.getType() != Value::Type::Int) {
                runtimeErr("Array index must be an integer");
                break;
            }
            int idx = index.asInt();
            std::vector<Value>& elements = arr.asArrayRef();
            if (idx < 0 || idx >= static_cast<int>(elements.size())) {
                runtimeErr("Array index out of bounds");
                break;
            }
            elements[idx] = value;
            push(arr);
            break;
        }
        case OpCode::OP_EXIT: {
            Value code = pop();
            if (code.getType() == Value::Type::Int) {
                std::exit(code.asInt());
            }
            std::exit(0);
            break;
        }
        case OpCode::OP_AND: {
            // Short-circuit AND
            Value left = pop();
            if (!left.asBool()) {
                push(Value(0));
            } else {
                Value right = pop();
                push(Value(right.asBool() ? 1 : 0));
            }
            break;
        }
        case OpCode::OP_OR: {
            Value left = pop();
            if (left.asBool()) {
                push(Value(1));
            } else {
                Value right = pop();
                push(Value(right.asBool() ? 1 : 0));
            }
            break;
        }
        case OpCode::OP_IMPORT: {
            // Import is handled by the compiler stage, skip at runtime
            break;
        }
        case OpCode::OP_NEW: {
            Value sizeVal = pop();
            int size = sizeVal.asInt();
            if (size < 0) {
                runtimeErr("new() size must be non-negative");
                break;
            }
            // Same per-function stack budget as the interpreter path (P3-5)
            if (!frames.empty()) {
                CallFrame& curFrame = frames.back();
                if (curFrame.newAllocBytes + size > Parser::MAX_FUNC_NEW_BYTES) {
                    runtimeErr("Function stack memory exceeded. new() total would be " +
                        std::to_string(curFrame.newAllocBytes + size) +
                        " bytes, but func stack limit is " +
                        std::to_string(Parser::MAX_FUNC_NEW_BYTES) +
                        " bytes. Define this variable outside the function (heap).");
                    break;
                }
                curFrame.newAllocBytes += size;
            }
            std::vector<uint8_t> bytes(size, 0);
            push(Value(bytes));
            break;
        }
        case OpCode::OP_HALT: {
            frames.clear();
            break;
        }
        default:
            runtimeErr("Unknown opcode: " + std::to_string(instruction));
            break;
        }
    }
}
