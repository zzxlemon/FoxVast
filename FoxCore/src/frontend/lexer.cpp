#include "lexer.hpp"

Lexer::Lexer(const std::string& src) : source(src), pos(0), line(1), col(1) {}

Token Lexer::makeToken(TokenT type, const std::string& value) {
    return Token(type, value, line, col);
}

Token Lexer::makeToken(TokenT type, const std::string& value, int tokenLine, int tokenCol) {
    return Token(type, value, tokenLine, tokenCol);
}

void Lexer::skipWhitespaceExceptNewline() {
    while (pos < source.size()) {
        char c = source[pos];
        if (c == ' ' || c == '\t' || c == '\r') {
            pos++;
            col++;
        }
        else if (c == '\n') {
            break;
        }
        else {
            break;
        }
    }
}

void Lexer::skipComments() {
    while (pos < source.size()) {
        if (source[pos] == '#') {
            if (pos + 1 < source.size() && source[pos + 1] == '*') {
                skipBlockComment();
            }
            else {
                skipLineComment();
            }
            skipWhitespaceExceptNewline();
            continue;
        }
        break;
    }
}

void Lexer::skipLineComment() {
    while (pos < source.size() && source[pos] != '\n') {
        pos++;
        col++;
    }
}

void Lexer::skipBlockComment() {
    pos += 2;
    col += 2;
    while (pos < source.size()) {
        if (source[pos] == '\n') {
            pos++;
            line++;
            col = 1;
            continue;
        }
        if (source[pos] == '*' && pos + 1 < source.size() && source[pos + 1] == '#') {
            pos += 2;
            col += 2;
            return;
        }
        pos++;
        col++;
    }
    throw std::runtime_error("Syntax error: " + std::to_string(line) + ":" + std::to_string(col) + ": unterminated block comment");
}

std::string Lexer::readIdentifier() {
    size_t start = pos;
    while (pos < source.size() && (isalpha(static_cast<unsigned char>(source[pos])) || source[pos] == '_' || isdigit(static_cast<unsigned char>(source[pos])) || source[pos] == '.')) {
        pos++;
    }
    std::string ident = source.substr(start, pos - start);
    col += static_cast<int>(pos - start);
    if (ident == "func") return "func";
    if (ident == "void") return "void";
    if (ident == "int") return "int";
    if (ident == "string") return "string"; 
    if (ident == "double") return "double";
    if (ident == "print") return "print";
    if (ident == "println") return "println";
    if (ident == "ret") return "ret";
    if (ident == "input") return "input";
    if (ident == "if") return "if";
    if (ident == "or") return "or";
    if (ident == "and") return "and";
	if (ident == "while") return "while";
	if (ident == "end") return "end";
    if (ident == "exit") return "exit";
	if (ident == "import") return "import";
    if (ident == "for") return "for";
    if (ident == "new") return "new";
    return ident;
}

std::string Lexer::readNumber() {
    size_t start = pos;
    bool hasDot = false;
    while (pos < source.size()) {
        if (isdigit(static_cast<unsigned char>(source[pos]))) {
            pos++;
        }
        else if (source[pos] == '.' && !hasDot) {
            hasDot = true;
            pos++;
        }
        else {
            break;
        }
    }
    col += static_cast<int>(pos - start);
    return source.substr(start, pos - start);
}

std::string Lexer::readString() {
    int startCol = col;
    pos++;
    col++;
    size_t start = pos;
    while (pos < source.size() && source[pos] != '"') {
        if (source[pos] == '\n') {
            line++;
            col = 1;
        } else {
            col++;
        }
        pos++;
    }
    if (pos >= source.size()) {
        throw std::runtime_error("Syntax error: " + std::to_string(line) + ":" + std::to_string(col) + ": unterminated string (missing closing double quote)");
    }
    std::string str = source.substr(start, pos - start);
    pos++;
    col++;
    return str;
}

bool Lexer::readArrow() {
    if (pos + 1 < source.size() && source[pos] == '-' && source[pos + 1] == '>') {
        pos += 2;
        col += 2;
        return true;
    }
    return false;
}

bool Lexer::readLeftArrow() {
    if (pos + 1 < source.size() && source[pos] == '<' && source[pos + 1] == '-') {
        pos += 2;
        col += 2;
        return true;
    }
    return false;
}

Token Lexer::nextToken() {
    skipWhitespaceExceptNewline();
    skipComments();
    if (pos >= source.size()) {
        return makeToken(TOKEN_EOF, "", line, col);
    }

    char c = source[pos];

    if (c == '\n') {
        pos++;
        int prevCol = col;
        line++;
        col = 1;
        return Token(TOKEN_NEWLINE, "\n", line - 1, prevCol);
    }

    int tokenLine = line;
    int tokenCol = col;

    if (c == '-') {
        if (readArrow()) {
            return makeToken(TOKEN_ARROW, "->", tokenLine, tokenCol);
        }
        else {
            pos++;
            col++;
            return makeToken(TOKEN_MINUS, "-", tokenLine, tokenCol);
        }
    }
    else if (c == ':') {
        pos++;
        col++;
        return makeToken(TOKEN_COLON, ":", tokenLine, tokenCol);
    }
    else if (c == '{') {
        pos++;
        col++;
        return makeToken(TOKEN_LBRACE, "{", tokenLine, tokenCol);
    }
    else if (c == '}') {
        pos++;
        col++;
        return makeToken(TOKEN_RBRACE, "}", tokenLine, tokenCol);
    }

    if (c == '=') {
        if (pos + 1 < source.size() && source[pos + 1] == '=') {
            pos += 2;
            col += 2;
            return makeToken(TOKEN_EQ, "==", tokenLine, tokenCol);
        }
        else {
            pos++;
            col++;
            return makeToken(TOKEN_EQUAL, "=", tokenLine, tokenCol);
        }
    }
    else if (c == '!') {
        if (pos + 1 < source.size() && source[pos + 1] == '=') {
            pos += 2;
            col += 2;
            return makeToken(TOKEN_NE, "!=", tokenLine, tokenCol);
        }
        else {
            pos++;
            col++;
            return makeToken(TOKEN_NOT, "!", tokenLine, tokenCol);
        }
    }
    else if (c == '>') {
        if (pos + 1 < source.size() && source[pos + 1] == '=') {
            pos += 2;
            col += 2;
            return makeToken(TOKEN_GE, ">=", tokenLine, tokenCol);
        }
        else {
            pos++;
            col++;
            return makeToken(TOKEN_GT, ">", tokenLine, tokenCol);
        }
    }
    else if (c == '<') {
        if (readLeftArrow()) {
            return makeToken(TOKEN_LEFT_ARROW, "<-", tokenLine, tokenCol);
        }
        else if (pos + 1 < source.size() && source[pos + 1] == '=') {
            pos += 2;
            col += 2;
            return makeToken(TOKEN_LE, "<=", tokenLine, tokenCol);
        }
        else {
            pos++;
            col++;
            return makeToken(TOKEN_LT, "<", tokenLine, tokenCol);
        }
    }

    switch (c) {
    case '+': pos++; col++; return makeToken(TOKEN_PLUS, "+", tokenLine, tokenCol);
    case '(': pos++; col++; return makeToken(TOKEN_LPAREN, "(", tokenLine, tokenCol);
    case ')': pos++; col++; return makeToken(TOKEN_RPAREN, ")", tokenLine, tokenCol);
    case '[': pos++; col++; return makeToken(TOKEN_LBRACKET, "[", tokenLine, tokenCol);
    case ']': pos++; col++; return makeToken(TOKEN_RBRACKET, "]", tokenLine, tokenCol);
    case ';': pos++; col++; return makeToken(TOKEN_SEMICOLON, ";", tokenLine, tokenCol);
    case '"': {
        std::string str = readString();
        return makeToken(TOKEN_STRING, str, tokenLine, tokenCol);
    }
    case ',': pos++; col++; return makeToken(TOKEN_COMMA, ",", tokenLine, tokenCol);
    case '.': pos++; col++; return makeToken(TOKEN_DOT, ".", tokenLine, tokenCol);
    default:
        if (isalpha(static_cast<unsigned char>(c)) || c == '_') {
            std::string ident = readIdentifier();
            if (ident == "func") return makeToken(TOKEN_FUNC, "func", tokenLine, tokenCol);
            if (ident == "void") return makeToken(TOKEN_VOID, "void", tokenLine, tokenCol);
            if (ident == "int") {
                size_t tempPos = pos;
                while (tempPos < source.size() && (source[tempPos] == ' ' || source[tempPos] == '\t' || source[tempPos] == '\r')) {
                    tempPos++;
                }
                if (tempPos < source.size() && source[tempPos] == '(') {
                    return makeToken(TOKEN_INT_CAST, "int", tokenLine, tokenCol);
                }
                return makeToken(TOKEN_INT, "int", tokenLine, tokenCol);
            }
            if (ident == "string") return makeToken(TOKEN_STRING_TYPE, "string", tokenLine, tokenCol);
            if (ident == "double") {
                size_t tempPos = pos;
                while (tempPos < source.size() && (source[tempPos] == ' ' || source[tempPos] == '\t' || source[tempPos] == '\r')) {
                    tempPos++;
                }
                if (tempPos < source.size() && source[tempPos] == '(') {
                    return makeToken(TOKEN_DOUBLE_CAST, "double", tokenLine, tokenCol);
                }
                return makeToken(TOKEN_DOUBLE, "double", tokenLine, tokenCol);
            }
            if (ident == "print") return makeToken(TOKEN_PRINT, "print", tokenLine, tokenCol);
            if (ident == "println") return makeToken(TOKEN_PRINTLN, "println", tokenLine, tokenCol);
            if (ident == "ret" || ident == "RET") return makeToken(TOKEN_RET, "ret", tokenLine, tokenCol);
            if (ident == "input") return makeToken(TOKEN_INPUT, "input", tokenLine, tokenCol);
            if (ident == "if") return makeToken(TOKEN_IF, "if", tokenLine, tokenCol);
            if (ident == "or" || ident == "OR") return makeToken(TOKEN_OR, "or", tokenLine, tokenCol);
            if (ident == "and" || ident == "AND") return makeToken(TOKEN_AND, "and", tokenLine, tokenCol);
            if (ident == "for") return makeToken(TOKEN_FOR, "for", tokenLine, tokenCol);
            if (ident == "fn") return makeToken(TOKEN_FN, "fn", tokenLine, tokenCol);
            if (ident == "goto") return makeToken(TOKEN_GOTO, "goto", tokenLine, tokenCol);
            if (ident == "while") return makeToken(TOKEN_WHILE, "while", tokenLine, tokenCol);
            if (ident == "endl" || ident == "ENDL") return makeToken(TOKEN_ENDL, "endl", tokenLine, tokenCol);
            if (ident == "exit") return makeToken(TOKEN_EXIT, "exit", tokenLine, tokenCol);
            if (ident == "import") return makeToken(TOKEN_IMPORT, "import", tokenLine, tokenCol);
            if (ident == "new") return makeToken(TOKEN_NEW, "new", tokenLine, tokenCol);
            return makeToken(TOKEN_IDENTIFIER, ident, tokenLine, tokenCol);
        }
        else if (isdigit(static_cast<unsigned char>(c))) {
            std::string num = readNumber();
            if (num.find('.') != std::string::npos) {
                return makeToken(TOKEN_DOUBLE_NUM, num, tokenLine, tokenCol);
            }
            else {
                return makeToken(TOKEN_NUMBER, num, tokenLine, tokenCol);
            }
        }
        else {
            throw std::runtime_error("Syntax error: " + std::to_string(line) + ":" + std::to_string(col) + ": invalid character: " + std::string(1, c));
        }
    }
}

void Lexer::ungetToken() {
    if (pos > 0) pos--;
}
