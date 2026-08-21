#include "parser.hpp"
#include "../util/common.hpp"
#include "../interpreter/library_manager.hpp" 
#include "../interpreter/interpreter.hpp"
#include <iostream>
#include <algorithm>
#include <cstdlib>

std::vector<std::tuple<std::string, std::string, std::string>> imported_source_files;
std::string import_base_file;
std::string import_prefix;
std::string import_alias;
std::unordered_map<std::string, ClassDef> g_classRegistry;

namespace {
std::string dirOf(const std::string& path) {
    size_t sep = path.find_last_of("/\\");
    if (sep == std::string::npos) return ".";
    if (sep == 0) return path.substr(0, 1);
    return path.substr(0, sep);
}
std::string fileStemOf(const std::string& path) {
    size_t slash = path.find_last_of("/\\");
    size_t start = (slash == std::string::npos) ? 0 : slash + 1;
    size_t dot = path.rfind('.');
    size_t end = (dot != std::string::npos && dot > start) ? dot : path.size();
    return path.substr(start, end - start);
}
}

// Resolve an 'import "..."' path relative to the importing file's directory
// (falling back to the current working directory).
std::string resolveImportPath(const std::string& importPath) {
    if (!importPath.empty() && (importPath[1] == ':' || importPath[0] == '/' || importPath[0] == '\\')) {
        return importPath; // already absolute
    }
    if (!import_base_file.empty()) {
        std::string candidate = dirOf(import_base_file) + "/" + importPath;
        if (fileExists(candidate)) return candidate;
    }
    if (fileExists(importPath)) return importPath;
    return importPath; // caller reports the error
}

static int g_funcNewAllocBytes = -1;

void Parser::resetNewAllocBytes() {
    g_funcNewAllocBytes = 0;
}

bool Parser::checkNewAllocBytes(int size) {
    if (g_funcNewAllocBytes < 0) {
        return true;
    }
    if (g_funcNewAllocBytes + size > MAX_FUNC_NEW_BYTES) {
        throw std::runtime_error(
            "Function stack memory exceeded. new() total would be " +
            std::to_string(g_funcNewAllocBytes + size) +
            " bytes, but func stack limit is " +
            std::to_string(MAX_FUNC_NEW_BYTES) +
            " bytes. Define this variable outside the function (heap).");
    }
    g_funcNewAllocBytes += size;
    return true;
}

static std::string makeParseError(const Token& token, const std::string& message) {
    return "Syntax error: " + token.position() + ": " + message;
}

static const char* tokenTypeName(TokenT type) {
    switch (type) {
    case TOKEN_EOF: return "end of file";
    case TOKEN_NEWLINE: return "newline";
    case TOKEN_IDENTIFIER: return "identifier";
    case TOKEN_NUMBER: return "number";
    case TOKEN_DOUBLE_NUM: return "double";
    case TOKEN_STRING: return "string";
    case TOKEN_PLUS: return "'+'";
    case TOKEN_MINUS: return "'-'";
    case TOKEN_EQUAL: return "'='";
    case TOKEN_LPAREN: return "'('";
    case TOKEN_RPAREN: return "')'";
    case TOKEN_LBRACE: return "'{'";
    case TOKEN_RBRACE: return "'}'";
    case TOKEN_LBRACKET: return "'['";
    case TOKEN_RBRACKET: return "']'";
    case TOKEN_COMMA: return "','";
    case TOKEN_SEMICOLON: return "';'";
    case TOKEN_DOT: return "'.'";
    case TOKEN_ARROW: return "'->'";
    case TOKEN_LEFT_ARROW: return "'<-'";
    case TOKEN_GT: return "'>'";
    case TOKEN_LT: return "'<'";
    case TOKEN_EQ: return "'=='";
    case TOKEN_NE: return "'!='";
    case TOKEN_GE: return "'>='";
    case TOKEN_LE: return "'<='";
    case TOKEN_NOT: return "'!'";
    case TOKEN_MUL: return "'*'";
    case TOKEN_DIV: return "'/'";
    case TOKEN_MOD: return "'%'";
    default: return "token";
    }
}

// The lexer decodes escape sequences ("\\" -> '\', "\n" -> newline, \uXXXX ->
// UTF-8). parseSingleStatement rebuilds source lines from token values, so the
// decoded content must be escaped again; otherwise a lone '\' or '"' corrupts
// the reconstructed line ("\\" would become "\" -> unterminated string).
static std::string reEscapeStringLiteral(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\x1b': out += "\\e"; break;
        default: out += c; break;
        }
    }
    return out;
}

void Parser::skipWhitespace(Lexer& lexer, Token& currentToken) {
    while (currentToken.type != TOKEN_EOF && !currentToken.value.empty()
        && currentToken.type != TOKEN_STRING
        && isspace(static_cast<unsigned char>(currentToken.value[0]))
        && currentToken.value[0] != '\n') {
        currentToken = lexer.nextToken();
    }
}

// One-token pushback for parseClassDef's method preview: the probe already
// consumed '(' from the stream, so it is stashed here and pulled back by the
// next eat() on that lexer. Parser-level, single-threaded, always consumed
// before the next member is parsed.
static bool g_classPushbackActive = false;
static Token g_classPushbackToken = Token(TOKEN_EOF, "");

static Token classPullToken(Lexer& lexer) {
    if (g_classPushbackActive) {
        g_classPushbackActive = false;
        return g_classPushbackToken;
    }
    return lexer.nextToken();
}

void Parser::eat(Lexer& lexer, Token& currentToken, TokenT expectedType) {
    if (currentToken.type == expectedType) {
        currentToken = classPullToken(lexer);
        skipWhitespace(lexer, currentToken);
    }
    else {
        throw std::runtime_error(makeParseError(currentToken,
            "Expected token type " + std::to_string(expectedType) + ", got " + std::to_string(currentToken.type) + " (value: " + currentToken.value + ")"));
    }
}

std::unique_ptr<Expr> Parser::parsePostfix(Lexer& lexer, Token& currentToken, std::unique_ptr<Expr> expr) {
    while (currentToken.type == TOKEN_LBRACKET) {
        eat(lexer, currentToken, TOKEN_LBRACKET);
        auto indexExpr = parseExpr(lexer, currentToken);
        eat(lexer, currentToken, TOKEN_RBRACKET);
        expr = std::unique_ptr<IndexExpr>(new IndexExpr(std::move(expr), std::move(indexExpr)));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parsePrimary(Lexer& lexer, Token& currentToken) {
    skipWhitespace(lexer, currentToken);
    Token token = currentToken;

    if (token.type == TOKEN_INT_CAST) {
        eat(lexer, currentToken, TOKEN_INT_CAST);
        return parseCastExpr(lexer, currentToken, CastType::Int);
    }
    else if (token.type == TOKEN_DOUBLE_CAST) {
        eat(lexer, currentToken, TOKEN_DOUBLE_CAST);
        return parseCastExpr(lexer, currentToken, CastType::Double);
    }
    else if (token.type == TOKEN_NEW) {
        eat(lexer, currentToken, TOKEN_NEW);
        // Class instantiation: 'new Point(a, b)' (fields or init arguments).
        if (currentToken.type == TOKEN_IDENTIFIER &&
            g_classRegistry.find(currentToken.value) != g_classRegistry.end()) {
            std::string className = currentToken.value;
            eat(lexer, currentToken, TOKEN_IDENTIFIER);
            eat(lexer, currentToken, TOKEN_LPAREN);
            std::vector<std::unique_ptr<Expr>> args;
            while (currentToken.type != TOKEN_RPAREN && !currentToken.value.empty()) {
                args.push_back(parseExpr(lexer, currentToken));
                if (currentToken.type == TOKEN_COMMA) {
                    eat(lexer, currentToken, TOKEN_COMMA);
                    skipWhitespace(lexer, currentToken);
                }
            }
            eat(lexer, currentToken, TOKEN_RPAREN);
            return std::unique_ptr<ObjectNewExpr>(new ObjectNewExpr(className, std::move(args)));
        }
        // Byte buffer allocation: 'new(size)'
        eat(lexer, currentToken, TOKEN_LPAREN);
        auto sizeExpr = parseExpr(lexer, currentToken);
        eat(lexer, currentToken, TOKEN_RPAREN);
        return std::unique_ptr<NewExpr>(new NewExpr(std::move(sizeExpr)));
    }

    if (token.type == TOKEN_NOT) {
        eat(lexer, currentToken, TOKEN_NOT);
        auto operand = parsePrimary(lexer, currentToken);
        return std::unique_ptr<UnaryExpr>(new UnaryExpr(TOKEN_NOT, std::move(operand)));
    }

    if (token.type == TOKEN_MINUS) {
        eat(lexer, currentToken, TOKEN_MINUS);
        auto operand = parsePrimary(lexer, currentToken);
        return std::unique_ptr<UnaryExpr>(new UnaryExpr(TOKEN_MINUS, std::move(operand)));
    }

    if (token.type == TOKEN_IDENTIFIER) {
        eat(lexer, currentToken, TOKEN_IDENTIFIER);
        // NOTE (P3-7): library-qualified names (lib.func) arrive pre-merged by
        // the lexer (readIdentifier includes '.'), so TOKEN_DOT never appears
        // here. Supporting real member access requires a lexer change first.
        std::string fullName = token.value;

        std::unique_ptr<Expr> baseExpr;
        if (currentToken.type == TOKEN_LPAREN) {
            eat(lexer, currentToken, TOKEN_LPAREN);
            std::vector<std::unique_ptr<Expr>> args;
            while (currentToken.type != TOKEN_RPAREN && currentToken.type != TOKEN_EOF) {
                args.push_back(parseExpr(lexer, currentToken));
                if (currentToken.type == TOKEN_COMMA) {
                    eat(lexer, currentToken, TOKEN_COMMA);
                    skipWhitespace(lexer, currentToken);
                }
            }
            eat(lexer, currentToken, TOKEN_RPAREN);
            baseExpr = std::unique_ptr<CallExpr>(new CallExpr(fullName, std::move(args)));
        }
        else {
            baseExpr = std::unique_ptr<IdentifierExpr>(new IdentifierExpr(fullName));
        }
        return parsePostfix(lexer, currentToken, std::move(baseExpr));
    }

    else if (token.type == TOKEN_NUMBER) {
        eat(lexer, currentToken, TOKEN_NUMBER);
        return std::unique_ptr<NumberExpr>(new NumberExpr(std::stoi(token.value)));
    }
    else if (token.type == TOKEN_DOUBLE_NUM) {
        eat(lexer, currentToken, TOKEN_DOUBLE_NUM);
        return std::unique_ptr<DoubleExpr>(new DoubleExpr(std::stod(token.value)));
    }
    else if (token.type == TOKEN_STRING) {
        eat(lexer, currentToken, TOKEN_STRING);
        return std::unique_ptr<StringExpr>(new StringExpr(token.value));
    }
    else if (token.type == TOKEN_LBRACKET) {
        eat(lexer, currentToken, TOKEN_LBRACKET);
        std::vector<std::unique_ptr<Expr>> elements;
        while (currentToken.type != TOKEN_RBRACKET && currentToken.type != TOKEN_EOF) {
            elements.push_back(parseExpr(lexer, currentToken));
            if (currentToken.type == TOKEN_COMMA) {
                eat(lexer, currentToken, TOKEN_COMMA);
                skipWhitespace(lexer, currentToken);
            }
        }
        eat(lexer, currentToken, TOKEN_RBRACKET);
        auto arrayExpr = std::unique_ptr<ArrayExpr>(new ArrayExpr(std::move(elements)));
        return parsePostfix(lexer, currentToken, std::move(arrayExpr));
    }
    else if (token.type == TOKEN_LBRACE) {
        eat(lexer, currentToken, TOKEN_LBRACE);
        std::vector<std::pair<std::string, std::unique_ptr<Expr>>> entries;
        while (currentToken.type != TOKEN_RBRACE && currentToken.type != TOKEN_EOF) {
            std::string key;
            if (currentToken.type == TOKEN_STRING) {
                key = currentToken.value;
                eat(lexer, currentToken, TOKEN_STRING);
            } else if (currentToken.type == TOKEN_IDENTIFIER) {
                key = currentToken.value;
                eat(lexer, currentToken, TOKEN_IDENTIFIER);
            } else {
                throw std::runtime_error(makeParseError(currentToken,
                    "Dict key must be a string or identifier"));
            }
            eat(lexer, currentToken, TOKEN_COLON);
            auto value = parseExpr(lexer, currentToken);
            entries.emplace_back(key, std::move(value));
            if (currentToken.type == TOKEN_COMMA) {
                eat(lexer, currentToken, TOKEN_COMMA);
                skipWhitespace(lexer, currentToken);
            }
        }
        eat(lexer, currentToken, TOKEN_RBRACE);
        auto dictExpr = std::unique_ptr<DictExpr>(new DictExpr(std::move(entries)));
        return parsePostfix(lexer, currentToken, std::move(dictExpr));
    }
    else if (token.type == TOKEN_LPAREN) {
        eat(lexer, currentToken, TOKEN_LPAREN);
        auto expr = parseExpr(lexer, currentToken);
        eat(lexer, currentToken, TOKEN_RPAREN);
        return parsePostfix(lexer, currentToken, std::move(expr));
    }
    else {
        throw std::runtime_error(makeParseError(token, "Invalid expression start: " + token.value));
    }
}

std::unique_ptr<Expr> Parser::parseTerm(Lexer& lexer, Token& currentToken) {
    auto left = parsePrimary(lexer, currentToken);
    while (currentToken.type == TOKEN_MUL || currentToken.type == TOKEN_DIV ||
           currentToken.type == TOKEN_MOD) {
        TokenT op = currentToken.type;
        eat(lexer, currentToken, op);
        auto right = parsePrimary(lexer, currentToken);
        left = std::unique_ptr<BinaryExpr>(new BinaryExpr(std::move(left), op, std::move(right)));
    }
    return left;
}

std::unique_ptr<Expr> Parser::parseAdd(Lexer& lexer, Token& currentToken) {
    auto left = parseTerm(lexer, currentToken);
    while (currentToken.type == TOKEN_PLUS || currentToken.type == TOKEN_MINUS) {
        TokenT op = currentToken.type;
        eat(lexer, currentToken, op);
        auto right = parseTerm(lexer, currentToken);
        left = std::unique_ptr<BinaryExpr>(new BinaryExpr(std::move(left), op, std::move(right)));
    }
    return left;
}

std::unique_ptr<Expr> Parser::parseExpr(Lexer& lexer, Token& currentToken) {
    auto left = parseCompare(lexer, currentToken);
    while (currentToken.type == TOKEN_AND || currentToken.type == TOKEN_OR) {
        Token opToken = currentToken;
        eat(lexer, currentToken, opToken.type);
        auto right = parseCompare(lexer, currentToken);
        left = std::unique_ptr<ConditionExpr>(new ConditionExpr(std::move(left), opToken.type, std::move(right)));
    }
    return left;
}

void Parser::parseAssignment(Lexer& lexer, Token& currentToken,
    std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    skipWhitespace(lexer, currentToken);
    std::string varName = currentToken.value;
    eat(lexer, currentToken, TOKEN_IDENTIFIER);
    eat(lexer, currentToken, TOKEN_EQUAL);
    auto expr = parseExpr(lexer, currentToken);
    variables[varName] = expr->evaluate(variables, functions);
}

void Parser::parsePrint(Lexer& lexer, Token& currentToken,
    std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    skipWhitespace(lexer, currentToken);
    eat(lexer, currentToken, TOKEN_PRINT);
    eat(lexer, currentToken, TOKEN_LPAREN); 
    auto expr = parseExpr(lexer, currentToken);
    eat(lexer, currentToken, TOKEN_RPAREN);

    Value result = expr->evaluate(variables, functions);
    switch (result.getType()) {
    case Value::Type::Int: std::cout << result.asInt(); break;
    case Value::Type::Double: std::cout << result.asDouble(); break;
    case Value::Type::String: std::cout << result.asString(); break;
    case Value::Type::Void: std::cout << "(void)"; break;
    }
    // std::cout << std::endl;
}

void Parser::parseEndl(Lexer& lexer, Token& currentToken,
    std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    skipWhitespace(lexer, currentToken);
	eat(lexer, currentToken, TOKEN_ENDL);
    std::cout << std::endl;
}

void Parser::parseExit(Lexer& lexer, Token& currentToken,
    std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    skipWhitespace(lexer, currentToken);
    eat(lexer, currentToken, TOKEN_EXIT);
	eat(lexer, currentToken, TOKEN_LPAREN);
	auto expr = parseExpr(lexer, currentToken);
	eat(lexer, currentToken, TOKEN_RPAREN);
	Value exitCodeVal = expr->evaluate(variables, functions);
    if (exitCodeVal.asInt() == 0)
        exit(0);
    exit(exitCodeVal.asInt()); 
}

Value Parser::parseRet(Lexer& lexer, Token& currentToken,
    std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    skipWhitespace(lexer, currentToken);
    eat(lexer, currentToken, TOKEN_RET);
    auto expr = parseExpr(lexer, currentToken);
    Value retVal = expr->evaluate(variables, functions);
    return retVal;
}
    
std::string Parser::parseSingleStatement(Lexer& lexer, Token& currentToken) {
    std::string stmt;
    skipWhitespace(lexer, currentToken);

    if (currentToken.type == TOKEN_IF || currentToken.type == TOKEN_WHILE ||
        currentToken.type == TOKEN_FOR || currentToken.type == TOKEN_TRY) {
        int braceDepth = 0;
        bool insideBlock = false;
        int parenDepth = 0;
        while (currentToken.type != TOKEN_EOF) {
            if (currentToken.type == TOKEN_LBRACE && parenDepth == 0) {
                insideBlock = true;
                braceDepth++;
            }
            else if (currentToken.type == TOKEN_RBRACE && insideBlock && parenDepth == 0) {
                braceDepth--;
                if (braceDepth == 0) {
                    stmt += "} ";
                    currentToken = lexer.nextToken();
                    skipWhitespace(lexer, currentToken);
                    while (currentToken.type == TOKEN_NEWLINE) {
                        currentToken = lexer.nextToken();
                        skipWhitespace(lexer, currentToken);
                    }
                    if (currentToken.type == TOKEN_ELSE || currentToken.type == TOKEN_CATCH) {
                        insideBlock = false;
                        continue;
                    }
                    break;
                }
            }
            else if (currentToken.type == TOKEN_LPAREN) {
                parenDepth++;
            }
            else if (currentToken.type == TOKEN_RPAREN) {
                if (parenDepth > 0) parenDepth--;
            }
            else if (!insideBlock && parenDepth == 0 && (currentToken.type == TOKEN_NEWLINE || currentToken.type == TOKEN_SEMICOLON)) {
                if (currentToken.type == TOKEN_SEMICOLON) {
                    stmt += "; ";
                }
                currentToken = lexer.nextToken();
                break;
            }
            if (currentToken.type == TOKEN_STRING) {
                stmt += "\"" + reEscapeStringLiteral(currentToken.value) + "\"";
            }
            else {
                stmt += currentToken.value;
            }
            stmt += " ";
            currentToken = lexer.nextToken();
            skipWhitespace(lexer, currentToken);
        }
    }
    else {
        int exprBraceDepth = 0;
        while (currentToken.type != TOKEN_EOF && currentToken.type != TOKEN_NEWLINE &&
               !(currentToken.type == TOKEN_RBRACE && exprBraceDepth == 0)) {
            if (currentToken.type == TOKEN_LBRACE) {
                exprBraceDepth++;
            } else if (currentToken.type == TOKEN_RBRACE && exprBraceDepth > 0) {
                exprBraceDepth--;
            }
            if (currentToken.type == TOKEN_STRING) {
                stmt += "\"" + reEscapeStringLiteral(currentToken.value) + "\"";
            }
            else {
                stmt += currentToken.value;
            }
            stmt += " ";
            currentToken = lexer.nextToken();
            skipWhitespace(lexer, currentToken);
        }

        if (currentToken.type == TOKEN_NEWLINE) {
            currentToken = lexer.nextToken();
        }
    }

    size_t start = stmt.find_first_not_of(" \t\n\r");
    size_t end = stmt.find_last_not_of(" \t\n\r");
    if (start != std::string::npos && end != std::string::npos) {
        stmt = stmt.substr(start, end - start + 1);
    }
    else {
        stmt.clear();
    }
    return stmt;
}

// ============================================================
// Object / class support
// ============================================================

Value defaultFieldValue(const std::string& type) {
    if (type == "int") return Value(0);
    if (type == "double") return Value(0.0);
    if (type == "string") return Value(std::string(""));
    if (type == "dict") return Value(std::unordered_map<std::string, GcHandle>());
    if (type == "array") return Value(std::vector<Value>());
    if (type == "bytes") return Value(std::vector<uint8_t>());
    return Value();
}

bool readObjectMember(const std::unordered_map<std::string, Value>& variables,
    const std::string& dottedName, Value& out) {
    size_t dot = dottedName.rfind('.');
    if (dot == std::string::npos || dot == 0) return false;
    std::string objName = dottedName.substr(0, dot);
    std::string memberName = dottedName.substr(dot + 1);
    auto it = variables.find(objName);
    if (it == variables.end() || it->second.getType() != Value::Type::Object) return false;
    const auto& members = it->second.asObjectDict();
    auto mIt = members.find(memberName);
    if (mIt == members.end()) {
        throw std::runtime_error("Undefined field '" + memberName + "' in class '" +
            it->second.asObjectClass() + "'");
    }
    const Value* memberVal = Gc::instance().deref(mIt->second);
    if (!memberVal) {
        throw std::runtime_error("Dangling field '" + memberName + "' in class '" +
            it->second.asObjectClass() + "'");
    }
    out = *memberVal;
    return true;
}

bool assignObjectMember(std::unordered_map<std::string, Value>& variables,
    const std::string& dottedName, const Value& value) {
    size_t dot = dottedName.rfind('.');
    if (dot == std::string::npos || dot == 0) return false;
    std::string objName = dottedName.substr(0, dot);
    std::string memberName = dottedName.substr(dot + 1);
    auto it = variables.find(objName);
    if (it == variables.end() || it->second.getType() != Value::Type::Object) return false;
    it->second.asObjectDictRef()[memberName] = Gc::instance().alloc(value);
    return true;
}

bool callObjectMethod(std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions,
    const std::string& dottedName, const std::vector<Value>& argVals, Value& out) {
    size_t dot = dottedName.rfind('.');
    if (dot == std::string::npos || dot == 0) return false;
    std::string objName = dottedName.substr(0, dot);
    std::string methodName = dottedName.substr(dot + 1);
    auto it = variables.find(objName);
    if (it == variables.end() || it->second.getType() != Value::Type::Object) return false;

    std::string fullName = it->second.asObjectClass() + "." + methodName;
    auto fit = functions.find(fullName);
    if (fit == functions.end()) return false;

    const Function& func = fit->second;
    if (argVals.size() != func.parameters.size() - 1) {
        throw std::runtime_error("Method " + fullName + " expects " +
            std::to_string(func.parameters.size() - 1) + " arguments, got " +
            std::to_string(argVals.size()));
    }
    Interpreter funcInterp;
    funcInterp.variables = variables;
    funcInterp.functions = functions;
    funcInterp.variables[func.parameters[0].name] = it->second;
    for (size_t i = 0; i < argVals.size(); ++i) {
        funcInterp.variables[func.parameters[i + 1].name] = argVals[i];
    }
    out = funcInterp.executeFunction(func);
    return true;
}

void Parser::parseFunction() {
    eat(funcLexer, funcCurrentToken, TOKEN_FUNC);
    Function func = parseFunctionRest();
    tempFunctions.push_back(func);
}

Function Parser::parseFunctionRest() {
    std::string funcName = funcCurrentToken.value;
    eat(funcLexer, funcCurrentToken, TOKEN_IDENTIFIER);
    eat(funcLexer, funcCurrentToken, TOKEN_LPAREN);

    std::vector<Parameter> params;
    while (funcCurrentToken.type != TOKEN_RPAREN && funcCurrentToken.type != TOKEN_EOF) {
        if (funcCurrentToken.type == TOKEN_NEWLINE || funcCurrentToken.value.empty()) {
            funcCurrentToken = funcLexer.nextToken();
            skipWhitespace(funcLexer, funcCurrentToken);
            continue;
        }

        std::string paramName = funcCurrentToken.value;
        eat(funcLexer, funcCurrentToken, TOKEN_IDENTIFIER);

        if (funcCurrentToken.type != TOKEN_LEFT_ARROW) {
            throw std::runtime_error(makeParseError(funcCurrentToken, "Parameter definition expected '<-', got: " + funcCurrentToken.value));
        }
        eat(funcLexer, funcCurrentToken, TOKEN_LEFT_ARROW);

        std::string paramType;
        if (funcCurrentToken.type == TOKEN_INT) {
            paramType = "int";
            eat(funcLexer, funcCurrentToken, TOKEN_INT);
        }
        else if (funcCurrentToken.type == TOKEN_DOUBLE) {
            paramType = "double";
            eat(funcLexer, funcCurrentToken, TOKEN_DOUBLE);
        }
        else if (funcCurrentToken.type == TOKEN_STRING_TYPE) {
            paramType = "string";
            eat(funcLexer, funcCurrentToken, TOKEN_STRING_TYPE);
        }
        else if (funcCurrentToken.type == TOKEN_DICT) {
            paramType = "dict";
            eat(funcLexer, funcCurrentToken, TOKEN_DICT);
        }
        else {
            throw std::runtime_error(makeParseError(funcCurrentToken, "Unsupported parameter type: " + funcCurrentToken.value));
        }

        params.push_back({ paramName, paramType });

        if (funcCurrentToken.type == TOKEN_COMMA) {
            eat(funcLexer, funcCurrentToken, TOKEN_COMMA);
            skipWhitespace(funcLexer, funcCurrentToken);
        }
    }

    eat(funcLexer, funcCurrentToken, TOKEN_RPAREN);
    std::string returnType = "void";
    if (funcCurrentToken.type == TOKEN_ARROW) {
        eat(funcLexer, funcCurrentToken, TOKEN_ARROW);
        if (funcCurrentToken.type == TOKEN_VOID) {
            returnType = "void";
            eat(funcLexer, funcCurrentToken, TOKEN_VOID);
        }
        else if (funcCurrentToken.type == TOKEN_INT) {
            returnType = "int";
            eat(funcLexer, funcCurrentToken, TOKEN_INT);
        }
        else if (funcCurrentToken.type == TOKEN_STRING_TYPE) {
            returnType = "string";
            eat(funcLexer, funcCurrentToken, TOKEN_STRING_TYPE);
        }
        else if (funcCurrentToken.type == TOKEN_DOUBLE) {
            returnType = "double";
            eat(funcLexer, funcCurrentToken, TOKEN_DOUBLE);
        }
        else if (funcCurrentToken.type == TOKEN_DICT) {
            returnType = "dict";
            eat(funcLexer, funcCurrentToken, TOKEN_DICT);
        }
        else {
            throw std::runtime_error(makeParseError(funcCurrentToken, "Unsupported return type: " + funcCurrentToken.value));
        }
    }

    if (funcCurrentToken.type == TOKEN_COLON) {
        eat(funcLexer, funcCurrentToken, TOKEN_COLON);
    }
    eat(funcLexer, funcCurrentToken, TOKEN_LBRACE);

    Function func;
    func.name = funcName;
    func.returnType = returnType;
	func.parameters = params;

    // NOTE: eat(LBRACE) already advanced funcCurrentToken to the first body
    // token. Calling nextToken() again here would discard it (single-line
    // bodies like "{ ret 5 }" would lose the first statement).
    skipWhitespace(funcLexer, funcCurrentToken);

    while (funcCurrentToken.type != TOKEN_RBRACE && funcCurrentToken.type != TOKEN_EOF) {
        int stmtLine = funcCurrentToken.line;
        std::string line = parseSingleStatement(funcLexer, funcCurrentToken);
        if (!line.empty()) {
            func.body.push_back(line);
            func.bodyLines.push_back(stmtLine);
            // Type declarations (int x = 5) return nullptr from parseLineToStmt;
            // real syntax errors propagate so they are reported (P2-2).
            func.compiledBody.push_back(std::move(parseLineToStmt(line)));
        }
        skipWhitespace(funcLexer, funcCurrentToken);
    }

    eat(funcLexer, funcCurrentToken, TOKEN_RBRACE);
    return func;
}

void Parser::parseClassDef() {
    bool isStruct = (funcCurrentToken.type == TOKEN_STRUCT);
    eat(funcLexer, funcCurrentToken, isStruct ? TOKEN_STRUCT : TOKEN_CLASS);
    std::string className = funcCurrentToken.value;
    eat(funcLexer, funcCurrentToken, TOKEN_IDENTIFIER);
    eat(funcLexer, funcCurrentToken, TOKEN_LBRACE);
    skipWhitespace(funcLexer, funcCurrentToken);

    ClassDef def;
    def.name = className;
    def.isStruct = isStruct;

    while (funcCurrentToken.type != TOKEN_RBRACE && funcCurrentToken.type != TOKEN_EOF) {
        if (funcCurrentToken.type == TOKEN_NEWLINE || funcCurrentToken.value.empty()) {
            funcCurrentToken = funcLexer.nextToken();
            skipWhitespace(funcLexer, funcCurrentToken);
            continue;
        }

        if (funcCurrentToken.type == TOKEN_FUNC) {
            eat(funcLexer, funcCurrentToken, TOKEN_FUNC);
        }

        // Field declaration: name <- type
        if (funcCurrentToken.type == TOKEN_IDENTIFIER) {
            std::string name = funcCurrentToken.value;
            // The probe below already consumes one token from funcLexer, so
            // the consumer branch must not re-pull the stream; a pushback
            // slot is used where the probe must be handed back (methods).
            Token probe = funcLexer.nextToken();
            skipWhitespace(funcLexer, probe);
            if (probe.type == TOKEN_LEFT_ARROW) {
                funcCurrentToken = funcLexer.nextToken();
                skipWhitespace(funcLexer, funcCurrentToken);
                std::string type;
                if (funcCurrentToken.type == TOKEN_INT) type = "int";
                else if (funcCurrentToken.type == TOKEN_DOUBLE) type = "double";
                else if (funcCurrentToken.type == TOKEN_STRING_TYPE) type = "string";
                else if (funcCurrentToken.type == TOKEN_DICT) type = "dict";
                else if (funcCurrentToken.type == TOKEN_NEW) type = "bytes";
                else {
                    throw std::runtime_error(makeParseError(funcCurrentToken,
                        "Unsupported field type: " + funcCurrentToken.value));
                }
                funcCurrentToken = funcLexer.nextToken();
                skipWhitespace(funcLexer, funcCurrentToken);
                def.fields.push_back({ name, type });
                continue;
            }
            // name(...) -> method, or 'init(...)' constructor
            if (probe.type == TOKEN_LPAREN) {
                g_classPushbackActive = true;
                g_classPushbackToken = probe;
                bool isInit = (name == "init");
                Function method = parseFunctionRest();
                if (isInit) {
                    def.hasInit = true;
                    def.initFunc = method;
                } else {
                    method.name = className + "." + method.name;
                    def.methods.push_back(method);
                }
                continue;
            }
            throw std::runtime_error(makeParseError(funcCurrentToken,
                "Expected '<-' or '(' after identifier in class body, got: " + probe.value));
        }

        throw std::runtime_error(makeParseError(funcCurrentToken,
            "Unexpected token in class body: " + funcCurrentToken.value));
    }

    eat(funcLexer, funcCurrentToken, TOKEN_RBRACE);
    g_classRegistry[className] = def;
}

Parser::Parser(const std::string& src, std::unordered_map<std::string, Value>& vars,
    std::unordered_map<std::string, Function>& funcs)
    : funcLexer(src), variables(vars), functions(funcs), funcCurrentToken(funcLexer.nextToken()) {
    skipWhitespace(funcLexer, funcCurrentToken);
}

void Parser::parseAllFunctions() {
    tempFunctions.clear();
    while (funcCurrentToken.type != TOKEN_EOF) {
        skipWhitespace(funcLexer, funcCurrentToken);
        if (funcCurrentToken.type == TOKEN_FUNC) {
            parseFunction();
        }
        else if (funcCurrentToken.type == TOKEN_CLASS || funcCurrentToken.type == TOKEN_STRUCT) {
            parseClassDef();
        }
        else if (funcCurrentToken.type == TOKEN_IMPORT || funcCurrentToken.type == TOKEN_PLUGIN_IMPORT) {
            parseImportStatement(funcLexer, funcCurrentToken, variables, functions);
        }
        else {
            funcCurrentToken = funcLexer.nextToken();
        }
    }
    for (const auto& func : tempFunctions) {
        functions[func.name] = func;
    }
}

// ============================================================
// Handler-based line parsing (shared between interpreter & bytecode)
// ============================================================
void Parser::parseLine(const std::string& line, StmtHandler& handler) {
    Lexer lineLexer(line);
    Token currentToken = lineLexer.nextToken();
    skipWhitespace(lineLexer, currentToken);

    if (currentToken.type == TOKEN_EOF) return;
    if (currentToken.type == TOKEN_SEMICOLON) {
        throw std::runtime_error(makeParseError(currentToken,
            "FoxVast does not use semicolons. Remove the ';' and start a new line instead."));
    }

    if (currentToken.type == TOKEN_IMPORT) {
        return;
    }
    if (currentToken.type == TOKEN_IF) {
        IfStatement ifStmt = parseIfStatement(lineLexer, currentToken);
        handler.onIf(std::move(ifStmt));
        return;
    }
    if (currentToken.type == TOKEN_FOR) {
        ForStatement forStmt = parseForStatement(lineLexer, currentToken);
        handler.onFor(std::move(forStmt));
        return;
    }
    if (currentToken.type == TOKEN_WHILE) {
        WhileStatement whileStmt = parseWhileStatement(lineLexer, currentToken);
        handler.onWhile(std::move(whileStmt));
        return;
    }
    if (currentToken.type == TOKEN_TRY) {
        TryStatement tryStmt = parseTryStatement(lineLexer, currentToken);
        handler.onTry(std::move(tryStmt));
        return;
    }
    if (currentToken.type == TOKEN_BREAK) {
        eat(lineLexer, currentToken, TOKEN_BREAK);
        handler.onBreak();
        return;
    }
    if (currentToken.type == TOKEN_CONTINUE) {
        eat(lineLexer, currentToken, TOKEN_CONTINUE);
        handler.onContinue();
        return;
    }
    if (currentToken.type == TOKEN_ERROR) {
        eat(lineLexer, currentToken, TOKEN_ERROR);
        eat(lineLexer, currentToken, TOKEN_LPAREN);
        auto message = parseExpr(lineLexer, currentToken);
        eat(lineLexer, currentToken, TOKEN_RPAREN);
        handler.onError(std::move(message));
        return;
    }
    if (currentToken.type == TOKEN_YIELD) {
        eat(lineLexer, currentToken, TOKEN_YIELD);
        std::unique_ptr<Expr> value;
        if (currentToken.type == TOKEN_LPAREN) {
            eat(lineLexer, currentToken, TOKEN_LPAREN);
            value = parseExpr(lineLexer, currentToken);
            eat(lineLexer, currentToken, TOKEN_RPAREN);
        }
        handler.onYield(std::move(value));
        return;
    }
    if (currentToken.type == TOKEN_FN) {
        eat(lineLexer, currentToken, TOKEN_FN);
        std::string labelName = currentToken.value;
        eat(lineLexer, currentToken, TOKEN_IDENTIFIER);
        eat(lineLexer, currentToken, TOKEN_COLON);
        handler.onFnLabel(labelName);
        return;
    }
    if (currentToken.type == TOKEN_GOTO) {
        eat(lineLexer, currentToken, TOKEN_GOTO);
        std::string labelName = currentToken.value;
        eat(lineLexer, currentToken, TOKEN_IDENTIFIER);
        handler.onGoto(labelName);
        return;
    }
    if (currentToken.type == TOKEN_INPUT) {
        eat(lineLexer, currentToken, TOKEN_INPUT);
        eat(lineLexer, currentToken, TOKEN_LPAREN);
        eat(lineLexer, currentToken, TOKEN_RPAREN);
        eat(lineLexer, currentToken, TOKEN_ARROW);
        std::string varName = currentToken.value;
        eat(lineLexer, currentToken, TOKEN_IDENTIFIER);
        handler.onInput(varName);
        return;
    }
    if (currentToken.type == TOKEN_PRINT) {
        eat(lineLexer, currentToken, TOKEN_PRINT);
        eat(lineLexer, currentToken, TOKEN_LPAREN);
        auto expr = parseExpr(lineLexer, currentToken);
        eat(lineLexer, currentToken, TOKEN_RPAREN);
        handler.onPrint(std::move(expr));
        return;
    }
    if (currentToken.type == TOKEN_PRINTLN) {
        eat(lineLexer, currentToken, TOKEN_PRINTLN);
        eat(lineLexer, currentToken, TOKEN_LPAREN);
        auto expr = parseExpr(lineLexer, currentToken);
        eat(lineLexer, currentToken, TOKEN_RPAREN);
        handler.onPrintln(std::move(expr));
        return;
    }
    if (currentToken.type == TOKEN_ENDL) {
        eat(lineLexer, currentToken, TOKEN_ENDL);
        handler.onEndl();
        return;
    }
    if (currentToken.type == TOKEN_EXIT) {
        eat(lineLexer, currentToken, TOKEN_EXIT);
        eat(lineLexer, currentToken, TOKEN_LPAREN);
        auto expr = parseExpr(lineLexer, currentToken);
        eat(lineLexer, currentToken, TOKEN_RPAREN);
        handler.onExit(std::move(expr));
        return;
    }
    if (currentToken.type == TOKEN_FREE) {
        eat(lineLexer, currentToken, TOKEN_FREE);
        eat(lineLexer, currentToken, TOKEN_LPAREN);
        skipWhitespace(lineLexer, currentToken);
        std::string varName = currentToken.value;
        eat(lineLexer, currentToken, TOKEN_IDENTIFIER);
        eat(lineLexer, currentToken, TOKEN_RPAREN);
        handler.onFree(varName);
        return;
    }
    if (currentToken.type == TOKEN_FREE_ALL) {
        eat(lineLexer, currentToken, TOKEN_FREE_ALL);
        eat(lineLexer, currentToken, TOKEN_LPAREN);
        eat(lineLexer, currentToken, TOKEN_RPAREN);
        handler.onFreeAll();
        return;
    }
    if (currentToken.type == TOKEN_RET) {
        eat(lineLexer, currentToken, TOKEN_RET);
        if (currentToken.type != TOKEN_NEWLINE && currentToken.type != TOKEN_EOF && currentToken.type != TOKEN_RBRACE) {
            auto expr = parseExpr(lineLexer, currentToken);
            handler.onRet(std::move(expr));
        } else {
            handler.onRet(nullptr);
        }
        return;
    }
    if (currentToken.type == TOKEN_IDENTIFIER) {
        std::string identName = currentToken.value;
        Token nextToken = lineLexer.nextToken();
        skipWhitespace(lineLexer, nextToken);

        if (nextToken.type == TOKEN_LPAREN) {
            currentToken = nextToken;
            eat(lineLexer, currentToken, TOKEN_LPAREN);
            std::vector<std::unique_ptr<Expr>> args;
            while (currentToken.type != TOKEN_RPAREN && currentToken.type != TOKEN_EOF) {
                args.push_back(parseExpr(lineLexer, currentToken));
                if (currentToken.type == TOKEN_COMMA) {
                    eat(lineLexer, currentToken, TOKEN_COMMA);
                    skipWhitespace(lineLexer, currentToken);
                }
            }
            eat(lineLexer, currentToken, TOKEN_RPAREN);
            handler.onCall(identName, std::move(args));
        } else if (nextToken.type == TOKEN_PLUS_EQ || nextToken.type == TOKEN_MINUS_EQ ||
            nextToken.type == TOKEN_MUL_EQ || nextToken.type == TOKEN_DIV_EQ ||
            nextToken.type == TOKEN_MOD_EQ) {
            TokenT opToken = nextToken.type;
            currentToken = nextToken;
            eat(lineLexer, currentToken, opToken);
            auto right = parseExpr(lineLexer, currentToken);
            TokenT binaryOp = TOKEN_PLUS;
            switch (opToken) {
            case TOKEN_PLUS_EQ: binaryOp = TOKEN_PLUS; break;
            case TOKEN_MINUS_EQ: binaryOp = TOKEN_MINUS; break;
            case TOKEN_MUL_EQ: binaryOp = TOKEN_MUL; break;
            case TOKEN_DIV_EQ: binaryOp = TOKEN_DIV; break;
            case TOKEN_MOD_EQ: binaryOp = TOKEN_MOD; break;
            default: break;
            }
            auto left = std::make_unique<IdentifierExpr>(identName);
            auto combined = std::make_unique<BinaryExpr>(std::move(left), binaryOp, std::move(right));
            handler.onAssign(identName, std::move(combined));
        } else if (nextToken.type == TOKEN_EQUAL) {
            currentToken = nextToken;
            eat(lineLexer, currentToken, TOKEN_EQUAL);
            auto expr = parseExpr(lineLexer, currentToken);
            handler.onAssign(identName, std::move(expr));
        } else if (nextToken.type == TOKEN_LBRACKET) {
            currentToken = nextToken;
            eat(lineLexer, currentToken, TOKEN_LBRACKET);
            auto indexExpr = parseExpr(lineLexer, currentToken);
            eat(lineLexer, currentToken, TOKEN_RBRACKET);
            skipWhitespace(lineLexer, currentToken);
            eat(lineLexer, currentToken, TOKEN_EQUAL);
            auto expr = parseExpr(lineLexer, currentToken);
            handler.onIndexAssign(identName, std::move(indexExpr), std::move(expr));
        } else if (identName == "END" || identName == "end") {
            handler.onEndl();
        } else {
            std::string nextVal = nextToken.value.empty()
                ? "<" + std::string(tokenTypeName(nextToken.type)) + ">"
                : "'" + nextToken.value + "'";
            throw std::runtime_error(makeParseError(nextToken,
                "Expected '(' or '=' after identifier, got " + nextVal));
        }
        return;
    }
}

// ============================================================
// Executing handler for the interpreter path
// ============================================================
namespace {
    class ExecutingHandler : public StmtHandler {
    public:
        std::unordered_map<std::string, Value>& variables;
        std::unordered_map<std::string, Function>& functions;
        Value retValue;

        ExecutingHandler(std::unordered_map<std::string, Value>& vars,
                         std::unordered_map<std::string, Function>& funcs)
            : variables(vars), functions(funcs) {}

        void onPrint(std::unique_ptr<Expr> arg) override {
            // Use Value::toString() to match the VM's OP_PRINT formatting (P3-4)
            std::cout << arg->evaluate(variables, functions).toString();
        }
        void onPrintln(std::unique_ptr<Expr> arg) override {
            // Use Value::toString() to match the VM's OP_PRINTLN formatting (P3-4)
            std::cout << arg->evaluate(variables, functions).toString() << std::endl;
        }
        void onExit(std::unique_ptr<Expr> arg) override {
            Value val = arg->evaluate(variables, functions);
            if (val.getType() == Value::Type::Int) std::exit(val.asInt());
            std::exit(0);
        }
        void onFree(const std::string& varName) override {
            variables.erase(varName);
        }
        void onFreeAll() override {
            variables.clear();
        }
        Value onRet(std::unique_ptr<Expr> arg) override {
            if (arg) {
                retValue = arg->evaluate(variables, functions);
            } else {
                // Bare "ret": signal the enclosing executor to stop, like the
                // compiled-path RetStmt does with the Return marker.
                retValue = Value::makeReturnMarker();
            }
            return retValue;
        }
        void onEndl() override { std::cout << std::endl; }
        void onInput(const std::string& varName) override {
            std::string userInput;
            std::getline(std::cin, userInput);
            variables[varName] = Value(userInput);
        }
        void onCall(const std::string& name, std::vector<std::unique_ptr<Expr>> args) override {
            CallExpr callExpr(name, std::move(args));
            callExpr.evaluate(variables, functions);
        }
        void onAssign(const std::string& name, std::unique_ptr<Expr> expr) override {
            variables[name] = expr->evaluate(variables, functions);
            if (variables[name].getType() == Value::Type::Bytes) {
                int sz = static_cast<int>(variables[name].asBytes().size());
                Parser::checkNewAllocBytes(sz);
            }
        }
        void onIndexAssign(const std::string& name, std::unique_ptr<Expr> index, std::unique_ptr<Expr> value) override {
            Value idxVal = index->evaluate(variables, functions);
            Value val = value->evaluate(variables, functions);
            if (variables.find(name) == variables.end()) {
                throw std::runtime_error("Undefined array variable: " + name);
            }
            std::vector<Value>& arr = variables[name].asArrayRef();
            int idx = idxVal.asInt();
            if (idx < 0 || idx >= static_cast<int>(arr.size())) {
                throw std::runtime_error("Array index out of bounds: " + std::to_string(idx));
            }
            arr[idx] = val;
        }
        void onIf(IfStatement ifStmt) override {
            retValue = Parser::executeIfStatement(ifStmt, variables, functions);
        }
        void onWhile(WhileStatement whileStmt) override {
            retValue = Parser::executeWhileStatement(whileStmt, variables, functions);
        }
        void onFor(ForStatement forStmt) override {
            retValue = Parser::executeForStatement(forStmt, variables, functions);
        }
        void onTry(TryStatement tryStmt) override {
            TryStmt stmt(tryStmt.errorVar,
                Parser::compileBody(tryStmt.body),
                Parser::compileBody(tryStmt.catchBody));
            retValue = stmt.execute(variables, functions);
        }
        void onBreak() override {
            throw BreakException();
        }
        void onContinue() override {
            throw ContinueException();
        }
        void onError(std::unique_ptr<Expr> message) override {
            ErrorStmt stmt(std::move(message));
            stmt.execute(variables, functions);
        }
        void onYield(std::unique_ptr<Expr> value) override {
            throw std::runtime_error("yield is only supported by the bytecode VM");
        }
        void onFnLabel(const std::string& name) override {}
        void onGoto(const std::string& name) override {
            throw GotoException(name);
        }
    };
}

// ============================================================
// CompilingHandler ?? builds Stmt nodes without executing
// ============================================================
namespace {
    class CompilingHandler : public StmtHandler {
    public:
        std::unique_ptr<Stmt> stmt;

        void onPrint(std::unique_ptr<Expr> arg) override {
            stmt = std::make_unique<PrintStmt>(std::move(arg));
        }
        void onPrintln(std::unique_ptr<Expr> arg) override {
            stmt = std::make_unique<PrintlnStmt>(std::move(arg));
        }
        void onExit(std::unique_ptr<Expr> arg) override {
            stmt = std::make_unique<ExitStmt>(std::move(arg));
        }
        void onFree(const std::string& varName) override {
            stmt = std::make_unique<FreeStmt>(varName);
        }
        void onFreeAll() override {
            stmt = std::make_unique<FreeAllStmt>();
        }
        Value onRet(std::unique_ptr<Expr> arg) override {
            stmt = std::make_unique<RetStmt>(std::move(arg));
            return Value();
        }
        void onEndl() override {
            stmt = std::make_unique<EndlStmt>();
        }
        void onInput(const std::string& varName) override {
            stmt = std::make_unique<InputStmt>(varName);
        }
        void onCall(const std::string& name, std::vector<std::unique_ptr<Expr>> args) override {
            stmt = std::make_unique<CallStmt>(name, std::move(args));
        }
        void onAssign(const std::string& name, std::unique_ptr<Expr> expr) override {
            stmt = std::make_unique<AssignStmt>(name, std::move(expr));
        }
        void onIndexAssign(const std::string& name, std::unique_ptr<Expr> index, std::unique_ptr<Expr> value) override {
            stmt = std::make_unique<IndexAssignStmt>(name, std::move(index), std::move(value));
        }
        void onIf(IfStatement ifStmt) override {
            stmt = std::make_unique<IfStmt>(ifStmt.condition,
                Parser::compileBody(ifStmt.body),
                Parser::compileBody(ifStmt.elseBody));
        }
        void onWhile(WhileStatement whileStmt) override {
            stmt = std::make_unique<WhileStmt>(whileStmt.condition, Parser::compileBody(whileStmt.body));
        }
        void onFor(ForStatement forStmt) override {
            stmt = std::make_unique<ForStmt>(forStmt.init, forStmt.condition, forStmt.iter, Parser::compileBody(forStmt.body));
        }
        void onTry(TryStatement tryStmt) override {
            stmt = std::make_unique<TryStmt>(tryStmt.errorVar,
                Parser::compileBody(tryStmt.body),
                Parser::compileBody(tryStmt.catchBody));
        }
        void onBreak() override {
            stmt = std::make_unique<BreakStmt>();
        }
        void onContinue() override {
            stmt = std::make_unique<ContinueStmt>();
        }
        void onError(std::unique_ptr<Expr> message) override {
            stmt = std::make_unique<ErrorStmt>(std::move(message));
        }
        void onYield(std::unique_ptr<Expr> value) override {
            stmt = std::make_unique<YieldStmt>(std::move(value));
        }
        void onFnLabel(const std::string& name) override {
            stmt = std::make_unique<LabelStmt>(name);
        }
        void onGoto(const std::string& name) override {
            stmt = std::make_unique<GotoStmt>(name);
        }
    };
}

std::unique_ptr<Stmt> Parser::parseLineToStmt(const std::string& line) {
    if (line.empty()) return nullptr;
    // Skip type declarations (int/double/string var = expr) ?? not expressible as Stmts
    std::string trimmed = line;
    size_t start = trimmed.find_first_not_of(" \t");
    if (start != std::string::npos) trimmed = trimmed.substr(start);
    if (trimmed.rfind("int ", 0) == 0 || trimmed.rfind("double ", 0) == 0 ||
        trimmed.rfind("string ", 0) == 0 || trimmed.rfind("dict ", 0) == 0) {
        return nullptr;
    }
    CompilingHandler handler;
    parseLine(line, handler);
    return std::move(handler.stmt);
}

std::vector<std::unique_ptr<Stmt>> Parser::compileBody(const std::vector<std::string>& lines) {
    std::vector<std::unique_ptr<Stmt>> compiled;
    compiled.reserve(lines.size());
    for (const auto& line : lines) {
        auto stmt = parseLineToStmt(line);
        if (stmt) compiled.push_back(std::move(stmt));
    }
    return compiled;
}

Value Parser::parseLine(const std::string& line,
    std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    ExecutingHandler handler(variables, functions);
    parseLine(line, handler);
    return handler.retValue;
}

std::unique_ptr<Expr> Parser::parseCastExpr(Lexer& lexer, Token& currentToken, CastType castType) {
    eat(lexer, currentToken, TOKEN_LPAREN);
    auto expr = parseExpr(lexer, currentToken);
    eat(lexer, currentToken, TOKEN_RPAREN);
    return std::unique_ptr<CastExpr>(new CastExpr(castType, std::move(expr)));
}

void Parser::parseInputStatement(Lexer& lexer, Token& currentToken,
    std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    skipWhitespace(lexer, currentToken);
    eat(lexer, currentToken, TOKEN_INPUT);
    eat(lexer, currentToken, TOKEN_LPAREN);
    eat(lexer, currentToken, TOKEN_RPAREN);
    eat(lexer, currentToken, TOKEN_ARROW);
    std::string varName = currentToken.value;
    eat(lexer, currentToken, TOKEN_IDENTIFIER);

    InputExpr inputExpr;
    variables[varName] = inputExpr.evaluate(variables, functions);
}

std::unique_ptr<Expr> Parser::parseCompare(Lexer& lexer, Token& currentToken) {
    auto left = parseAdd(lexer, currentToken);
    while (currentToken.type == TOKEN_EQ || currentToken.type == TOKEN_NE ||
        currentToken.type == TOKEN_GT || currentToken.type == TOKEN_LT ||
        currentToken.type == TOKEN_GE || currentToken.type == TOKEN_LE) {
        Token opToken = currentToken;
        eat(lexer, currentToken, opToken.type);
        auto right = parseAdd(lexer, currentToken);

        CompareType cmpType;
        switch (opToken.type) {
        case TOKEN_EQ: cmpType = CompareType::EQ; break;
        case TOKEN_NE: cmpType = CompareType::NE; break;
        case TOKEN_GT: cmpType = CompareType::GT; break;
        case TOKEN_LT: cmpType = CompareType::LT; break;
        case TOKEN_GE: cmpType = CompareType::GE; break;
        case TOKEN_LE: cmpType = CompareType::LE; break;
        default: throw std::runtime_error("Unsupported comparison operator");
        }
        left = std::unique_ptr<CompareExpr>(new CompareExpr(std::move(left), cmpType, std::move(right)));
    }
    return left;
}

IfStatement Parser::parseIfStatement(Lexer& lexer, Token& currentToken) {
    IfStatement ifStmt;
    skipWhitespace(lexer, currentToken);
    eat(lexer, currentToken, TOKEN_IF);
    eat(lexer, currentToken, TOKEN_LPAREN);

    std::string condStr;
    int parenDepth = 0;
    while (currentToken.type != TOKEN_EOF) {
        if (currentToken.type == TOKEN_NEWLINE) {
            currentToken = lexer.nextToken();
            skipWhitespace(lexer, currentToken);
            continue;
        }
        if (currentToken.type == TOKEN_LPAREN) parenDepth++;
        if (currentToken.type == TOKEN_RPAREN) {
            if (parenDepth == 0) break;
            parenDepth--;
        }
        if (currentToken.type == TOKEN_STRING)
            condStr += "\"" + reEscapeStringLiteral(currentToken.value) + "\" ";
        else
            condStr += currentToken.value + " ";
        currentToken = lexer.nextToken();
        skipWhitespace(lexer, currentToken);
    }
    size_t start = condStr.find_first_not_of(" ");
    size_t end = condStr.find_last_not_of(" ");
    ifStmt.condition = (start != std::string::npos) ? condStr.substr(start, end - start + 1) : "";
    eat(lexer, currentToken, TOKEN_RPAREN);

    if (currentToken.type == TOKEN_LBRACE) {
        eat(lexer, currentToken, TOKEN_LBRACE);
        while (currentToken.type != TOKEN_RBRACE && currentToken.type != TOKEN_EOF) {
            if (currentToken.type == TOKEN_NEWLINE) {
                currentToken = lexer.nextToken();
                continue;
            }
            std::string stmt = parseSingleStatement(lexer, currentToken);
            if (!stmt.empty()) {
                ifStmt.body.push_back(stmt);
            }
            skipWhitespace(lexer, currentToken);
        }
        eat(lexer, currentToken, TOKEN_RBRACE);
    } else {
        std::string stmt = parseSingleStatement(lexer, currentToken);
        if (!stmt.empty()) {
            ifStmt.body.push_back(stmt);
        }
    }

    if (currentToken.type == TOKEN_ELSE) {
        eat(lexer, currentToken, TOKEN_ELSE);
        if (currentToken.type == TOKEN_LBRACE) {
            eat(lexer, currentToken, TOKEN_LBRACE);
            while (currentToken.type != TOKEN_RBRACE && currentToken.type != TOKEN_EOF) {
                if (currentToken.type == TOKEN_NEWLINE) {
                    currentToken = lexer.nextToken();
                    continue;
                }
                std::string stmt = parseSingleStatement(lexer, currentToken);
                if (!stmt.empty()) {
                    ifStmt.elseBody.push_back(stmt);
                }
                skipWhitespace(lexer, currentToken);
            }
            eat(lexer, currentToken, TOKEN_RBRACE);
        } else {
            std::string stmt = parseSingleStatement(lexer, currentToken);
            if (!stmt.empty()) {
                ifStmt.elseBody.push_back(stmt);
            }
        }
    }
    return ifStmt;
}

Value Parser::executeIfStatement(const IfStatement& ifStmt,
    std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    if (ifStmt.condition.empty()) return Value();
    Lexer condLexer(ifStmt.condition);
    Token condToken = condLexer.nextToken();
    skipWhitespace(condLexer, condToken);
    auto condExpr = parseExpr(condLexer, condToken);
    Value condResult = condExpr->evaluate(variables, functions);
    bool isTrue = condResult.asBool();
    if (isTrue) {
        for (const auto& stmt : ifStmt.body) {
            Value val = parseLine(stmt, variables, functions);
            if (val.getType() != Value::Type::Void) {
                return val;
            }
        }
    } else {
        for (const auto& stmt : ifStmt.elseBody) {
            Value val = parseLine(stmt, variables, functions);
            if (val.getType() != Value::Type::Void) {
                return val;
            }
        }
    }
    return Value();
}

WhileStatement Parser::parseWhileStatement(Lexer& lexer, Token& currentToken) {
    WhileStatement whileStmt;
    skipWhitespace(lexer, currentToken);
    eat(lexer, currentToken, TOKEN_WHILE);
    eat(lexer, currentToken, TOKEN_LPAREN);

    std::string condStr;
    int parenDepth = 0;
    while (currentToken.type != TOKEN_EOF) {
        if (currentToken.type == TOKEN_NEWLINE) {
            currentToken = lexer.nextToken();
            skipWhitespace(lexer, currentToken);
            continue;
        }
        if (currentToken.type == TOKEN_LPAREN) parenDepth++;
        if (currentToken.type == TOKEN_RPAREN) {
            if (parenDepth == 0) break;
            parenDepth--;
        }
        if (currentToken.type == TOKEN_STRING)
            condStr += "\"" + reEscapeStringLiteral(currentToken.value) + "\" ";
        else
            condStr += currentToken.value + " ";
        currentToken = lexer.nextToken();
        skipWhitespace(lexer, currentToken);
    }
    size_t start = condStr.find_first_not_of(" ");
    size_t end = condStr.find_last_not_of(" ");
    whileStmt.condition = (start != std::string::npos) ? condStr.substr(start, end - start + 1) : "";
    eat(lexer, currentToken, TOKEN_RPAREN);

    if (currentToken.type == TOKEN_LBRACE) {
        eat(lexer, currentToken, TOKEN_LBRACE);
        while (currentToken.type != TOKEN_RBRACE && currentToken.type != TOKEN_EOF) {
            if (currentToken.type == TOKEN_NEWLINE) {
                currentToken = lexer.nextToken();
                continue;
            }
            std::string stmt = parseSingleStatement(lexer, currentToken);
            if (!stmt.empty()) {
                whileStmt.body.push_back(stmt);
            }
            skipWhitespace(lexer, currentToken);
        }
        eat(lexer, currentToken, TOKEN_RBRACE);
    } else {
        std::string stmt = parseSingleStatement(lexer, currentToken);
        if (!stmt.empty()) {
            whileStmt.body.push_back(stmt);
        }
    }
    return whileStmt;
}

Value Parser::executeWhileStatement(const WhileStatement& whileStmt,
    std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    if (whileStmt.condition.empty()) return Value();
    Lexer condLexer(whileStmt.condition);
    Token condToken = condLexer.nextToken();
    skipWhitespace(condLexer, condToken);
    auto condExpr = parseExpr(condLexer, condToken);
    Value condResult = condExpr->evaluate(variables, functions);
    while (condResult.asBool()) {
        for (const auto& stmt : whileStmt.body) {
            Value val = parseLine(stmt, variables, functions);
            if (val.getType() != Value::Type::Void) {
                return val;
            }
        }
        condLexer = Lexer(whileStmt.condition);
        condToken = condLexer.nextToken();
        skipWhitespace(condLexer, condToken);
        condExpr = parseExpr(condLexer, condToken);
        condResult = condExpr->evaluate(variables, functions);
    }
    return Value();
}

ForStatement Parser::parseForStatement(Lexer& lexer, Token& currentToken) {
    ForStatement forStmt;
    skipWhitespace(lexer, currentToken);
    eat(lexer, currentToken, TOKEN_FOR);
    eat(lexer, currentToken, TOKEN_LPAREN);

    std::string init;
    while (currentToken.type != TOKEN_SEMICOLON && currentToken.type != TOKEN_EOF) {
        if (currentToken.type == TOKEN_NEWLINE) {
            currentToken = lexer.nextToken();
            skipWhitespace(lexer, currentToken);
            continue;
        }
        if (currentToken.type == TOKEN_STRING)
            init += "\"" + reEscapeStringLiteral(currentToken.value) + "\" ";
        else
            init += currentToken.value + " ";
        currentToken = lexer.nextToken();
        skipWhitespace(lexer, currentToken);
    }
    eat(lexer, currentToken, TOKEN_SEMICOLON);
    std::string condition;
    while (currentToken.type != TOKEN_SEMICOLON && currentToken.type != TOKEN_EOF) {
        if (currentToken.type == TOKEN_NEWLINE) {
            currentToken = lexer.nextToken();
            skipWhitespace(lexer, currentToken);
            continue;
        }
        if (currentToken.type == TOKEN_STRING)
            condition += "\"" + reEscapeStringLiteral(currentToken.value) + "\" ";
        else
            condition += currentToken.value + " ";
        currentToken = lexer.nextToken();
        skipWhitespace(lexer, currentToken);
    }
    eat(lexer, currentToken, TOKEN_SEMICOLON);
    std::string iter;
    int iterParenDepth = 0;
    while (currentToken.type != TOKEN_EOF) {
        if (currentToken.type == TOKEN_NEWLINE) {
            currentToken = lexer.nextToken();
            skipWhitespace(lexer, currentToken);
            continue;
        }
        if (currentToken.type == TOKEN_LPAREN) iterParenDepth++;
        if (currentToken.type == TOKEN_RPAREN) {
            if (iterParenDepth == 0) break;
            iterParenDepth--;
        }
        if (currentToken.type == TOKEN_STRING)
            iter += "\"" + reEscapeStringLiteral(currentToken.value) + "\" ";
        else
            iter += currentToken.value + " ";
        currentToken = lexer.nextToken();
        skipWhitespace(lexer, currentToken);
    }
    eat(lexer, currentToken, TOKEN_RPAREN);
    size_t start = init.find_first_not_of(" ");
    size_t end = init.find_last_not_of(" ");
    forStmt.init = (start != std::string::npos) ? init.substr(start, end - start + 1) : "";
    start = condition.find_first_not_of(" ");
    end = condition.find_last_not_of(" ");
    forStmt.condition = (start != std::string::npos) ? condition.substr(start, end - start + 1) : "";
    start = iter.find_first_not_of(" ");
    end = iter.find_last_not_of(" ");
    forStmt.iter = (start != std::string::npos) ? iter.substr(start, end - start + 1) : "";

    if (currentToken.type == TOKEN_LBRACE) {
        eat(lexer, currentToken, TOKEN_LBRACE);
        while (currentToken.type != TOKEN_RBRACE && currentToken.type != TOKEN_EOF) {
            if (currentToken.type == TOKEN_NEWLINE) {
                currentToken = lexer.nextToken();
                continue;
            }
            std::string stmt = parseSingleStatement(lexer, currentToken);
            if (!stmt.empty()) {
                forStmt.body.push_back(stmt);
            }
            skipWhitespace(lexer, currentToken);
        }
        eat(lexer, currentToken, TOKEN_RBRACE);
    } else {
        std::string stmt = parseSingleStatement(lexer, currentToken);
        if (!stmt.empty()) {
            forStmt.body.push_back(stmt);
        }
    }
    return forStmt;
}

TryStatement Parser::parseTryStatement(Lexer& lexer, Token& currentToken) {
    TryStatement tryStmt;
    skipWhitespace(lexer, currentToken);
    eat(lexer, currentToken, TOKEN_TRY);

    if (currentToken.type == TOKEN_LBRACE) {
        eat(lexer, currentToken, TOKEN_LBRACE);
        while (currentToken.type != TOKEN_RBRACE && currentToken.type != TOKEN_EOF) {
            if (currentToken.type == TOKEN_NEWLINE) {
                currentToken = lexer.nextToken();
                continue;
            }
            std::string stmt = parseSingleStatement(lexer, currentToken);
            if (!stmt.empty()) {
                tryStmt.body.push_back(stmt);
            }
            skipWhitespace(lexer, currentToken);
        }
        eat(lexer, currentToken, TOKEN_RBRACE);
    } else {
        std::string stmt = parseSingleStatement(lexer, currentToken);
        if (!stmt.empty()) {
            tryStmt.body.push_back(stmt);
        }
    }

    if (currentToken.type != TOKEN_CATCH) {
        throw std::runtime_error(makeParseError(currentToken,
            "Expected 'catch' after try block"));
    }
    eat(lexer, currentToken, TOKEN_CATCH);

    if (currentToken.type == TOKEN_LPAREN) {
        eat(lexer, currentToken, TOKEN_LPAREN);
        tryStmt.errorVar = currentToken.value;
        eat(lexer, currentToken, TOKEN_IDENTIFIER);
        eat(lexer, currentToken, TOKEN_RPAREN);
    } else {
        tryStmt.errorVar = currentToken.value;
        eat(lexer, currentToken, TOKEN_IDENTIFIER);
    }

    if (currentToken.type == TOKEN_LBRACE) {
        eat(lexer, currentToken, TOKEN_LBRACE);
        while (currentToken.type != TOKEN_RBRACE && currentToken.type != TOKEN_EOF) {
            if (currentToken.type == TOKEN_NEWLINE) {
                currentToken = lexer.nextToken();
                continue;
            }
            std::string stmt = parseSingleStatement(lexer, currentToken);
            if (!stmt.empty()) {
                tryStmt.catchBody.push_back(stmt);
            }
            skipWhitespace(lexer, currentToken);
        }
        eat(lexer, currentToken, TOKEN_RBRACE);
    } else {
        std::string stmt = parseSingleStatement(lexer, currentToken);
        if (!stmt.empty()) {
            tryStmt.catchBody.push_back(stmt);
        }
    }
    return tryStmt;
}

Value Parser::executeForStatement(const ForStatement& forStmt,
    std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    if (!forStmt.init.empty()) {
        parseLine(forStmt.init, variables, functions);
    }
    while (true) {
        if (!forStmt.condition.empty()) {
            Lexer condLexer(forStmt.condition);
            Token condToken = condLexer.nextToken();
            skipWhitespace(condLexer, condToken);
            auto condExpr = parseExpr(condLexer, condToken);
            Value condResult = condExpr->evaluate(variables, functions);
            if (!condResult.asBool()) break;
        }
        // Empty condition = infinite loop (C semantics, aligns with bytecode OP_TRUE)

        for (const auto& stmt : forStmt.body) {
            Value val = parseLine(stmt, variables, functions);
            if (val.getType() != Value::Type::Void) {
                return val;
            }
        }
        if (!forStmt.iter.empty()) {
            parseLine(forStmt.iter, variables, functions);
        }
    }
    return Value();
}

void Parser::parseImportStatement(Lexer& lexer, Token& currentToken,
    std::unordered_map<std::string, Value>& variables,
    std::unordered_map<std::string, Function>& functions) {
    skipWhitespace(lexer, currentToken);

    // Plugin import: !import name  (or  !import "path/to/file.fox")
    // Bare names resolve to: script directory -> C:\FoxLibs\ -> cwd, with
    // ".fox" appended. The file is imported like any other source file.
    if (currentToken.type == TOKEN_PLUGIN_IMPORT) {
        eat(lexer, currentToken, TOKEN_PLUGIN_IMPORT);
        if (currentToken.type == TOKEN_STRING) {
            std::string importPath = currentToken.value;
            eat(lexer, currentToken, TOKEN_STRING);
            std::string ns = fileStemOf(importPath);
            std::string alias;
            if (currentToken.type == TOKEN_ARROW) {
                eat(lexer, currentToken, TOKEN_ARROW);
                skipWhitespace(lexer, currentToken);
                if (currentToken.type != TOKEN_IDENTIFIER) {
                    throw std::runtime_error(makeParseError(currentToken,
                        "Expected an alias name after '->' in !import"));
                }
                alias = currentToken.value;
                eat(lexer, currentToken, TOKEN_IDENTIFIER);
            }
            std::string fullPath = resolveImportPath(importPath);
            if (!fileExists(fullPath)) {
                throw std::runtime_error(makeParseError(currentToken,
                    "Cannot find imported plugin file: " + importPath));
            }
            imported_source_files.push_back({fullPath, ns, alias});
            return;
        }
        std::string pluginName = currentToken.value;
        eat(lexer, currentToken, TOKEN_IDENTIFIER);
        std::string alias;
        if (currentToken.type == TOKEN_ARROW) {
            eat(lexer, currentToken, TOKEN_ARROW);
            skipWhitespace(lexer, currentToken);
            if (currentToken.type != TOKEN_IDENTIFIER) {
                throw std::runtime_error(makeParseError(currentToken,
                    "Expected an alias name after '->' in !import"));
            }
            alias = currentToken.value;
            eat(lexer, currentToken, TOKEN_IDENTIFIER);
        }
        std::vector<std::string> candidates;
        if (!import_base_file.empty()) {
            candidates.push_back(dirOf(import_base_file) + "/" + pluginName + ".fox");
        }
#ifdef _WIN32
        candidates.push_back("C:\\FoxLibs\\" + pluginName + ".fox");
#else
        // Linux: ~/.foxlibs (or $FOXLIB_PATH), then /usr/local/lib/foxlibs
        {
            const char* home = std::getenv("HOME");
            if (home && *home) {
                candidates.push_back(std::string(home) + "/.foxlibs/" + pluginName + ".fox");
            }
            if (const char* foxlib = std::getenv("FOXLIB_PATH"); foxlib && *foxlib) {
                candidates.push_back(std::string(foxlib) + "/" + pluginName + ".fox");
            }
            candidates.push_back("/usr/local/lib/foxlibs/" + pluginName + ".fox");
        }
#endif
        candidates.push_back(pluginName + ".fox");
        for (const auto& candidate : candidates) {
            if (fileExists(candidate)) {
                imported_source_files.push_back({candidate, pluginName, alias});
                return;
            }
        }
        throw std::runtime_error(makeParseError(currentToken,
            "Library not found: " + pluginName +
            " (searched the script directory, ~/.foxlibs, and the working directory)"));
    }

    eat(lexer, currentToken, TOKEN_IMPORT);

    // Cross-file import: import "path/to/file.fox"
    if (currentToken.type == TOKEN_STRING) {
        std::string importPath = currentToken.value;
        eat(lexer, currentToken, TOKEN_STRING);
        std::string fullPath = resolveImportPath(importPath);
        if (!fileExists(fullPath)) {
            throw std::runtime_error(makeParseError(currentToken,
                "Cannot find imported file: " + importPath +
                " (searched relative to the importing file, then the working directory)"));
        }
        imported_source_files.push_back({fullPath, import_prefix, import_alias});
        return;
    }

    std::string libName = currentToken.value;
    eat(lexer, currentToken, TOKEN_IDENTIFIER);

    std::string alias;
    if (currentToken.type == TOKEN_ARROW) {
        eat(lexer, currentToken, TOKEN_ARROW);
        skipWhitespace(lexer, currentToken);
        if (currentToken.type != TOKEN_IDENTIFIER) {
            throw std::runtime_error(makeParseError(currentToken, "Expected alias name after '->'"));
        }
        alias = currentToken.value;
        eat(lexer, currentToken, TOKEN_IDENTIFIER);
    }

    auto& libMgr = LibraryManager::getInstance();

    std::string internalName = libMgr.resolveExternalPath(libName);

    if (libMgr.isSystemLibrary(libName)) {

    }
    else if (libMgr.isLibraryAvailable(internalName)) {
        try {
            libMgr.loadExternalLibrary(internalName);
        }
        catch (const std::exception& e) {
            throw std::runtime_error("Failed to import library " + libName + ": " + e.what());
        }
    }
    else {
        throw std::runtime_error(makeParseError(currentToken, "Library not found: " + libName + ", please ensure the library file exists in C:\\FoxLibs\\ directory"));
    }

    std::string callName;
    if (!alias.empty()) {
        callName = alias;
    } else if (libMgr.isExternalPath(libName)) {
        size_t lastDot = libName.rfind('.');
        callName = (lastDot != std::string::npos) ? libName.substr(lastDot + 1) : libName;
    } else {
        callName = libName;
    }
    libMgr.markImported(internalName, callName);
}
