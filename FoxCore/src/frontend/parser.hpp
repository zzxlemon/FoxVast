#pragma once
#include "lexer.hpp"
#include "token.hpp"
#include "value.hpp"
#include "function.hpp"
#include "ast.hpp"
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>
#include <stdexcept>

struct GotoException : std::exception {
    std::string label;
    explicit GotoException(const std::string& l) : label(l) {}
    const char* what() const noexcept override { return label.c_str(); }
};

struct BreakException : std::exception {
    const char* what() const noexcept override { return "break outside loop"; }
};

struct ContinueException : std::exception {
    const char* what() const noexcept override { return "continue outside loop"; }
};

struct LangErrorException : std::exception {
    std::string message;
    explicit LangErrorException(const std::string& m) : message(m) {}
    const char* what() const noexcept override { return message.c_str(); }
};

// Cross-file import support: 'import "file.fox"' statements are recorded here
// (resolved to full paths) while parsing; the interpreter/compiler then merge
// the functions of every imported file. import_base_file is the file currently
// being parsed, used to resolve relative import paths.
extern std::vector<std::string> imported_source_files;
extern std::string import_base_file;

struct ClassField {
    std::string name;
    std::string type;
};

struct ClassDef {
    std::string name;
    bool isStruct = false;
    std::vector<ClassField> fields;
    std::vector<Function> methods; // methods with implicit 'this' as first param
    bool hasInit = false;
    Function initFunc;             // constructor (this + declared params)
};

// Class/struct definitions collected while parsing. Cleared by the interpreter
// and the bytecode compiler before each program parse, then serialized into
// .fc files for the VM.
extern std::unordered_map<std::string, ClassDef> g_classRegistry;

// Type-default value for a field declaration ("int" -> 0, etc.)
Value defaultFieldValue(const std::string& type);

// Object member helpers shared by the interpreter and the AST evaluator.
// Each returns false when 'dottedName' does NOT refer to an object member
// (so callers can fall back to library/name resolution).
bool readObjectMember(const std::unordered_map<std::string, Value>& variables,
    const std::string& dottedName, Value& out);
bool assignObjectMember(std::unordered_map<std::string, Value>& variables,
    const std::string& dottedName, const Value& value);
bool callObjectMethod(std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions,
    const std::string& dottedName, const std::vector<Value>& argVals, Value& out);

struct StmtHandler {
    virtual ~StmtHandler() = default;
    virtual void onPrint(std::unique_ptr<Expr> arg) = 0;
    virtual void onPrintln(std::unique_ptr<Expr> arg) = 0;
    virtual void onExit(std::unique_ptr<Expr> arg) = 0;
    virtual void onFree(const std::string& varName) = 0;
    virtual void onFreeAll() = 0;
    virtual Value onRet(std::unique_ptr<Expr> arg) = 0;
    virtual void onEndl() = 0;
    virtual void onInput(const std::string& varName) = 0;
    virtual void onCall(const std::string& name, std::vector<std::unique_ptr<Expr>> args) = 0;
    virtual void onAssign(const std::string& name, std::unique_ptr<Expr> expr) = 0;
    virtual void onIndexAssign(const std::string& name, std::unique_ptr<Expr> index, std::unique_ptr<Expr> value) = 0;
    virtual void onIf(IfStatement ifStmt) = 0;
    virtual void onWhile(WhileStatement whileStmt) = 0;
    virtual void onFor(ForStatement forStmt) = 0;
    virtual void onTry(TryStatement tryStmt) = 0;
    virtual void onBreak() = 0;
    virtual void onContinue() = 0;
    virtual void onError(std::unique_ptr<Expr> message) = 0;
    virtual void onYield(std::unique_ptr<Expr> value) = 0;
    virtual void onFnLabel(const std::string& name) = 0;
    virtual void onGoto(const std::string& name) = 0;
};

class Parser {
private:
    Lexer funcLexer;
    Token funcCurrentToken;
    std::vector<Function> tempFunctions;
    
    static void eat(Lexer& lexer, Token& currentToken, TokenT expectedType);
    
    static std::unique_ptr<Expr> parsePrimary(Lexer& lexer, Token& currentToken);
    static std::unique_ptr<Expr> parsePostfix(Lexer& lexer, Token& currentToken, std::unique_ptr<Expr> expr);
    static std::unique_ptr<Expr> parseTerm(Lexer& lexer, Token& currentToken);
    static std::unique_ptr<Expr> parseAdd(Lexer& lexer, Token& currentToken); 
    
    static void parseAssignment(Lexer& lexer, Token& currentToken,
        std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions);
        
    static void parsePrint(Lexer& lexer, Token& currentToken,
        std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions);
        
    static void parseEndl(Lexer& lexer, Token& currentToken,
        std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions);
        
    static void parseExit(Lexer& lexer, Token& currentToken,
        std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions);
        
    static Value parseRet(Lexer& lexer, Token& currentToken,
        std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions);
        
    static void parseInputStatement(Lexer& lexer, Token& currentToken,
        std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions);

    static std::unique_ptr<Expr> parseCastExpr(Lexer& lexer, Token& currentToken, CastType castType);
    
    static std::string parseSingleStatement(Lexer& lexer, Token& currentToken);
    
    void parseFunction();
    // Parses 'name(params) -> type { body }' with the lexer already positioned
    // after the 'func' keyword. Used by top-level functions and class methods.
    Function parseFunctionRest();
    void parseClassDef();
    
    static std::unique_ptr<Expr> parseCompare(Lexer& lexer, Token& currentToken);
    
    static std::unique_ptr<Expr> parseCondition(Lexer& lexer, Token& currentToken);
    
    static IfStatement parseIfStatement(Lexer& lexer, Token& currentToken);
    
    static WhileStatement parseWhileStatement(Lexer& lexer, Token& currentToken);
    
    static ForStatement parseForStatement(Lexer& lexer, Token& currentToken);

    static TryStatement parseTryStatement(Lexer& lexer, Token& currentToken);
    
    static void parseImportStatement(Lexer& lexer, Token& currentToken,
        std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions);

public:
    std::unordered_map<std::string, Value>& variables;
    std::unordered_map<std::string, Function>& functions;
    std::unordered_map<std::string, bool> importedLibraries;

    static void skipWhitespace(Lexer& lexer, Token& currentToken);
    static std::unique_ptr<Expr> parseExpr(Lexer& lexer, Token& currentToken);

    static void parseLine(const std::string& line, StmtHandler& handler);

    static Value executeIfStatement(const IfStatement& ifStmt,
        std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions);
    static Value executeWhileStatement(const WhileStatement& whileStmt,
        std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions);
    static Value executeForStatement(const ForStatement& forStmt,
        std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions);

    Parser(const std::string& src, std::unordered_map<std::string, Value>& vars,
        std::unordered_map<std::string, Function>& funcs);
    void parseAllFunctions();
    static Value parseLine(const std::string& line,
        std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions);
    static void resetNewAllocBytes();
    static bool checkNewAllocBytes(int size);

    // Per-function-call stack limit for new() allocations (P3-5)
    static constexpr int MAX_FUNC_NEW_BYTES = 512;

    // Compile a single source line into a Stmt node (no re-parsing at runtime)
    static std::unique_ptr<Stmt> parseLineToStmt(const std::string& line);

    // Compile a body of source lines into Stmt nodes
    static std::vector<std::unique_ptr<Stmt>> compileBody(const std::vector<std::string>& lines);
};
