#include "parser.hpp"
#include "../util/common.hpp"
#include "../interpreter/library_manager.hpp" 
#include <iostream>
#include <algorithm>

static int g_funcNewAllocBytes = -1;
static const int MAX_FUNC_NEW_BYTES = 512;

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
    default: return "token";
    }
}

void Parser::skipWhitespace(Lexer& lexer, Token& currentToken) {
    while (currentToken.type != TOKEN_EOF && !currentToken.value.empty()
        && currentToken.type != TOKEN_STRING
        && isspace(static_cast<unsigned char>(currentToken.value[0]))
        && currentToken.value[0] != '\n') {
        currentToken = lexer.nextToken();
    }
}

void Parser::eat(Lexer& lexer, Token& currentToken, TokenT expectedType) {
    if (currentToken.type == expectedType) {
        currentToken = lexer.nextToken();
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
        std::string fullName = token.value;

        // Handle dot notation: lib.func or lib.func(...)
        while (currentToken.type == TOKEN_DOT) {
            eat(lexer, currentToken, TOKEN_DOT);
            fullName += "." + currentToken.value;
            eat(lexer, currentToken, TOKEN_IDENTIFIER);
        }

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

std::unique_ptr<Expr> Parser::parseAdd(Lexer& lexer, Token& currentToken) {
    auto left = parsePrimary(lexer, currentToken);
    while (currentToken.type == TOKEN_PLUS || currentToken.type == TOKEN_MINUS) {
        TokenT op = currentToken.type;
        eat(lexer, currentToken, op);
        auto right = parsePrimary(lexer, currentToken);
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

    if (currentToken.type == TOKEN_IF || currentToken.type == TOKEN_WHILE || currentToken.type == TOKEN_FOR) {
        int braceDepth = 0;
        bool insideBlock = false;
        while (currentToken.type != TOKEN_EOF) {
            if (currentToken.type == TOKEN_LBRACE) {
                insideBlock = true;
                braceDepth++;
            }
            else if (currentToken.type == TOKEN_RBRACE && insideBlock) {
                braceDepth--;
                if (braceDepth == 0) {
                    stmt += "} ";
                    currentToken = lexer.nextToken();
                    skipWhitespace(lexer, currentToken);
                    break;
                }
            }
            else if (!insideBlock && (currentToken.type == TOKEN_NEWLINE || currentToken.type == TOKEN_SEMICOLON)) {
                if (currentToken.type == TOKEN_SEMICOLON) {
                    stmt += "; ";
                }
                currentToken = lexer.nextToken();
                break;
            }
            if (currentToken.type == TOKEN_STRING) {
                stmt += "\"" + currentToken.value + "\"";
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
        while (currentToken.type != TOKEN_EOF && currentToken.type != TOKEN_NEWLINE && currentToken.type != TOKEN_RBRACE) {
            if (currentToken.type == TOKEN_STRING) {
                stmt += "\"" + currentToken.value + "\"";
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

void Parser::parseFunction() {
    eat(funcLexer, funcCurrentToken, TOKEN_FUNC);
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

    funcCurrentToken = funcLexer.nextToken();
    skipWhitespace(funcLexer, funcCurrentToken);

    while (funcCurrentToken.type != TOKEN_RBRACE && funcCurrentToken.type != TOKEN_EOF) {
        std::string line = parseSingleStatement(funcLexer, funcCurrentToken);
        if (!line.empty()) {
            func.body.push_back(line);
            try {
                func.compiledBody.push_back(std::move(parseLineToStmt(line)));
            } catch (...) {
                // Type declarations (int x = 5) don't compile to Stmts; store nullptr
                func.compiledBody.push_back(nullptr);
            }
        }
        skipWhitespace(funcLexer, funcCurrentToken);
    }

    eat(funcLexer, funcCurrentToken, TOKEN_RBRACE);
    tempFunctions.push_back(func);
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
        else if (funcCurrentToken.type == TOKEN_IMPORT) {
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
            "FoxLang does not use semicolons. Remove the ';' and start a new line instead."));
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
        } else if (nextToken.type == TOKEN_DOT) {
            currentToken = nextToken;
            eat(lineLexer, currentToken, TOKEN_DOT);
            std::string funcName = currentToken.value;
            eat(lineLexer, currentToken, TOKEN_IDENTIFIER);
            if (currentToken.type == TOKEN_LPAREN) {
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
                handler.onCall(identName + "." + funcName, std::move(args));
            } else {
                std::string nextVal = currentToken.value.empty()
                    ? "<" + std::string(tokenTypeName(currentToken.type)) + ">"
                    : "'" + currentToken.value + "'";
                throw std::runtime_error(makeParseError(currentToken,
                    "Expected '(' after function name in qualified call, got " + nextVal));
            }
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
            Value val = arg->evaluate(variables, functions);
            switch (val.getType()) {
            case Value::Type::Int: std::cout << val.asInt(); break;
            case Value::Type::Double: std::cout << val.asDouble(); break;
            case Value::Type::String: std::cout << val.asString(); break;
            case Value::Type::Void: break;
            case Value::Type::Array: std::cout << "[array]"; break;
            }
        }
        void onPrintln(std::unique_ptr<Expr> arg) override {
            Value val = arg->evaluate(variables, functions);
            switch (val.getType()) {
            case Value::Type::Int: std::cout << val.asInt(); break;
            case Value::Type::Double: std::cout << val.asDouble(); break;
            case Value::Type::String: std::cout << val.asString(); break;
            case Value::Type::Void: break;
            case Value::Type::Array: std::cout << "[array]"; break;
            }
            std::cout << std::endl;
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
        void onFnLabel(const std::string& name) override {}
        void onGoto(const std::string& name) override {
            throw GotoException(name);
        }
    };
}

// ============================================================
// CompilingHandler — builds Stmt nodes without executing
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
            stmt = std::make_unique<IfStmt>(ifStmt.condition, Parser::compileBody(ifStmt.body));
        }
        void onWhile(WhileStatement whileStmt) override {
            stmt = std::make_unique<WhileStmt>(whileStmt.condition, Parser::compileBody(whileStmt.body));
        }
        void onFor(ForStatement forStmt) override {
            stmt = std::make_unique<ForStmt>(forStmt.init, forStmt.condition, forStmt.iter, Parser::compileBody(forStmt.body));
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
    // Skip type declarations (int/double/string var = expr) — not expressible as Stmts
    std::string trimmed = line;
    size_t start = trimmed.find_first_not_of(" \t");
    if (start != std::string::npos) trimmed = trimmed.substr(start);
    if (trimmed.rfind("int ", 0) == 0 || trimmed.rfind("double ", 0) == 0 || trimmed.rfind("string ", 0) == 0) {
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

    if (isOutInfo) {
        std::cout << "[执行] 输入赋值：" << varName << " = \"" << variables[varName].asString() << "\"" << std::endl;
    }
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
        if (currentToken.type == TOKEN_LPAREN) parenDepth++;
        if (currentToken.type == TOKEN_RPAREN) {
            if (parenDepth == 0) break;
            parenDepth--;
        }
        if (currentToken.type == TOKEN_STRING)
            condStr += "\"" + currentToken.value + "\" ";
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
        if (currentToken.type == TOKEN_LPAREN) parenDepth++;
        if (currentToken.type == TOKEN_RPAREN) {
            if (parenDepth == 0) break;
            parenDepth--;
        }
        if (currentToken.type == TOKEN_STRING)
            condStr += "\"" + currentToken.value + "\" ";
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
        if (currentToken.type == TOKEN_STRING)
            init += "\"" + currentToken.value + "\" ";
        else
            init += currentToken.value + " ";
        currentToken = lexer.nextToken();
        skipWhitespace(lexer, currentToken);
    }
    eat(lexer, currentToken, TOKEN_SEMICOLON);
    std::string condition;
    while (currentToken.type != TOKEN_SEMICOLON && currentToken.type != TOKEN_EOF) {
        if (currentToken.type == TOKEN_STRING)
            condition += "\"" + currentToken.value + "\" ";
        else
            condition += currentToken.value + " ";
        currentToken = lexer.nextToken();
        skipWhitespace(lexer, currentToken);
    }
    eat(lexer, currentToken, TOKEN_SEMICOLON);
    std::string iter;
    int iterParenDepth = 0;
    while (currentToken.type != TOKEN_EOF) {
        if (currentToken.type == TOKEN_LPAREN) iterParenDepth++;
        if (currentToken.type == TOKEN_RPAREN) {
            if (iterParenDepth == 0) break;
            iterParenDepth--;
        }
        if (currentToken.type == TOKEN_STRING)
            iter += "\"" + currentToken.value + "\" ";
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
        else {
            break;
        }

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
    eat(lexer, currentToken, TOKEN_IMPORT);

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