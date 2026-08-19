#pragma once
#include "value.hpp"
#include "token.hpp"
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>

class Interpreter;
struct Function;
struct CompiledFunction;
enum class CastType { Int, Double };

enum class CompareType {
    EQ, NE, GT, LT, GE, LE
};

class Expr {
public:
    virtual ~Expr() = default;
    virtual Value evaluate(std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions) = 0;
    virtual Value::Type compileBytecode(CompiledFunction& cf,
        std::unordered_map<std::string, Value::Type>& varTypes) const = 0;
};

class Stmt {
public:
    virtual ~Stmt() = default;
    virtual Value execute(std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions) = 0;
};

class IdentifierExpr : public Expr {
public:
    std::string name;
    explicit IdentifierExpr(const std::string& n);
    Value evaluate(std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions) override;
    Value::Type compileBytecode(CompiledFunction& cf,
        std::unordered_map<std::string, Value::Type>& varTypes) const override;
};

class NumberExpr : public Expr {
public:
    int value;
    explicit NumberExpr(int v);
    Value evaluate(std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions) override;
    Value::Type compileBytecode(CompiledFunction& cf,
        std::unordered_map<std::string, Value::Type>& varTypes) const override;
};

class DoubleExpr : public Expr {
public:
    double value;
    explicit DoubleExpr(double v);
    Value evaluate(std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions) override;
    Value::Type compileBytecode(CompiledFunction& cf,
        std::unordered_map<std::string, Value::Type>& varTypes) const override;
};

class StringExpr : public Expr {
public:
    std::string value;
    explicit StringExpr(const std::string& v);
    Value evaluate(std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions) override;
    Value::Type compileBytecode(CompiledFunction& cf,
        std::unordered_map<std::string, Value::Type>& varTypes) const override;
};

class ArrayExpr : public Expr {
public:
    std::vector<std::unique_ptr<Expr>> elements;
    explicit ArrayExpr(std::vector<std::unique_ptr<Expr>>&& elems);
    Value evaluate(std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions) override;
    Value::Type compileBytecode(CompiledFunction& cf,
        std::unordered_map<std::string, Value::Type>& varTypes) const override;
};

class DictExpr : public Expr {
public:
    std::vector<std::pair<std::string, std::unique_ptr<Expr>>> entries;
    explicit DictExpr(std::vector<std::pair<std::string, std::unique_ptr<Expr>>>&& e);
    Value evaluate(std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions) override;
    Value::Type compileBytecode(CompiledFunction& cf,
        std::unordered_map<std::string, Value::Type>& varTypes) const override;
};

class IndexExpr : public Expr {
public:
    std::unique_ptr<Expr> arrayExpr;
    std::unique_ptr<Expr> indexExpr;
    IndexExpr(std::unique_ptr<Expr> arr, std::unique_ptr<Expr> idx);
    Value evaluate(std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions) override;
    Value::Type compileBytecode(CompiledFunction& cf,
        std::unordered_map<std::string, Value::Type>& varTypes) const override;
};

class CallExpr : public Expr {
public:
    std::string funcName;
    std::vector<std::unique_ptr<Expr>> args; 
    explicit CallExpr(const std::string& name);
    CallExpr(const std::string& name, std::vector<std::unique_ptr<Expr>>&& arguments);
    Value evaluate(std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions) override;
    Value::Type compileBytecode(CompiledFunction& cf,
        std::unordered_map<std::string, Value::Type>& varTypes) const override;
};

class BinaryExpr : public Expr {
public:
    std::unique_ptr<Expr> left;
    TokenT op;
    std::unique_ptr<Expr> right;
    BinaryExpr(std::unique_ptr<Expr> l, TokenT o, std::unique_ptr<Expr> r);
    Value evaluate(std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions) override;
    Value::Type compileBytecode(CompiledFunction& cf,
        std::unordered_map<std::string, Value::Type>& varTypes) const override;
};

class InputExpr : public Expr {
public:
    Value evaluate(std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions) override;
    Value::Type compileBytecode(CompiledFunction& cf,
        std::unordered_map<std::string, Value::Type>& varTypes) const override;
};

class NewExpr : public Expr {
public:
    std::unique_ptr<Expr> sizeExpr;
    explicit NewExpr(std::unique_ptr<Expr> size) : sizeExpr(std::move(size)) {}
    Value evaluate(std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions) override;
    Value::Type compileBytecode(CompiledFunction& cf,
        std::unordered_map<std::string, Value::Type>& varTypes) const override;
};

class ObjectNewExpr : public Expr {
public:
    std::string className;
    std::vector<std::unique_ptr<Expr>> args;
    ObjectNewExpr(const std::string& cn, std::vector<std::unique_ptr<Expr>>&& a);
    Value evaluate(std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions) override;
    Value::Type compileBytecode(CompiledFunction& cf,
        std::unordered_map<std::string, Value::Type>& varTypes) const override;
};

class UnaryExpr : public Expr {
public:
    TokenT op;
    std::unique_ptr<Expr> operand;
    UnaryExpr(TokenT o, std::unique_ptr<Expr> e) : op(o), operand(std::move(e)) {}
    Value evaluate(std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions) override;
    Value::Type compileBytecode(CompiledFunction& cf,
        std::unordered_map<std::string, Value::Type>& varTypes) const override;
};

class CastExpr : public Expr {
public:
    CastType castType;
    std::unique_ptr<Expr> expr; 
    CastExpr(CastType type, std::unique_ptr<Expr> e) : castType(type), expr(std::move(e)) {}
    Value evaluate(std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions) override;
    Value::Type compileBytecode(CompiledFunction& cf,
        std::unordered_map<std::string, Value::Type>& varTypes) const override;
};

class CompareExpr : public Expr {
public:
    std::unique_ptr<Expr> left;
    CompareType op;
    std::unique_ptr<Expr> right;
    CompareExpr(std::unique_ptr<Expr> l, CompareType o, std::unique_ptr<Expr> r)
        : left(std::move(l)), op(o), right(std::move(r)) {
    }
    Value evaluate(std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions) override;
    Value::Type compileBytecode(CompiledFunction& cf,
        std::unordered_map<std::string, Value::Type>& varTypes) const override;
};

class ConditionExpr : public Expr {
public:
    std::unique_ptr<Expr> left;
    TokenT op; 
    std::unique_ptr<Expr> right;
    ConditionExpr(std::unique_ptr<Expr> l, TokenT o, std::unique_ptr<Expr> r)
        : left(std::move(l)), op(o), right(std::move(r)) {
    }
    Value evaluate(std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions) override;
    Value::Type compileBytecode(CompiledFunction& cf,
        std::unordered_map<std::string, Value::Type>& varTypes) const override;
};

// ============================================================
// Statement nodes (pre-compiled, no re-parsing at runtime)
// ============================================================

class PrintStmt : public Stmt {
public:
    std::unique_ptr<Expr> arg;
    explicit PrintStmt(std::unique_ptr<Expr> a);
    Value execute(std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions) override;
};

class PrintlnStmt : public Stmt {
public:
    std::unique_ptr<Expr> arg;
    explicit PrintlnStmt(std::unique_ptr<Expr> a);
    Value execute(std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions) override;
};

class ExitStmt : public Stmt {
public:
    std::unique_ptr<Expr> arg;
    explicit ExitStmt(std::unique_ptr<Expr> a);
    Value execute(std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions) override;
};

class RetStmt : public Stmt {
public:
    std::unique_ptr<Expr> arg;
    bool hasArg;
    explicit RetStmt(std::unique_ptr<Expr> a);
    Value execute(std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions) override;
};

class EndlStmt : public Stmt {
public:
    Value execute(std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions) override;
};

class InputStmt : public Stmt {
public:
    std::string varName;
    explicit InputStmt(const std::string& name);
    Value execute(std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions) override;
};

class CallStmt : public Stmt {
public:
    std::string funcName;
    std::vector<std::unique_ptr<Expr>> args;
    CallStmt(const std::string& name, std::vector<std::unique_ptr<Expr>> arguments);
    Value execute(std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions) override;
};

class AssignStmt : public Stmt {
public:
    std::string varName;
    std::unique_ptr<Expr> expr;
    AssignStmt(const std::string& name, std::unique_ptr<Expr> e);
    Value execute(std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions) override;
};

class IndexAssignStmt : public Stmt {
public:
    std::string varName;
    std::unique_ptr<Expr> index;
    std::unique_ptr<Expr> value;
    IndexAssignStmt(const std::string& name, std::unique_ptr<Expr> idx, std::unique_ptr<Expr> val);
    Value execute(std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions) override;
};

class IfStmt : public Stmt {
public:
    std::string condition;
    std::vector<std::unique_ptr<Stmt>> body;
    std::vector<std::unique_ptr<Stmt>> elseBody;
    IfStmt(const std::string& cond, std::vector<std::unique_ptr<Stmt>>&& b,
           std::vector<std::unique_ptr<Stmt>>&& eb);
    Value execute(std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions) override;
};

class WhileStmt : public Stmt {
public:
    std::string condition;
    std::vector<std::unique_ptr<Stmt>> body;
    WhileStmt(const std::string& cond, std::vector<std::unique_ptr<Stmt>>&& b);
    Value execute(std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions) override;
};

class ForStmt : public Stmt {
public:
    std::string init;
    std::string condition;
    std::string iter;
    std::vector<std::unique_ptr<Stmt>> body;
    ForStmt(const std::string& i, const std::string& c, const std::string& it,
            std::vector<std::unique_ptr<Stmt>>&& b);
    Value execute(std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions) override;
};

class LabelStmt : public Stmt {
public:
    std::string name;
    explicit LabelStmt(const std::string& n);
    Value execute(std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions) override;
};

class GotoStmt : public Stmt {
public:
    std::string label;
    explicit GotoStmt(const std::string& l);
    Value execute(std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions) override;
};

class BreakStmt : public Stmt {
public:
    Value execute(std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions) override;
};

class ContinueStmt : public Stmt {
public:
    Value execute(std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions) override;
};

class ErrorStmt : public Stmt {
public:
    std::unique_ptr<Expr> message;
    explicit ErrorStmt(std::unique_ptr<Expr> m);
    Value execute(std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions) override;
};

class YieldStmt : public Stmt {
public:
    std::unique_ptr<Expr> value; // may be null for bare "yield"
    explicit YieldStmt(std::unique_ptr<Expr> v);
    Value execute(std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions) override;
};

class TryStmt : public Stmt {
public:
    std::string errorVar;
    std::vector<std::unique_ptr<Stmt>> body;
    std::vector<std::unique_ptr<Stmt>> catchBody;
    TryStmt(const std::string& var, std::vector<std::unique_ptr<Stmt>>&& b,
            std::vector<std::unique_ptr<Stmt>>&& cb);
    Value execute(std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions) override;
};

// Keep old struct names as aliases for backward-compat during transition
// (they are no longer used internally, BytecodeCompiler gets updated separately)
// Parsing intermediates �� store raw source lines, converted to Stmt nodes later
struct IfStatement {
    std::string condition;
    std::vector<std::string> body;
    std::vector<std::string> elseBody;
};

struct WhileStatement {
    std::string condition;
    std::vector<std::string> body;
};

struct ForStatement {
    std::string init;
    std::string condition;
    std::string iter;
    std::vector<std::string> body;
};

struct TryStatement {
    std::string errorVar;
    std::vector<std::string> body;
    std::vector<std::string> catchBody;
};

class FreeStmt : public Stmt {
public:
    std::string varName;
    explicit FreeStmt(const std::string& name);
    Value execute(std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions) override;
};

class FreeAllStmt : public Stmt {
public:
    FreeAllStmt() = default;
    Value execute(std::unordered_map<std::string, Value>& variables,
        std::unordered_map<std::string, Function>& functions) override;
};
