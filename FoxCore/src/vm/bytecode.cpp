#include "bytecode.hpp"
#include <typeinfo>
#include <cstdio>
#include "../util/error_reporter.hpp"
#include "../util/utils.hpp"

// This VM name is FVM

// ============================================================
// Chunk implementation
// ============================================================
void Chunk::write(uint8_t byte) {
    code.push_back(byte);
}

void Chunk::writeOp(OpCode op) {
    instructionOffsets.push_back(code.size());
    instructionLines.push_back(currentLine);
    write(static_cast<uint8_t>(op));
}

int Chunk::lineAt(size_t ip) const {
    // Upper bound: first instruction start > ip
    auto it = std::upper_bound(instructionOffsets.begin(), instructionOffsets.end(), ip);
    if (it == instructionOffsets.begin()) return 0;
    size_t idx = static_cast<size_t>(it - instructionOffsets.begin()) - 1;
    return idx < instructionLines.size() ? instructionLines[idx] : 0;
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
    case Value::Type::Dict: {
        data.push_back(6);
        const auto& dict = v.asDict();
        writeVarint(data, static_cast<uint32_t>(dict.size()));
        for (const auto& [key, h] : dict) {
            writeString(data, key);
            const Value* elem = Gc::instance().deref(h);
            if (elem) writeValue(data, *elem);
            else writeValue(data, Value());
        }
        break;
    }
    case Value::Type::Object: {
        data.push_back(7);
        writeString(data, v.asObjectClass());
        const auto& members = v.asObjectDict();
        writeVarint(data, static_cast<uint32_t>(members.size()));
        for (const auto& [key, h] : members) {
            writeString(data, key);
            const Value* elem = Gc::instance().deref(h);
            if (elem) writeValue(data, *elem);
            else writeValue(data, Value());
        }
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
    case 6: { // dict
        uint32_t dictLen = readVarint(data, size, offset);
        std::unordered_map<std::string, GcHandle> dict;
        for (uint32_t j = 0; j < dictLen; j++) {
            std::string key = readString(data, size, offset);
            Value elem;
            if (!readValue(data, size, offset, elem)) return false;
            dict[key] = Gc::instance().alloc(elem);
        }
        out = Value(dict);
        return true;
    }
    case 7: { // object
        std::string className = readString(data, size, offset);
        uint32_t memberCount = readVarint(data, size, offset);
        out = Value::makeObject(className);
        auto& members = out.asObjectDictRef();
        for (uint32_t j = 0; j < memberCount; j++) {
            std::string key = readString(data, size, offset);
            Value elem;
            if (!readValue(data, size, offset, elem)) return false;
            members[key] = Gc::instance().alloc(elem);
        }
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

    // Instruction line table
    writeVarint(data, static_cast<uint32_t>(instructionOffsets.size()));
    int prevLine = 0;
    for (size_t i = 0; i < instructionOffsets.size(); i++) {
        writeVarint(data, static_cast<uint32_t>(instructionOffsets[i]));
        // Delta-encode lines: source lines are non-decreasing, so deltas are >= 0
        int delta = instructionLines[i] - prevLine;
        writeVarint(data, static_cast<uint32_t>(delta));
        prevLine = instructionLines[i];
    }

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

    // Instruction line table
    uint32_t instrCount = readVarint(data, size, offset);
    instructionOffsets.clear();
    instructionLines.clear();
    instructionOffsets.reserve(instrCount);
    instructionLines.reserve(instrCount);
    int prevLine = 0;
    for (uint32_t i = 0; i < instrCount; i++) {
        uint32_t off = readVarint(data, size, offset);
        int line = prevLine + static_cast<int>(readVarint(data, size, offset));
        instructionOffsets.push_back(off);
        instructionLines.push_back(line);
        prevLine = line;
    }

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
    writeVarint(dummy, static_cast<uint32_t>(instructionOffsets.size()));
    int prevLine = 0;
    for (size_t i = 0; i < instructionOffsets.size(); i++) {
        writeVarint(dummy, static_cast<uint32_t>(instructionOffsets[i]));
        writeVarint(dummy, static_cast<uint32_t>(instructionLines[i] - prevLine));
        prevLine = instructionLines[i];
    }
    return dummy.size();
}

// ============================================================
// CompiledProgram serialization
// ============================================================
std::vector<uint8_t> CompiledProgram::serialize() const {
    std::vector<uint8_t> data;

    // Magic "FOXC"
    data.push_back('F'); data.push_back('O'); data.push_back('X'); data.push_back('C');
    // Version (3: source-line table per chunk)
    writeUint32(data, 3);

    // Import entries
    writeVarint(data, static_cast<uint32_t>(imports.size()));
    for (const auto& imp : imports) {
        writeString(data, imp.libName);
        writeString(data, imp.alias);
    }

    // Class/struct definitions
    writeVarint(data, static_cast<uint32_t>(classes.size()));
    for (const auto& cls : classes) {
        writeString(data, cls.name);
        data.push_back(cls.isStruct ? 1 : 0);
        writeVarint(data, static_cast<uint32_t>(cls.fields.size()));
        for (const auto& f : cls.fields) {
            writeString(data, f.name);
            writeString(data, f.type);
        }
        data.push_back(cls.hasInit ? 1 : 0);
        writeVarint(data, static_cast<uint32_t>(cls.methodNames.size()));
        for (const auto& m : cls.methodNames) {
            writeString(data, m);
        }
    }

    // Number of functions
    writeVarint(data, static_cast<uint32_t>(functions.size()));

    for (const auto& func : functions) {
        writeString(data, func.name);
        writeString(data, func.returnType);
        writeString(data, func.sourceFile);

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
    if (version != 3) {
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

    uint32_t classCount = readVarint(data, size, offset);
    for (uint32_t i = 0; i < classCount; i++) {
        CompiledClass cls;
        cls.name = readString(data, size, offset);
        cls.isStruct = (data[offset++] != 0);
        uint32_t fieldCount = readVarint(data, size, offset);
        for (uint32_t j = 0; j < fieldCount; j++) {
            ClassField f;
            f.name = readString(data, size, offset);
            f.type = readString(data, size, offset);
            cls.fields.push_back(f);
        }
        cls.hasInit = (data[offset++] != 0);
        uint32_t methodCount = readVarint(data, size, offset);
        for (uint32_t j = 0; j < methodCount; j++) {
            cls.methodNames.push_back(readString(data, size, offset));
        }
        prog.classIndex[cls.name] = prog.classes.size();
        prog.classes.push_back(std::move(cls));
    }

    uint32_t funcCount = readVarint(data, size, offset);

    for (uint32_t i = 0; i < funcCount; i++) {
        CompiledFunction cf;
        cf.name = readString(data, size, offset);
        cf.returnType = readString(data, size, offset);
        cf.sourceFile = readString(data, size, offset);

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

bool BytecodeCompiler::s_expandImports = true;
std::unordered_set<std::string> BytecodeCompiler::s_userFuncNames;

CompiledProgram BytecodeCompiler::compile(const std::string& source, const std::string& filename) {
    program = CompiledProgram();
    s_userFuncNames.clear();
    g_classRegistry.clear(); // fresh class/struct definitions per program

    // Initialize system libraries before parsing (needed for import resolution)
    RegFunc();

    // Parse each file into its own function map, then merge with duplicate
    // detection across files (import "file.fox" support)
    std::unordered_map<std::string, Function> parsedFunctions;
    std::unordered_map<std::string, std::string> funcOrigins; // func name -> source file

    auto parseInto = [&](const std::string& src, const std::string& label,
        const std::string& ns, const std::string& alias) {
        std::unordered_map<std::string, Value> fileVars;
        std::unordered_map<std::string, Function> fileFuncs;
        Parser parser(src, fileVars, fileFuncs);
        import_prefix = ns;
        import_alias = alias;
        parser.parseAllFunctions();
        import_prefix.clear();
        import_alias.clear();
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
            auto it = parsedFunctions.find(funcName);
            if (it != parsedFunctions.end()) {
                throw std::runtime_error("Duplicate function '" + funcName + "' defined in '"
                    + funcOrigins[funcName] + "' and '" + label + "'");
            }
            funcOrigins[funcName] = label;
            parsedFunctions[funcName] = func;
        }
    };

    // Parse the main file, then every imported file. Nested imports are
    // appended to imported_source_files while parsing; 'visited' prevents
    // cycles and duplicate processing.
    imported_source_files.clear();
    import_base_file = filename;
    parseInto(source, filename.empty() ? "<main>" : filename, "", "");

    if (s_expandImports) {
    std::unordered_set<std::string> visited;
    for (size_t idx = 0; idx < imported_source_files.size(); idx++) {
        // Copy: parsing appends to imported_source_files (nested imports),
        // which may reallocate the vector.
        std::string path = std::get<0>(imported_source_files[idx]);
        std::string ns = std::get<1>(imported_source_files[idx]);
        std::string alias = std::get<2>(imported_source_files[idx]);
        if (!visited.insert(path).second) continue;
        std::string src = read_file(path);
        if (src.empty()) {
            throw std::runtime_error("Cannot read imported file: " + path);
        }
        import_base_file = path;
        parseInto(src, path, ns, alias);
    }
    }
    import_base_file.clear();

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
        s_userFuncNames.insert(funcName);
    }

    // Compile each function
    for (const auto& [funcName, func] : parsedFunctions) {
        CompiledFunction cf;
        cf.name = func.name;
        cf.returnType = func.returnType;
        cf.sourceFile = funcOrigins[funcName];
        cf.parameters = func.parameters;
        cf.localCount = static_cast<int>(func.parameters.size()); // params are locals

        // Reserve slots for parameters as locals
        for (size_t i = 0; i < func.parameters.size(); i++) {
            // Opcodes for locals use slot indices 0..N-1
        }

        compileFunctionBody(cf, func.body, func.bodyLines);

        // Ensure every function ends with a return instruction.
        // NOTE: cannot use code.back() here: the operand byte of a 0-arg OP_CALL
        // is 0x00, which collides with the OP_RETURN opcode (P0-2). Appending
        // unconditionally is safe ?? an already-emitted OP_RETURN pops the frame,
        // so a trailing extra OP_RETURN is dead code.
        cf.chunk.writeOp(OpCode::OP_RETURN);

        program.functions.push_back(cf);
        program.functionIndex[cf.name] = program.functions.size() - 1;
    }

    // Compile class/struct definitions: methods become functions named
    // "<Class>.<method>" with 'this' bound as the first parameter.
    for (const auto& [className, def] : g_classRegistry) {
        CompiledClass cls;
        cls.name = className;
        cls.isStruct = def.isStruct;
        cls.fields = def.fields;
        cls.hasInit = def.hasInit;

        auto compileMethod = [&](const Function& method, const std::string& fnName) {
            CompiledFunction cf;
            // method.name already carries the "<Class>." prefix for regular
            // methods (parseClassDef); the VM looks up "className.methodName",
            // and constructors live under "className.init".
            cf.name = fnName;
            cf.returnType = method.returnType;
            cf.parameters = method.parameters;
            cf.localCount = static_cast<int>(method.parameters.size());
            compileFunctionBody(cf, method.body, method.bodyLines);
            cf.chunk.writeOp(OpCode::OP_RETURN);
            program.functions.push_back(cf);
            program.functionIndex[cf.name] = program.functions.size() - 1;
        };

        if (def.hasInit) {
            compileMethod(def.initFunc, className + ".init");
        }
        for (const auto& method : def.methods) {
            compileMethod(method, method.name);
            cls.methodNames.push_back(method.name);
        }

        program.classIndex[className] = program.classes.size();
        program.classes.push_back(std::move(cls));
    }

    return program;
}

void BytecodeCompiler::compileFunctionBody(CompiledFunction& cf, const std::vector<std::string>& body, const std::vector<int>& bodyLines) {
    std::unordered_map<std::string, size_t> labelAddresses;
    std::vector<std::pair<std::string, size_t>> gotoFixups;

    class BytecodeStmtHandler : public StmtHandler {
    public:
        CompiledFunction& cf;
        std::unordered_map<std::string, Value::Type>& varTypes;
        BytecodeCompiler& compiler;
        std::unordered_map<std::string, size_t>& labelAddresses;
        std::vector<std::pair<std::string, size_t>>& gotoFixups;
        struct LoopFix {
            size_t pos;
            size_t depth;
        };
        std::vector<LoopFix> breakFixups;
        std::vector<LoopFix> continueFixups;
        size_t loopDepth = 0;
        size_t tryDepth = 0;
        bool sawRetWithValue = false;
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
            if (arg) {
                if (cf.returnType == "void") {
                    throw std::runtime_error("Function " + cf.name + " is declared void but returned a value");
                }
                sawRetWithValue = true;
                arg->compileBytecode(cf, varTypes);
            }
            else if (cf.returnType != "void") {
                throw std::runtime_error("Function " + cf.name + " is declared " + cf.returnType + " but 'ret' returns no value");
            }
            for (size_t i = 0; i < tryDepth; i++) {
                cf.chunk.writeOp(OpCode::OP_END_TRY);
            }
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
            size_t dot = name.find('.');
            if (dot != std::string::npos) {
                std::string prefix = name.substr(0, dot);
                auto& libMgr = LibraryManager::getInstance();
                std::string resolvedLib = libMgr.resolveAlias(prefix);
                bool isNsScript = BytecodeCompiler::s_userFuncNames.find(name)
                    != BytecodeCompiler::s_userFuncNames.end();
                if (prefix != "co" &&
                    !isNsScript &&
                    !(libMgr.hasLibrary(resolvedLib) && libMgr.isImported(resolvedLib))) {
                    // Object method call statement: obj.method(args)
                    int objIdx = cf.addConstantStringDedup(prefix);
                    cf.chunk.writeOp(OpCode::OP_GET_GLOBAL);
                    cf.chunk.writeShort(static_cast<uint16_t>(objIdx));
                    for (auto& arg : args) {
                        arg->compileBytecode(cf, varTypes);
                    }
                    int memberIdx = cf.addConstantStringDedup(name.substr(dot + 1));
                    cf.chunk.writeOp(OpCode::OP_OBJ_CALL);
                    cf.chunk.writeShort(static_cast<uint16_t>(memberIdx));
                    cf.chunk.writeByte(static_cast<uint8_t>(args.size()));
                    cf.chunk.writeOp(OpCode::OP_POP);
                    return;
                }
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
            size_t dot = name.find('.');
            if (dot != std::string::npos) {
                std::string prefix = name.substr(0, dot);
                auto& libMgr = LibraryManager::getInstance();
                std::string resolvedLib = libMgr.resolveAlias(prefix);
                if (!(libMgr.hasLibrary(resolvedLib) && libMgr.isImported(resolvedLib))) {
                    // Object field write: obj.field = value
                    int objIdx = cf.addConstantStringDedup(prefix);
                    cf.chunk.writeOp(OpCode::OP_GET_GLOBAL);
                    cf.chunk.writeShort(static_cast<uint16_t>(objIdx));
                    int memberIdx = cf.addConstantStringDedup(name.substr(dot + 1));
                    cf.chunk.writeOp(OpCode::OP_OBJ_FIELD_SET);
                    cf.chunk.writeShort(static_cast<uint16_t>(memberIdx));
                    cf.chunk.writeOp(OpCode::OP_POP);
                    varTypes[prefix] = Value::Type::Object;
                    return;
                }
            }
            varTypes[name] = rhsType;
            int nameIdx = cf.addConstantStringDedup(name);
            cf.chunk.writeOp(OpCode::OP_DEF_GLOBAL);
            cf.chunk.writeShort(static_cast<uint16_t>(nameIdx));
        }
        void onIndexAssign(const std::string& name, std::unique_ptr<Expr> index, std::unique_ptr<Expr> value) override {
            value->compileBytecode(cf, varTypes);
            size_t dot = name.find('.');
            if (dot != std::string::npos) {
                std::string prefix = name.substr(0, dot);
                auto& libMgr = LibraryManager::getInstance();
                std::string resolvedLib = libMgr.resolveAlias(prefix);
                if (!(libMgr.hasLibrary(resolvedLib) && libMgr.isImported(resolvedLib))) {
                    // Object field index write: obj.field[idx] = value
                    int objIdx = cf.addConstantStringDedup(prefix);
                    int memberIdx = cf.addConstantStringDedup(name.substr(dot + 1));
                    cf.chunk.writeOp(OpCode::OP_GET_GLOBAL);
                    cf.chunk.writeShort(static_cast<uint16_t>(objIdx));
                    cf.chunk.writeOp(OpCode::OP_OBJ_FIELD_GET);
                    cf.chunk.writeShort(static_cast<uint16_t>(memberIdx));
                    index->compileBytecode(cf, varTypes);
                    cf.chunk.writeOp(OpCode::OP_INDEX_SET);
                    cf.chunk.writeOp(OpCode::OP_GET_GLOBAL);
                    cf.chunk.writeShort(static_cast<uint16_t>(objIdx));
                    cf.chunk.writeOp(OpCode::OP_OBJ_FIELD_SET);
                    cf.chunk.writeShort(static_cast<uint16_t>(memberIdx));
                    cf.chunk.writeOp(OpCode::OP_POP);
                    varTypes[prefix] = Value::Type::Object;
                    return;
                }
            }
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

            size_t skipElseJump = 0;
            if (!ifStmt.elseBody.empty()) {
                skipElseJump = cf.chunk.code.size();
                cf.chunk.writeOp(OpCode::OP_JMP);
                cf.chunk.writeShort(0);
            }

            size_t afterBody = cf.chunk.code.size();
            cf.chunk.patchJump(jumpInstr + 1, afterBody);

            if (!ifStmt.elseBody.empty()) {
                for (const auto& stmt : ifStmt.elseBody) {
                    Parser::parseLine(stmt, *this);
                }
                cf.chunk.patchJump(skipElseJump + 1, cf.chunk.code.size());
            }
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

            loopDepth++;
            for (const auto& stmt : whileStmt.body) {
                Parser::parseLine(stmt, *this);
            }
            size_t continueTarget = loopStart;

            size_t afterBody = cf.chunk.code.size();
            cf.chunk.writeOp(OpCode::OP_LOOP);
            int32_t loopOffset = static_cast<int32_t>(loopStart) - static_cast<int32_t>(afterBody + 3);
            cf.chunk.writeShort(static_cast<uint16_t>(static_cast<int32_t>(loopOffset)));
            size_t breakTarget = cf.chunk.code.size();
            cf.chunk.patchJump(exitJump + 1, breakTarget);

            std::vector<LoopFix> remainingBreaks;
            for (const auto& fix : breakFixups) {
                if (fix.depth == loopDepth) {
                    cf.chunk.patchJump(fix.pos, breakTarget);
                } else {
                    remainingBreaks.push_back(fix);
                }
            }
            breakFixups.swap(remainingBreaks);
            std::vector<LoopFix> remainingContinues;
            for (const auto& fix : continueFixups) {
                if (fix.depth == loopDepth) {
                    cf.chunk.patchJump(fix.pos, continueTarget);
                } else {
                    remainingContinues.push_back(fix);
                }
            }
            continueFixups.swap(remainingContinues);
            loopDepth--;
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

            loopDepth++;
            for (const auto& stmt : forStmt.body) {
                Parser::parseLine(stmt, *this);
            }

            size_t continueTarget = cf.chunk.code.size();
            if (!forStmt.iter.empty()) {
                Parser::parseLine(forStmt.iter, *this);
            }

            size_t afterBody = cf.chunk.code.size();
            cf.chunk.writeOp(OpCode::OP_LOOP);
            int32_t loopOffset = static_cast<int32_t>(loopStart) - static_cast<int32_t>(afterBody + 3);
            cf.chunk.writeShort(static_cast<uint16_t>(static_cast<int32_t>(loopOffset)));
            size_t breakTarget = cf.chunk.code.size();
            cf.chunk.patchJump(exitJump + 1, breakTarget);

            std::vector<LoopFix> remainingBreaks;
            for (const auto& fix : breakFixups) {
                if (fix.depth == loopDepth) {
                    cf.chunk.patchJump(fix.pos, breakTarget);
                } else {
                    remainingBreaks.push_back(fix);
                }
            }
            breakFixups.swap(remainingBreaks);
            std::vector<LoopFix> remainingContinues;
            for (const auto& fix : continueFixups) {
                if (fix.depth == loopDepth) {
                    cf.chunk.patchJump(fix.pos, continueTarget);
                } else {
                    remainingContinues.push_back(fix);
                }
            }
            continueFixups.swap(remainingContinues);
            loopDepth--;
        }
        void onTry(TryStatement tryStmt) override {
            cf.chunk.writeOp(OpCode::OP_TRY);
            int varNameIdx = cf.addConstantStringDedup(tryStmt.errorVar);
            cf.chunk.writeShort(static_cast<uint16_t>(varNameIdx));
            size_t catchOffsetPos = cf.chunk.code.size();
            cf.chunk.writeShort(0);
            size_t afterOperands = cf.chunk.code.size();

            tryDepth++;
            for (const auto& stmt : tryStmt.body) {
                Parser::parseLine(stmt, *this);
            }

            cf.chunk.writeOp(OpCode::OP_END_TRY);
            tryDepth--;
            size_t skipCatchJump = cf.chunk.code.size();
            cf.chunk.writeOp(OpCode::OP_JMP);
            cf.chunk.writeShort(0);
            size_t catchAddr = cf.chunk.code.size();
            int32_t catchRel = static_cast<int32_t>(catchAddr) - static_cast<int32_t>(afterOperands);
            cf.chunk.patchJumpOffset(catchOffsetPos, catchRel);

            for (const auto& stmt : tryStmt.catchBody) {
                Parser::parseLine(stmt, *this);
            }
            cf.chunk.patchJump(skipCatchJump + 1, cf.chunk.code.size());
        }
        void onBreak() override {
            if (loopDepth == 0) {
                throw std::runtime_error("break outside loop");
            }
            for (size_t i = 0; i < tryDepth; i++) {
                cf.chunk.writeOp(OpCode::OP_END_TRY);
            }
            size_t pos = cf.chunk.code.size() + 1;
            cf.chunk.writeOp(OpCode::OP_JMP);
            cf.chunk.writeShort(0);
            breakFixups.push_back({pos, loopDepth});
        }
        void onContinue() override {
            if (loopDepth == 0) {
                throw std::runtime_error("continue outside loop");
            }
            for (size_t i = 0; i < tryDepth; i++) {
                cf.chunk.writeOp(OpCode::OP_END_TRY);
            }
            size_t pos = cf.chunk.code.size() + 1;
            cf.chunk.writeOp(OpCode::OP_JMP);
            cf.chunk.writeShort(0);
            continueFixups.push_back({pos, loopDepth});
        }
        void onError(std::unique_ptr<Expr> message) override {
            message->compileBytecode(cf, varTypes);
            cf.chunk.writeOp(OpCode::OP_THROW);
        }
        void onYield(std::unique_ptr<Expr> value) override {
            if (value) value->compileBytecode(cf, varTypes);
            cf.chunk.writeOp(OpCode::OP_YIELD);
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

    for (size_t lineIdx = 0; lineIdx < body.size(); lineIdx++) {
        const std::string& line = body[lineIdx];
        if (line.empty()) continue;
        if (lineIdx < bodyLines.size()) {
            cf.chunk.currentLine = bodyLines[lineIdx];
        }

        // Check for type declaration (int/double/string var = expr)
        // These are not handled by parseLine (interpreter ignores them)
        std::string trimmed = line;
        size_t start = trimmed.find_first_not_of(" \t");
        if (start != std::string::npos) trimmed = trimmed.substr(start);
        if (trimmed.rfind("int ", 0) == 0 || trimmed.rfind("double ", 0) == 0 ||
            trimmed.rfind("string ", 0) == 0 || trimmed.rfind("dict ", 0) == 0) {
            std::string typeEnd;
            if (trimmed.rfind("int ", 0) == 0) typeEnd = trimmed.substr(4);
            else if (trimmed.rfind("double ", 0) == 0) typeEnd = trimmed.substr(7);
            else if (trimmed.rfind("dict ", 0) == 0) typeEnd = trimmed.substr(5);
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

    // A non-void function must contain at least one 'ret <expr>' statement
    // (a runtime check catches paths that fall through without returning)
    if (cf.returnType != "void" && !handler.sawRetWithValue) {
        throw std::runtime_error("Function " + cf.name + " is declared to return " + cf.returnType + " but never returns a value");
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
            // Not a library prefix: treat as an object method call
            // (obj.method(...)); the runtime validates the object.
            return true;
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

    if (s_userFuncNames.find(name) != s_userFuncNames.end()) {
        return true;
    }

    // Standalone compilation: the callee may live in another .fc loaded at
    // runtime, so defer the check (the VM reports missing functions).
    if (!s_expandImports) {
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
    case Value::Type::Bytes: return "bytes";
    case Value::Type::Dict: return "dict";
    case Value::Type::Object: return "object";
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

VM::VM() : runtimeError(false) {
    gcRootId_ = Gc::instance().addRoot([this](Gc& gc) { this->traceGC(gc); });
}

VM::~VM() {
    Gc::instance().removeRoot(gcRootId_);
}

void VM::traceGC(Gc& gc) {
    for (const auto& v : stack) v.traceGC();
    for (const auto& [name, v] : globals) v.traceGC();
    for (const auto& v : globalVector_) v.traceGC();
for (const auto& frame : frames) {
        for (const auto& v : frame.locals) v.traceGC();
        for (const auto& [name, v] : frame.savedGlobals) v.traceGC();
        for (size_t i = 0; i < frame.paramSaveCount && i < 4; i++) {
            frame.paramSavesSmall[i].traceGC();
        }
        for (const auto& v : frame.paramSavesBig) v.traceGC();
        frame.ctorResult.traceGC();
    }
    for (const auto& c : program.functions) {
        for (const auto& v : c.chunk.constants) v.traceGC();
    }
    for (const auto& extra : extraPrograms) {
        for (const auto& c : extra.functions) {
            for (const auto& v : c.chunk.constants) v.traceGC();
        }
    }
    // Frozen coroutine contexts are roots while suspended
    for (const auto& coro : coroutines_) {
        for (const auto& v : coro.stack) v.traceGC();
        for (const auto& frame : coro.frames) {
            for (const auto& v : frame.locals) v.traceGC();
            for (const auto& [name, v] : frame.savedGlobals) v.traceGC();
            frame.ctorResult.traceGC();
        }
        coro.result.traceGC();
        coro.lastYield.traceGC();
    }
}

void VM::loadProgram(const CompiledProgram& prog) {
    program = prog;
    extraPrograms.clear();
    globals.clear();
    globalVector_.clear();
    globalNames_.clear();
    globalSlots_.clear();
    globalConstCache_.clear();
    paramSlots_.clear();
    stack.clear();
    frames.clear();
    tryHandlers.clear();
    coroutines_.clear();
    runtimeError = false;
}

// ---- parameter binding helpers ------------------------------------------
// Parameters are globals by language design. Binding writes the argument
// into the parameter's global slot; the previous slot value is saved in the
// frame so it can be restored on return (recursion-safe). The name-keyed
// `globals` map is NOT touched on the hot path; flushGlobals() syncs the
// vector into it at library call boundaries.
void VM::bindParams(CallFrame& frame, const CompiledFunction& func,
                    const std::vector<Value>& args) {
    size_t n = func.parameters.size();
    auto& slots = paramSlots_[&func];
    if (slots.empty()) {
        slots.reserve(n);
        for (const auto& p : func.parameters) {
            slots.push_back(getOrCreateSlot(p.name));
        }
    }
    frame.paramSlotsPtr = &slots;
    frame.paramSaveCount = n;
    if (n <= 4) {
        for (size_t i = 0; i < n; i++) {
            frame.paramSavesSmall[i] = globalVector_[slots[i]];
            globalVector_[slots[i]] = args[i];
        }
    } else {
        frame.paramSavesBig.resize(n);
        for (size_t i = 0; i < n; i++) {
            frame.paramSavesBig[i] = globalVector_[slots[i]];
            globalVector_[slots[i]] = args[i];
        }
    }
}

void VM::restoreParams(CallFrame& frame) {
    size_t n = frame.paramSaveCount;
    if (n == 0) {
        return;
    }
    const auto& slots = *frame.paramSlotsPtr;
    if (n <= 4) {
        for (size_t i = 0; i < n; i++) {
            globalVector_[slots[i]] = frame.paramSavesSmall[i];
        }
    } else {
        for (size_t i = 0; i < n; i++) {
            globalVector_[slots[i]] = frame.paramSavesBig[i];
        }
    }
    frame.paramSaveCount = 0;
}

// ---- global slot fast-path helpers -------------------------------------
// Global reads/writes go through a slot-indexed vector (no per-access string
// hash). `globals` (the name-keyed map) stays available for libraries and
// error paths; flushGlobals() pushes vector state back into it at call
// boundaries, which are rare compared to instructions.
size_t VM::getOrCreateSlot(const std::string& name) {
    auto it = globalSlots_.find(name);
    if (it != globalSlots_.end()) {
        return it->second;
    }
    size_t slot = globalVector_.size();
    globalSlots_[name] = slot;
    globalVector_.push_back(Value());
    globalNames_.push_back(name);
    return slot;
}

void VM::dropGlobal(const std::string& name) {
    // The slot itself is retained (value voided) so pre-bound parameter
    // slots and the constant->slot caches stay valid for the whole run.
    auto it = globalSlots_.find(name);
    if (it != globalSlots_.end()) {
        globalVector_[it->second] = Value();
    }
    globals.erase(name);
}

bool VM::setGlobalSync(const std::string& name, const Value& v) {
    auto it = globalSlots_.find(name);
    if (it == globalSlots_.end()) return false;
    globalVector_[it->second] = v;
    globals[name] = v;
    return true;
}

void VM::flushGlobals() {
    for (size_t i = 0; i < globalVector_.size(); i++) {
        globals[globalNames_[i]] = globalVector_[i];
    }
}

size_t VM::slotForConst(const CompiledFunction* fn, size_t nameIdx) {
    auto& cache = globalConstCache_[fn];
    if (cache.empty()) {
        cache.assign(fn->chunk.constants.size(), SIZE_MAX);
    }
    if (nameIdx < cache.size() && cache[nameIdx] != SIZE_MAX) {
        return cache[nameIdx];
    }
    const Value& cv = fn->chunk.constants[nameIdx];
    if (cv.getType() != Value::Type::String) return SIZE_MAX;
    auto it = globalSlots_.find(cv.asString());
    if (it == globalSlots_.end()) return SIZE_MAX;
    size_t slot = it->second;
    if (nameIdx < cache.size()) cache[nameIdx] = slot;
    return slot;
}

void VM::addProgram(const CompiledProgram& prog) {
    extraPrograms.push_back(prog);
}

const CompiledFunction* VM::findFunction(const std::string& name) const {
    auto it = program.functionIndex.find(name);
    if (it != program.functionIndex.end()) {
        return &program.functions[it->second];
    }
    for (const auto& extra : extraPrograms) {
        auto eIt = extra.functionIndex.find(name);
        if (eIt != extra.functionIndex.end()) {
            return &extra.functions[eIt->second];
        }
    }
    return nullptr;
}

const CompiledClass* VM::findClass(const std::string& name) const {
    auto it = program.classIndex.find(name);
    if (it != program.classIndex.end()) {
        return &program.classes[it->second];
    }
    for (const auto& extra : extraPrograms) {
        auto eIt = extra.classIndex.find(name);
        if (eIt != extra.classIndex.end()) {
            return &extra.classes[eIt->second];
        }
    }
    return nullptr;
}

void VM::resetStack() {
    stack.clear();
}

void VM::runtimeErr(const std::string& msg) {
    if (!runtimeError) {
        std::vector<std::string> trace;
        std::vector<size_t> offsets;
        std::vector<int> lines;
        std::vector<std::string> files;
        for (const auto& f : frames) {
            trace.push_back(f.function->name);
            // Suspended frames are paused at their OP_CALL instruction; the
            // innermost frame points at the instruction that raised the error.
            offsets.push_back(f.instrStart);
            int ln = f.function->chunk.lineAt(f.instrStart);
            lines.push_back(ln);
            files.push_back(f.function->sourceFile);
        }
        ErrorReporter::reportRuntimeError(msg, trace, offsets, lines, files);
        runtimeError = true;
    }
}

bool VM::throwValue(const Value& err) {
    if (tryHandlers.empty()) return false;

    TryHandler handler = tryHandlers.back();
    tryHandlers.pop_back();

    while (!tryHandlers.empty() && tryHandlers.back().frameIndex >= handler.frameIndex) {
        tryHandlers.pop_back();
    }
    while (frames.size() > handler.frameIndex + 1) {
        CallFrame& cur = frames.back();
        for (const auto& [name, val] : cur.savedGlobals) {
            size_t slot = getOrCreateSlot(name);
            globalVector_[slot] = val;
            globals[name] = val;
        }
        for (const auto& name : cur.newGlobals) {
            dropGlobal(name);
        }
        frames.pop_back();
    }
    if (stack.size() > handler.stackDepth) {
        stack.resize(handler.stackDepth);
    }
    {
        size_t slot = getOrCreateSlot(handler.varName);
        globalVector_[slot] = err;
        globals[handler.varName] = err;
    }
    if (!frames.empty()) {
        frames.back().ip = handler.catchAddr;
    }
    return true;
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
    // System libraries observe globals through Interpreter::currentVariables,
    // so push the slot array state into the name-keyed map before dispatch.
    flushGlobals();
    // Coroutine control functions are VM-resident (they need the execution
    // context), so they are intercepted before library dispatch.
    if (funcName == "co.create") {
        if (args.empty() || args[0].getType() != Value::Type::String) {
            runtimeErr("co.create expects a function name string as first argument");
            return Value();
        }
        std::vector<Value> cargs(args.begin() + 1, args.end());
        return coroCreate(args[0].asString(), cargs);
    }
    if (funcName == "co.resume") {
        if (args.empty() || args[0].getType() != Value::Type::Coroutine) {
            runtimeErr("co.resume expects a coroutine value");
            return Value();
        }
        if (args.size() > 2) {
            runtimeErr("co.resume expects (coroutine[, value])");
            return Value();
        }
        Value arg = args.size() == 2 ? args[1] : Value();
        // Returns the value produced by the suspended yield statement.
        return coroResume(args[0].asCoroutineId(), arg);
    }
    if (funcName == "co.status") {
        if (args.size() != 1 || args[0].getType() != Value::Type::Coroutine) {
            runtimeErr("co.status expects a coroutine value");
            return Value();
        }
        return coroStatus(args[0].asCoroutineId());
    }
    if (funcName == "co.result") {
        if (args.size() != 1 || args[0].getType() != Value::Type::Coroutine) {
            runtimeErr("co.result expects a coroutine value");
            return Value();
        }
        return coroResult(args[0].asCoroutineId());
    }

    Interpreter::currentVariables = &globals;
    Interpreter sys;
    if (sys.isSystemFunction(funcName)) {
        return sys.SystemFunctionBuildIn(funcName, args);
    }
    runtimeErr("Unknown system function: " + funcName);
    return Value();
}

bool VM::callFunction(const std::string& name, int argCount) {
    const CompiledFunction* funcPtr = findFunction(name);
    if (funcPtr == nullptr) {
        return callSystemFunction(name, argCount);
    }

    const CompiledFunction& func = *funcPtr;

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

    // Save and bind parameters as global variables (mirrors OP_CALL)
    std::vector<Value> args(argCount);
    for (size_t i = 0; i < func.parameters.size() && i < static_cast<size_t>(argCount); i++) {
        args[i] = frame.locals[i];
    }
    bindParams(frame, func, args);

    // Constructor calls: the 'this' object (locals[0]) must be pushed back
    // onto the caller stack when the constructor frame returns.
    {
        size_t lastDot = name.find_last_of('.');
        if (lastDot != std::string::npos &&
            name.substr(lastDot + 1) == "init" &&
            program.classIndex.find(name.substr(0, lastDot)) != program.classIndex.end()) {
            frame.isCtorFrame = true;
            frame.ctorResult = frame.locals[0];
        }
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
    const CompiledFunction* mainFunc = findFunction("main");
    if (mainFunc == nullptr) {
        runtimeErr("No 'main' function found");
        return;
    }

    resetStack();
    frames.clear();
    tryHandlers.clear();
    runtimeError = false;
    coroYieldRequested_ = false;

    // Initialize system libraries
    RegFunc();

    // Start with main function
    CallFrame frame;
    frame.function = mainFunc;
    frame.ip = 0;
    frame.locals.resize(mainFunc->localCount);
    frames.push_back(frame);

    runLoop();
}

// Drives the instruction loop for the currently-swapped execution context.
// Used both for the main program (frames == host context) and for resumed
// coroutines (frames == the coroutine's frozen context). Returns when the
// frame stack empties, a runtime error fires, or the coroutine yields.
void VM::runLoop() {
    // Execution loop
    bool resumed = false;
    do {
        resumed = false;
        try {
        while (!frames.empty() && !runtimeError && !coroYieldRequested_) {
        CallFrame& cf = frames.back();
        const Chunk& chunk = cf.function->chunk;

        if (cf.ip >= chunk.code.size()) {
            CallFrame doneFrame = frames.back();
            frames.pop_back();
            if (activeCoro_ >= 0 && frames.empty()) {
                // Coroutine fell off the end: mark finished with a void result
                coroutines_[activeCoro_].dead = true;
                coroutines_[activeCoro_].result = Value();
                coroYieldRequested_ = true;
                continue;
            }
            if (!frames.empty()) {
                if (doneFrame.isCtorFrame) {
                    push(doneFrame.ctorResult);
                } else {
                    push(Value()); // void return value
                }
            }
            continue;
        }

        uint8_t instruction = chunk.code[cf.ip++];
        cf.instrStart = cf.ip - 1;
        Gc::instance().checkpoint();

        switch (static_cast<OpCode>(instruction)) {
        case OpCode::OP_RETURN: {
            Value retVal = Value();
            if (!stack.empty()) {
                retVal = pop();
            }
            // Return type validation (mirrors Interpreter::executeFunction)
            {
                const CompiledFunction& retFunc = *frames.back().function;
                if (retFunc.returnType != "void") {
                    bool mismatch = false;
                    if (retVal.getType() == Value::Type::Int) mismatch = (retFunc.returnType != "int");
                    else if (retVal.getType() == Value::Type::Double) mismatch = (retFunc.returnType != "double");
                    else if (retVal.getType() == Value::Type::String) mismatch = (retFunc.returnType != "string");
                    else if (retVal.getType() == Value::Type::Array) mismatch = (retFunc.returnType != "array");
                    else if (retVal.getType() == Value::Type::Dict) mismatch = (retFunc.returnType != "dict");
                    else if (retVal.getType() == Value::Type::Object) mismatch = (retFunc.returnType != "dict");
                    else if (retVal.getType() == Value::Type::Bytes) mismatch = (retFunc.returnType != "bytes");
                    else mismatch = true; // Void or any other type
                    if (mismatch) {
                        runtimeErr("Function " + retFunc.name + " expects to return " + retFunc.returnType + " type, actually returned " +
                            (retVal.getType() == Value::Type::Void ? "void" : "other type"));
                        break;
                    }
                }
                else if (retVal.getType() != Value::Type::Void) {
                    runtimeErr("Function " + retFunc.name + " is declared void but returned a value");
                    break;
                }
            }
            // Restore globals shadowed by this frame's parameters, and
            // remove parameters that did not exist before the call (P1-1)
            if (!frames.empty()) {
                restoreParams(frames.back());
            }
            CallFrame poppedFrame = frames.back();
            frames.pop_back();
            if (activeCoro_ >= 0 && frames.empty()) {
                // Coroutine returned: mark finished with this frame's result
                coroutines_[activeCoro_].dead = true;
                coroutines_[activeCoro_].result =
                    poppedFrame.isCtorFrame ? poppedFrame.ctorResult : retVal;
                coroYieldRequested_ = true;
                break;
            }
            if (!frames.empty()) {
                if (poppedFrame.isCtorFrame) {
                    push(poppedFrame.ctorResult);
                } else {
                    push(retVal);
                }
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
        case OpCode::OP_MOD: {
            Value b = pop();
            Value a = pop();
            if (a.getType() == Value::Type::Int && b.getType() == Value::Type::Int) {
                if (b.asInt() == 0) { runtimeErr("Modulo by zero"); break; }
                push(Value(a.asInt() % b.asInt()));
            } else if (a.getType() == Value::Type::Double && b.getType() == Value::Type::Double) {
                if (b.asDouble() == 0.0) { runtimeErr("Modulo by zero"); break; }
                push(Value(std::fmod(a.asDouble(), b.asDouble())));
            } else if (a.getType() == Value::Type::Int && b.getType() == Value::Type::Double) {
                if (b.asDouble() == 0.0) { runtimeErr("Modulo by zero"); break; }
                push(Value(std::fmod(static_cast<double>(a.asInt()), b.asDouble())));
            } else if (a.getType() == Value::Type::Double && b.getType() == Value::Type::Int) {
                if (b.asInt() == 0) { runtimeErr("Modulo by zero"); break; }
                push(Value(std::fmod(a.asDouble(), static_cast<double>(b.asInt()))));
            } else {
                runtimeErr("Type mismatch in modulo");
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
            } else if (a.getType() == b.getType() &&
                       (a.getType() == Value::Type::Array ||
                        a.getType() == Value::Type::Dict ||
                        a.getType() == Value::Type::Object ||
                        a.getType() == Value::Type::Bytes)) {
                result = valuesEqual(a, b);
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
            } else if (a.getType() == b.getType() &&
                       (a.getType() == Value::Type::Array ||
                        a.getType() == Value::Type::Dict ||
                        a.getType() == Value::Type::Object ||
                        a.getType() == Value::Type::Bytes)) {
                result = !valuesEqual(a, b);
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
        case OpCode::OP_ADD_IMM: {
            int64_t imm = static_cast<int8_t>(chunk.readByte(cf.ip));
            cf.ip++;
            Value a = pop();
            if (a.getType() == Value::Type::Int) {
                push(Value(static_cast<int>(a.asInt() + imm)));
            } else if (a.getType() == Value::Type::Double) {
                push(Value(a.asDouble() + static_cast<double>(imm)));
            } else {
                runtimeErr("Type mismatch in addition");
            }
            break;
        }
        case OpCode::OP_SUB_IMM: {
            int64_t imm = static_cast<int8_t>(chunk.readByte(cf.ip));
            cf.ip++;
            Value a = pop();
            if (a.getType() == Value::Type::Int) {
                push(Value(static_cast<int>(a.asInt() - imm)));
            } else if (a.getType() == Value::Type::Double) {
                push(Value(a.asDouble() - static_cast<double>(imm)));
            } else {
                runtimeErr("Type mismatch in subtraction");
            }
            break;
        }
        case OpCode::OP_MUL_IMM: {
            int64_t imm = static_cast<int8_t>(chunk.readByte(cf.ip));
            cf.ip++;
            Value a = pop();
            if (a.getType() == Value::Type::Int) {
                push(Value(static_cast<int>(a.asInt() * imm)));
            } else if (a.getType() == Value::Type::Double) {
                push(Value(a.asDouble() * static_cast<double>(imm)));
            } else {
                runtimeErr("Type mismatch in multiplication");
            }
            break;
        }
        case OpCode::OP_DIV_IMM: {
            int64_t imm = static_cast<int8_t>(chunk.readByte(cf.ip));
            cf.ip++;
            Value a = pop();
            if (imm == 0) { runtimeErr("Division by zero"); break; }
            if (a.getType() == Value::Type::Int) {
                push(Value(static_cast<int>(a.asInt() / imm)));
            } else if (a.getType() == Value::Type::Double) {
                push(Value(a.asDouble() / static_cast<double>(imm)));
            } else {
                runtimeErr("Type mismatch in division");
            }
            break;
        }
        case OpCode::OP_MOD_IMM: {
            int64_t imm = static_cast<int8_t>(chunk.readByte(cf.ip));
            cf.ip++;
            Value a = pop();
            if (imm == 0) { runtimeErr("Modulo by zero"); break; }
            if (a.getType() == Value::Type::Int) {
                push(Value(static_cast<int>(a.asInt() % imm)));
            } else if (a.getType() == Value::Type::Double) {
                push(Value(std::fmod(a.asDouble(), static_cast<double>(imm))));
            } else {
                runtimeErr("Type mismatch in modulo");
            }
            break;
        }
        case OpCode::OP_LT_IMM:
        case OpCode::OP_GT_IMM:
        case OpCode::OP_GE_IMM:
        case OpCode::OP_LE_IMM:
        case OpCode::OP_EQ_IMM:
        case OpCode::OP_NE_IMM: {
            double imm = static_cast<double>(static_cast<int8_t>(chunk.readByte(cf.ip)));
            cf.ip++;
            Value a = pop();
            double al = (a.getType() == Value::Type::Int) ? static_cast<double>(a.asInt()) : a.asDouble();
            bool result = false;
            switch (static_cast<OpCode>(instruction)) {
            case OpCode::OP_LT_IMM: result = (al < imm); break;
            case OpCode::OP_GT_IMM: result = (al > imm); break;
            case OpCode::OP_GE_IMM: result = (al >= imm); break;
            case OpCode::OP_LE_IMM: result = (al <= imm); break;
            case OpCode::OP_EQ_IMM: result = (al == imm); break;
            case OpCode::OP_NE_IMM: result = (al != imm); break;
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
            size_t slot = getOrCreateSlot(name);
            globalVector_[slot] = val;
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
            size_t slot = slotForConst(cf.function, nameIdx);
            if (slot == SIZE_MAX) {
                runtimeErr("Undefined variable: " + chunk.constants[nameIdx].asString());
                break;
            }
            push(globalVector_[slot]);
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
            size_t slot = slotForConst(cf.function, nameIdx);
            if (slot == SIZE_MAX) {
                runtimeErr("Undefined variable: " + name);
                break;
            }
            globalVector_[slot] = val;
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
            dropGlobal(name);
            break;
        }
        case OpCode::OP_CLEAR_GLOBALS: {
            globals.clear();
            globalVector_.clear();
            globalNames_.clear();
            globalSlots_.clear();
            globalConstCache_.clear();
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

            const CompiledFunction* fnPtr = findFunction(fnName);
            if (fnPtr != nullptr) {
                const CompiledFunction& func = *fnPtr;
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
                for (size_t i = 0; i < args.size() && i < func.localCount; i++) {
                    newFrame.locals[i] = args[i];
                }

                // Save and set parameters as global variables
                bindParams(newFrame, func, args);
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
        case OpCode::OP_DICT: {
            uint8_t entryCount = chunk.readByte(cf.ip);
            cf.ip++;
            std::unordered_map<std::string, GcHandle> dict;
            for (int i = 0; i < entryCount; i++) {
                Value keyVal = pop();
                Value value = pop();
                if (keyVal.getType() != Value::Type::String) {
                    runtimeErr("Dict key must be a string");
                    break;
                }
                dict[keyVal.asString()] = Gc::instance().alloc(value);
            }
            if (!runtimeError) push(Value(dict));
            break;
        }
        case OpCode::OP_INDEX_GET: {
            Value index = pop();
            Value arr = pop();
            if (arr.getType() == Value::Type::Array) {
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
            } else if (arr.getType() == Value::Type::Dict) {
                if (index.getType() != Value::Type::String) {
                    runtimeErr("Dict index must be a string");
                    break;
                }
                const auto& dict = arr.asDict();
                auto it = dict.find(index.asString());
                if (it == dict.end()) {
                    runtimeErr("Undefined dict key: " + index.asString());
                    break;
                }
                const Value* elem = Gc::instance().deref(it->second);
                if (!elem) {
                    runtimeErr("Dangling dict element");
                    break;
                }
                push(*elem);
            } else {
                runtimeErr("Index target is not an array or dict");
            }
            break;
        }
        case OpCode::OP_INDEX_SET: {
            Value index = pop();
            Value arr = pop();
            Value value = pop();
            if (arr.getType() == Value::Type::Array) {
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
            } else if (arr.getType() == Value::Type::Dict) {
                if (index.getType() != Value::Type::String) {
                    runtimeErr("Dict index must be a string");
                    break;
                }
                arr.asDictRef()[index.asString()] = Gc::instance().alloc(value);
                push(arr);
            } else {
                runtimeErr("Index target is not an array or dict");
            }
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
        case OpCode::OP_NEW_OBJ: {
            uint16_t classIdx = chunk.readShort(cf.ip);
            cf.ip += 2;
            uint8_t argCount = chunk.readByte(cf.ip);
            cf.ip++;
            if (classIdx >= chunk.constants.size() ||
                chunk.constants[classIdx].getType() != Value::Type::String) {
                runtimeErr("Invalid class name constant");
                break;
            }
            std::string className = chunk.constants[classIdx].asString();
            const CompiledClass* clsPtr = findClass(className);
            if (clsPtr == nullptr) {
                runtimeErr("Undefined class: " + className);
                break;
            }
            const CompiledClass& cls = *clsPtr;
            if (static_cast<int>(stack.size()) < argCount) {
                runtimeErr("Stack underflow in object construction");
                break;
            }
            std::vector<Value> args(argCount);
            for (int i = argCount - 1; i >= 0; i--) {
                args[i] = pop();
            }
            Value obj = Value::makeObject(className);
            for (const auto& f : cls.fields) {
                obj.asObjectDictRef()[f.name] = Gc::instance().alloc(defaultFieldValue(f.type));
            }
            std::string initName = className + ".init";
            if (cls.hasInit) {
                const CompiledFunction* initPtr = findFunction(initName);
                if (initPtr == nullptr) {
                    runtimeErr("Class " + className + " constructor was not compiled");
                    break;
                }
                const CompiledFunction& initFunc = *initPtr;
                if (argCount + 1 != static_cast<uint8_t>(initFunc.parameters.size())) {
                    runtimeErr("Class " + className + " constructor expects " +
                        std::to_string(initFunc.parameters.size() - 1) + " arguments, got " +
                        std::to_string(argCount));
                    break;
                }
                push(obj);
                for (const auto& arg : args) push(arg);
                push(Value(initName));
                callFunction(initName, argCount + 1);
            }
            else {
                if (argCount != 0) {
                    if (argCount != static_cast<uint8_t>(cls.fields.size())) {
                        runtimeErr("Class " + className + " has no constructor; expected " +
                            std::to_string(cls.fields.size()) + " positional field arguments, got " +
                            std::to_string(argCount));
                        break;
                    }
                    for (size_t i = 0; i < cls.fields.size(); i++) {
                        obj.asObjectDictRef()[cls.fields[i].name] =
                            Gc::instance().alloc(args[i]);
                    }
                }
                push(obj);
            }
            break;
        }
        case OpCode::OP_OBJ_FIELD_GET: {
            uint16_t fieldIdx = chunk.readShort(cf.ip);
            cf.ip += 2;
            if (fieldIdx >= chunk.constants.size() ||
                chunk.constants[fieldIdx].getType() != Value::Type::String) {
                runtimeErr("Invalid field name constant");
                break;
            }
            std::string fieldName = chunk.constants[fieldIdx].asString();
            Value obj = pop();
            if (obj.getType() != Value::Type::Object) {
                runtimeErr("Field access '" + fieldName + "' on a non-object value");
                break;
            }
            std::string className = obj.asObjectClass();
            auto clsIt = program.classIndex.find(className);
            if (clsIt == program.classIndex.end()) {
                runtimeErr("Unknown class: " + className);
                break;
            }
            const CompiledClass& cls = program.classes[clsIt->second];
            bool declared = false;
            for (const auto& f : cls.fields) {
                if (f.name == fieldName) { declared = true; break; }
            }
            if (!declared) {
                runtimeErr("Undefined field '" + fieldName + "' in class '" + className + "'");
                break;
            }
            const auto& members = obj.asObjectDict();
            auto it = members.find(fieldName);
            if (it == members.end()) {
                runtimeErr("Field '" + fieldName + "' of class '" + className + "' is not initialized");
                break;
            }
            const Value* fieldVal = Gc::instance().deref(it->second);
            if (!fieldVal) {
                runtimeErr("Dangling field '" + fieldName + "' in class '" + className + "'");
                break;
            }
            push(*fieldVal);
            break;
        }
        case OpCode::OP_OBJ_FIELD_SET: {
            uint16_t fieldIdx = chunk.readShort(cf.ip);
            cf.ip += 2;
            if (fieldIdx >= chunk.constants.size() ||
                chunk.constants[fieldIdx].getType() != Value::Type::String) {
                runtimeErr("Invalid field name constant");
                break;
            }
            std::string fieldName = chunk.constants[fieldIdx].asString();
            Value obj = pop();
            Value val = pop();
            if (obj.getType() != Value::Type::Object) {
                runtimeErr("Field write '" + fieldName + "' on a non-object value");
                break;
            }
            std::string className = obj.asObjectClass();
            auto clsIt = program.classIndex.find(className);
            if (clsIt == program.classIndex.end()) {
                runtimeErr("Unknown class: " + className);
                break;
            }
            const CompiledClass& cls = program.classes[clsIt->second];
            bool declared = false;
            for (const auto& f : cls.fields) {
                if (f.name == fieldName) { declared = true; break; }
            }
            if (!declared) {
                runtimeErr("Undefined field '" + fieldName + "' in class '" + className + "'");
                break;
            }
            obj.asObjectDictRef()[fieldName] = Gc::instance().alloc(val);
            push(obj);
            break;
        }
        case OpCode::OP_OBJ_CALL: {
            uint16_t methodIdx = chunk.readShort(cf.ip);
            cf.ip += 2;
            uint8_t argCount = chunk.readByte(cf.ip);
            cf.ip++;
            if (methodIdx >= chunk.constants.size() ||
                chunk.constants[methodIdx].getType() != Value::Type::String) {
                runtimeErr("Invalid method name constant");
                break;
            }
            std::string methodName = chunk.constants[methodIdx].asString();
            if (static_cast<int>(stack.size()) < argCount + 1) {
                runtimeErr("Stack underflow in method call");
                break;
            }
            std::vector<Value> args(argCount);
            for (int i = argCount - 1; i >= 0; i--) {
                args[i] = pop();
            }
            Value obj = pop();
            if (obj.getType() != Value::Type::Object) {
                runtimeErr("Cannot call method '" + methodName + "' on a non-object value");
                break;
            }
            std::string className = obj.asObjectClass();
            std::string fullName = className + "." + methodName;
            const CompiledFunction* mPtr = findFunction(fullName);
            if (mPtr == nullptr) {
                runtimeErr("Undefined method '" + methodName + "' in class '" + className + "'");
                break;
            }
            const CompiledFunction& mFunc = *mPtr;
            if (argCount + 1 != static_cast<uint8_t>(mFunc.parameters.size())) {
                runtimeErr("Method " + fullName + " expects " +
                    std::to_string(mFunc.parameters.size() - 1) + " arguments, got " +
                    std::to_string(argCount));
                break;
            }
            push(obj);
            for (const auto& arg : args) push(arg);
            push(Value(fullName));
            callFunction(fullName, argCount + 1);
            break;
        }
        case OpCode::OP_TRY: {
            uint16_t varNameIdx = chunk.readShort(cf.ip);
            cf.ip += 2;
            int32_t catchRel = static_cast<int16_t>(chunk.readShort(cf.ip));
            cf.ip += 2;
            if (varNameIdx >= chunk.constants.size() ||
                chunk.constants[varNameIdx].getType() != Value::Type::String) {
                runtimeErr("Invalid try error variable constant");
                break;
            }
            TryHandler handler;
            handler.stackDepth = stack.size();
            handler.frameIndex = frames.size() - 1;
            handler.catchAddr = cf.ip + static_cast<size_t>(catchRel);
            handler.varName = chunk.constants[varNameIdx].asString();
            tryHandlers.push_back(handler);
            break;
        }
        case OpCode::OP_END_TRY: {
            if (!tryHandlers.empty()) tryHandlers.pop_back();
            break;
        }
        case OpCode::OP_THROW: {
            Value message = pop();
            if (!throwValue(message)) {
                runtimeErr(message.toString());
            }
            break;
        }
        case OpCode::OP_HALT: {
            frames.clear();
            if (activeCoro_ >= 0) {
                coroutines_[activeCoro_].dead = true;
                coroutines_[activeCoro_].result = Value();
                coroYieldRequested_ = true;
            }
            break;
        }
        case OpCode::OP_YIELD: {
            if (activeCoro_ < 0) {
                runtimeErr("yield is only allowed inside a coroutine");
                break;
            }
            coroutines_[activeCoro_].lastYield =
                stack.empty() ? Value() : pop();
            coroutines_[activeCoro_].yieldInstrStart = cf.instrStart;
            coroYieldRequested_ = true;
            break;
        }
        default:
            runtimeErr("Unknown opcode: " + std::to_string(instruction));
            break;
        }
        }
        } catch (const std::exception& e) {
            if (!throwValue(Value(e.what()))) {
                runtimeErr(e.what());
            } else {
                resumed = true;
            }
        }
    } while (resumed);
}

// ============================================================
// Coroutine operations (co.create / co.resume / co.status /
// co.result). Context switching is done by swapping the VM's
// active frames/stack/tryHandlers with the coroutine's frozen
// copies. The execution loop is re-entered via runLoop(); the
// host's loop is still live on the C++ call stack (OP_CALL ->
// callSystemFunction -> executeSystemCall -> coroResume).
// ============================================================
Value VM::coroCreate(const std::string& fnName, const std::vector<Value>& args) {
    const CompiledFunction* funcPtr = findFunction(fnName);
    if (funcPtr == nullptr) {
        runtimeErr("Coroutine function not found: " + fnName);
        return Value();
    }
    const CompiledFunction& func = *funcPtr;
    if (static_cast<int>(args.size()) != static_cast<int>(func.parameters.size())) {
        runtimeErr("Function " + fnName + " expects " + std::to_string(func.parameters.size())
            + " arguments, got " + std::to_string(args.size()));
        return Value();
    }

    Coroutine coro;
    CallFrame frame;
    frame.function = &func;
    frame.ip = 0;
    frame.locals.resize(func.localCount);
    for (size_t i = 0; i < args.size(); i++) {
        frame.locals[i] = args[i];
    }
    // Parameter globals are NOT bound here: binding happens per resume
    // segment (see coroResume), so concurrent coroutines never clobber
    // each other's parameters through the shared globals table.
    coro.frames.push_back(frame);
    coroutines_.push_back(std::move(coro));
    return Value::makeCoroutine(static_cast<int>(coroutines_.size()) - 1);
}

Value VM::coroResume(int coroId, const Value& arg) {
    if (coroId < 0 || coroId >= static_cast<int>(coroutines_.size())) {
        runtimeErr("Invalid coroutine handle");
        return Value();
    }
    Coroutine& coro = coroutines_[coroId];
    if (activeCoro_ >= 0) {
        runtimeErr("co.resume cannot be called from inside a coroutine");
        return Value();
    }
if (coro.dead) {
        runtimeErr("Cannot resume a finished coroutine");
        return Value();
    }
    flushGlobals();

    // Bind this coroutine's parameter globals for the segment (saved in the
    // frame and restored when the segment suspends or finishes). Every other
    // global stays shared across the whole VM by design.
    std::vector<Value> cargs;
    if (!coro.frames.empty()) {
        const CompiledFunction& fn = *coro.frames.back().function;
        cargs.resize(fn.parameters.size());
        for (size_t i = 0; i < fn.parameters.size() &&
             i < coro.frames.back().locals.size(); i++) {
            cargs[i] = coro.frames.back().locals[i];
        }
        bindParams(coro.frames.back(), fn, cargs);
    }

    // Swap the coroutine context in and run until it yields or finishes
    std::swap(frames, coro.frames);
    std::swap(stack, coro.stack);
    std::swap(tryHandlers, coro.tryHandlers);
    activeCoro_ = coroId;
    coroYieldRequested_ = false;
    runLoop();
    coro.dead = frames.empty();
    coroYieldRequested_ = false; // host loop must keep running
    std::swap(frames, coro.frames);
    std::swap(stack, coro.stack);
    std::swap(tryHandlers, coro.tryHandlers);
    activeCoro_ = -1;

// Restore the parameter globals shadowed for this segment
    if (!coro.frames.empty()) {
        restoreParams(coro.frames.back());
    }
    flushGlobals();

    return coro.dead ? coro.result : coro.lastYield;
}

Value VM::coroStatus(int coroId) const {
    if (coroId < 0 || coroId >= static_cast<int>(coroutines_.size())) {
        return Value("invalid");
    }
    const Coroutine& coro = coroutines_[coroId];
    if (coro.dead) return Value("dead");
    if (activeCoro_ == coroId) return Value("running");
    return Value("suspended");
}

Value VM::coroResult(int coroId) const {
    if (coroId < 0 || coroId >= static_cast<int>(coroutines_.size())) {
        return Value();
    }
    const Coroutine& coro = coroutines_[coroId];
    return coro.dead ? coro.result : Value();
}

// ============================================================
// Disassembler (fox -d)
// ============================================================
namespace {
const char* opcodeName(uint8_t byte) {
    switch (static_cast<OpCode>(byte)) {
    case OpCode::OP_RETURN: return "OP_RETURN";
    case OpCode::OP_CONSTANT: return "OP_CONSTANT";
    case OpCode::OP_NEGATE: return "OP_NEGATE";
    case OpCode::OP_ADD: return "OP_ADD";
    case OpCode::OP_SUB: return "OP_SUB";
    case OpCode::OP_MUL: return "OP_MUL";
    case OpCode::OP_DIV: return "OP_DIV";
    case OpCode::OP_TRUE: return "OP_TRUE";
    case OpCode::OP_FALSE: return "OP_FALSE";
    case OpCode::OP_NIL: return "OP_NIL";
    case OpCode::OP_NOT: return "OP_NOT";
    case OpCode::OP_EQ: return "OP_EQ";
    case OpCode::OP_NE: return "OP_NE";
    case OpCode::OP_GT: return "OP_GT";
    case OpCode::OP_LT: return "OP_LT";
    case OpCode::OP_GE: return "OP_GE";
    case OpCode::OP_LE: return "OP_LE";
    case OpCode::OP_PRINT: return "OP_PRINT";
    case OpCode::OP_PRINTLN: return "OP_PRINTLN";
    case OpCode::OP_POP: return "OP_POP";
    case OpCode::OP_DEF_GLOBAL: return "OP_DEF_GLOBAL";
    case OpCode::OP_GET_GLOBAL: return "OP_GET_GLOBAL";
    case OpCode::OP_SET_GLOBAL: return "OP_SET_GLOBAL";
    case OpCode::OP_GET_LOCAL: return "OP_GET_LOCAL";
    case OpCode::OP_SET_LOCAL: return "OP_SET_LOCAL";
    case OpCode::OP_JMP: return "OP_JMP";
    case OpCode::OP_JMP_IF_FALSE: return "OP_JMP_IF_FALSE";
    case OpCode::OP_LOOP: return "OP_LOOP";
    case OpCode::OP_CALL: return "OP_CALL";
    case OpCode::OP_INPUT: return "OP_INPUT";
    case OpCode::OP_CAST_INT: return "OP_CAST_INT";
    case OpCode::OP_CAST_DOUBLE: return "OP_CAST_DOUBLE";
    case OpCode::OP_ARRAY: return "OP_ARRAY";
    case OpCode::OP_INDEX_GET: return "OP_INDEX_GET";
    case OpCode::OP_INDEX_SET: return "OP_INDEX_SET";
    case OpCode::OP_AND: return "OP_AND";
    case OpCode::OP_OR: return "OP_OR";
    case OpCode::OP_ENDLN: return "OP_ENDLN";
    case OpCode::OP_EXIT: return "OP_EXIT";
    case OpCode::OP_IMPORT: return "OP_IMPORT";
    case OpCode::OP_NEW: return "OP_NEW";
    case OpCode::OP_UNSET_GLOBAL: return "OP_UNSET_GLOBAL";
    case OpCode::OP_CLEAR_GLOBALS: return "OP_CLEAR_GLOBALS";
    case OpCode::OP_MOD: return "OP_MOD";
    case OpCode::OP_DICT: return "OP_DICT";
    case OpCode::OP_TRY: return "OP_TRY";
    case OpCode::OP_END_TRY: return "OP_END_TRY";
    case OpCode::OP_THROW: return "OP_THROW";
    case OpCode::OP_NEW_OBJ: return "OP_NEW_OBJ";
    case OpCode::OP_OBJ_FIELD_GET: return "OP_OBJ_FIELD_GET";
    case OpCode::OP_OBJ_FIELD_SET: return "OP_OBJ_FIELD_SET";
    case OpCode::OP_OBJ_CALL: return "OP_OBJ_CALL";
    case OpCode::OP_YIELD: return "OP_YIELD";
    case OpCode::OP_ADD_IMM: return "OP_ADD_IMM";
    case OpCode::OP_SUB_IMM: return "OP_SUB_IMM";
    case OpCode::OP_MUL_IMM: return "OP_MUL_IMM";
    case OpCode::OP_DIV_IMM: return "OP_DIV_IMM";
    case OpCode::OP_MOD_IMM: return "OP_MOD_IMM";
    case OpCode::OP_LT_IMM: return "OP_LT_IMM";
    case OpCode::OP_GT_IMM: return "OP_GT_IMM";
    case OpCode::OP_GE_IMM: return "OP_GE_IMM";
    case OpCode::OP_LE_IMM: return "OP_LE_IMM";
    case OpCode::OP_EQ_IMM: return "OP_EQ_IMM";
    case OpCode::OP_NE_IMM: return "OP_NE_IMM";
    case OpCode::OP_HALT: return "OP_HALT";
    default: return "OP_UNKNOWN";
    }
}

std::string disasmValue(const Value& v) {
    switch (v.getType()) {
    case Value::Type::String: return "\"" + v.asString() + "\"";
    case Value::Type::Array: return "[array]";
    case Value::Type::Bytes: return "[bytes]";
    case Value::Type::Dict: return "[dict]";
    case Value::Type::Void: return "void";
    default: return v.toString();
    }
}

void printAddr(std::ostream& out, size_t addr) {
    char buf[16];
    snprintf(buf, sizeof(buf), "0x%04zX", addr);
    out << buf;
}
} // namespace

void disassembleProgram(const CompiledProgram& prog, std::ostream& out) {
    if (!prog.imports.empty()) {
        out << "== Imports ==" << std::endl;
        for (const auto& imp : prog.imports) {
            out << "  import " << imp.libName;
            if (!imp.alias.empty() && imp.alias != imp.libName) out << " -> " << imp.alias;
            out << std::endl;
        }
        out << std::endl;
    }

    for (const auto& func : prog.functions) {
        out << "=== " << func.name << " -> " << func.returnType << " (";
        for (size_t p = 0; p < func.parameters.size(); p++) {
            if (p > 0) out << ", ";
            out << func.parameters[p].name << " : " << func.parameters[p].type;
        }
        out << " | " << func.localCount << " locals) ===" << std::endl;

        const Chunk& chunk = func.chunk;

        // Collect jump targets so they can be marked as labels
        std::unordered_set<size_t> targets;
        size_t ip = 0;
        while (ip < chunk.code.size()) {
            OpCode op = static_cast<OpCode>(chunk.code[ip]);
            ip++;
            switch (op) {
            case OpCode::OP_JMP:
            case OpCode::OP_JMP_IF_FALSE:
            case OpCode::OP_LOOP: {
                int16_t off = static_cast<int16_t>(chunk.readShort(ip));
                targets.insert(ip + 2 + static_cast<size_t>(off));
                ip += 2;
                break;
            }
            case OpCode::OP_CONSTANT:
            case OpCode::OP_DEF_GLOBAL:
            case OpCode::OP_GET_GLOBAL:
            case OpCode::OP_SET_GLOBAL:
            case OpCode::OP_UNSET_GLOBAL:
                ip += 2;
                break;
            case OpCode::OP_GET_LOCAL:
            case OpCode::OP_SET_LOCAL:
            case OpCode::OP_CALL:
            case OpCode::OP_ARRAY:
            case OpCode::OP_DICT:
            case OpCode::OP_ADD_IMM:
            case OpCode::OP_SUB_IMM:
            case OpCode::OP_MUL_IMM:
            case OpCode::OP_DIV_IMM:
            case OpCode::OP_MOD_IMM:
            case OpCode::OP_LT_IMM:
            case OpCode::OP_GT_IMM:
            case OpCode::OP_GE_IMM:
            case OpCode::OP_LE_IMM:
            case OpCode::OP_EQ_IMM:
            case OpCode::OP_NE_IMM:
                ip += 1;
                break;
            case OpCode::OP_OBJ_FIELD_GET:
            case OpCode::OP_OBJ_FIELD_SET:
                ip += 2;
                break;
            case OpCode::OP_NEW_OBJ:
            case OpCode::OP_OBJ_CALL:
                ip += 3;
                break;
            case OpCode::OP_TRY:
                ip += 4;
                break;
            default:
                break;
            }
        }

        // Decode instructions
        ip = 0;
        int lastConstIdx = -1;
        while (ip < chunk.code.size()) {
            if (targets.count(ip) > 0) out << "  ==> ";
            else out << "      ";
            printAddr(out, ip);
            out << "  ";

            OpCode op = static_cast<OpCode>(chunk.code[ip]);
            ip++;
            switch (op) {
            case OpCode::OP_CONSTANT: {
                uint16_t idx = chunk.readShort(ip);
                ip += 2;
                lastConstIdx = idx;
                out << "OP_CONSTANT  #" << idx;
                if (idx < chunk.constants.size()) out << "  " << disasmValue(chunk.constants[idx]);
                break;
            }
            case OpCode::OP_DEF_GLOBAL: {
                uint16_t idx = chunk.readShort(ip);
                ip += 2;
                out << "OP_DEF_GLOBAL  ";
                if (idx < chunk.constants.size()) out << disasmValue(chunk.constants[idx]);
                break;
            }
            case OpCode::OP_GET_GLOBAL: {
                uint16_t idx = chunk.readShort(ip);
                ip += 2;
                out << "OP_GET_GLOBAL  ";
                if (idx < chunk.constants.size()) out << disasmValue(chunk.constants[idx]);
                break;
            }
            case OpCode::OP_SET_GLOBAL: {
                uint16_t idx = chunk.readShort(ip);
                ip += 2;
                out << "OP_SET_GLOBAL  ";
                if (idx < chunk.constants.size()) out << disasmValue(chunk.constants[idx]);
                break;
            }
            case OpCode::OP_UNSET_GLOBAL: {
                uint16_t idx = chunk.readShort(ip);
                ip += 2;
                out << "OP_UNSET_GLOBAL  ";
                if (idx < chunk.constants.size()) out << disasmValue(chunk.constants[idx]);
                break;
            }
            case OpCode::OP_JMP:
            case OpCode::OP_JMP_IF_FALSE:
            case OpCode::OP_LOOP: {
                int16_t off = static_cast<int16_t>(chunk.readShort(ip));
                size_t target = ip + 2 + static_cast<size_t>(off);
                ip += 2;
                out << opcodeName(static_cast<uint8_t>(op)) << "  -> ";
                printAddr(out, target);
                break;
            }
            case OpCode::OP_GET_LOCAL:
            case OpCode::OP_SET_LOCAL: {
                uint8_t slot = chunk.readByte(ip);
                ip += 1;
                out << opcodeName(static_cast<uint8_t>(op)) << "  slot " << static_cast<int>(slot);
                break;
            }
            case OpCode::OP_CALL: {
                uint8_t argc = chunk.readByte(ip);
                ip += 1;
                out << "OP_CALL  " << static_cast<int>(argc) << " arg(s)";
                if (lastConstIdx >= 0 && lastConstIdx < static_cast<int>(chunk.constants.size()) &&
                    chunk.constants[lastConstIdx].getType() == Value::Type::String) {
                    out << "  " << disasmValue(chunk.constants[lastConstIdx]);
                }
                break;
            }
            case OpCode::OP_ARRAY: {
                uint8_t n = chunk.readByte(ip);
                ip += 1;
                out << "OP_ARRAY  " << static_cast<int>(n) << " elem(s)";
                break;
            }
            case OpCode::OP_ADD_IMM:
            case OpCode::OP_SUB_IMM:
            case OpCode::OP_MUL_IMM:
            case OpCode::OP_DIV_IMM:
            case OpCode::OP_MOD_IMM:
            case OpCode::OP_LT_IMM:
            case OpCode::OP_GT_IMM:
            case OpCode::OP_GE_IMM:
            case OpCode::OP_LE_IMM:
            case OpCode::OP_EQ_IMM:
            case OpCode::OP_NE_IMM: {
                int8_t imm = static_cast<int8_t>(chunk.readByte(ip));
                ip += 1;
                out << opcodeName(static_cast<uint8_t>(op)) << "  " << static_cast<int>(imm);
                break;
            }
            case OpCode::OP_OBJ_FIELD_GET:
            case OpCode::OP_OBJ_FIELD_SET: {
                uint16_t idx = chunk.readShort(ip);
                ip += 2;
                out << opcodeName(static_cast<uint8_t>(op)) << "  ";
                if (idx < chunk.constants.size()) out << disasmValue(chunk.constants[idx]);
                break;
            }
            case OpCode::OP_NEW_OBJ: {
                uint16_t idx = chunk.readShort(ip);
                ip += 2;
                uint8_t argc = chunk.readByte(ip);
                ip += 1;
                out << "OP_NEW_OBJ  " << static_cast<int>(argc) << " arg(s)  ";
                if (idx < chunk.constants.size()) out << disasmValue(chunk.constants[idx]);
                break;
            }
            case OpCode::OP_OBJ_CALL: {
                uint16_t idx = chunk.readShort(ip);
                ip += 2;
                uint8_t argc = chunk.readByte(ip);
                ip += 1;
                out << "OP_OBJ_CALL  " << static_cast<int>(argc) << " arg(s)  ";
                if (idx < chunk.constants.size()) out << disasmValue(chunk.constants[idx]);
                break;
            }
            case OpCode::OP_DICT: {
                uint8_t n = chunk.readByte(ip);
                ip += 1;
                out << "OP_DICT  " << static_cast<int>(n) << " entry(s)";
                break;
            }
            case OpCode::OP_TRY: {
                uint16_t varIdx = chunk.readShort(ip);
                ip += 2;
                int16_t rel = static_cast<int16_t>(chunk.readShort(ip));
                ip += 2;
                out << "OP_TRY  var#";
                out << varIdx;
                out << "  catch +" << rel;
                break;
            }
            default:
                out << opcodeName(static_cast<uint8_t>(op));
                break;
            }
            out << std::endl;
        }
        out << std::endl;
    }
}

