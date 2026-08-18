#include "lexer.hpp"

static int hexDigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void appendUtf8(std::string& out, int cp) {
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    }
    else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
    else if (cp <= 0xFFFF) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
    else if (cp <= 0x10FFFF) {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
    else {
        // Outside the Unicode range: emit U+FFFD
        out += "\xEF\xBF\xBD";
    }
}

Lexer::Lexer(const std::string& src) : source(src), pos(0), line(1), col(1) {
    // Skip a UTF-8 BOM so source files saved by Windows editors parse cleanly.
    if (source.size() >= 3 &&
        static_cast<unsigned char>(source[0]) == 0xEF &&
        static_cast<unsigned char>(source[1]) == 0xBB &&
        static_cast<unsigned char>(source[2]) == 0xBF) {
        pos = 3;
    }
}

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
    if (ident == "else") return "else";
    if (ident == "break") return "break";
    if (ident == "continue") return "continue";
    if (ident == "dict") return "dict";
    if (ident == "try") return "try";
    if (ident == "catch") return "catch";
    if (ident == "error") return "error";
    if (ident == "class") return "class";
    if (ident == "struct") return "struct";
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
    std::string result;
    bool closed = false;
    while (pos < source.size()) {
        char c = source[pos];
        if (c == '"') {
            pos++;
            col++;
            closed = true;
            break;
        }
        if (c == '\n') {
            line++;
            col = 1;
            pos++;
            continue;
        }
        if (c == '\\') {
            if (pos + 1 >= source.size()) {
                result += c;
                pos++;
                col++;
                continue;
            }
            char e = source[pos + 1];
            pos += 2;
            col += 2;
            switch (e) {
            case '"': result += '"'; break;
            case '\\': result += '\\'; break;
            case '/': result += '/'; break;
            case 'b': result += '\b'; break;
            case 'f': result += '\f'; break;
            case 'n': result += '\n'; break;
            case 'r': result += '\r'; break;
            case 't': result += '\t'; break;
            case 'e': result += '\x1b'; break; // ANSI escape (colors)
            case 'u': {
                int cp = 0;
                bool ok = true;
                for (int k = 0; k < 4; k++) {
                    if (pos + k >= source.size()) { ok = false; break; }
                    int hv = hexDigit(source[pos + k]);
                    if (hv < 0) { ok = false; break; }
                    cp = cp * 16 + hv;
                }
                if (ok) {
                    pos += 4;
                    col += 4;
                    // Try to combine a surrogate pair (\uD800-\uDBFF followed by
                    // \uDC00-\uDFFF) into a single code point.
                    if (cp >= 0xD800 && cp <= 0xDBFF &&
                        pos + 6 <= source.size() &&
                        source[pos] == '\\' && source[pos + 1] == 'u') {
                        int lo = 0;
                        bool ok2 = true;
                        for (int k = 2; k < 6; k++) {
                            int hv = hexDigit(source[pos + k]);
                            if (hv < 0) { ok2 = false; break; }
                            lo = lo * 16 + hv;
                        }
                        if (ok2 && lo >= 0xDC00 && lo <= 0xDFFF) {
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                            pos += 6;
                            col += 6;
                        }
                    }
                    appendUtf8(result, cp);
                }
                else {
                    // Malformed \u escape: keep the original text.
                    result += '\\';
                    result += 'u';
                }
                break;
            }
            default:
                // Unknown escape: keep it verbatim (e.g. Windows paths "C:\temp").
                result += '\\';
                result += e;
                break;
            }
            continue;
        }
        result += c;
        pos++;
        col++;
    }
    if (!closed) {
        throw std::runtime_error("Syntax error: " + std::to_string(line) + ":" + std::to_string(col) + ": unterminated string (missing closing double quote)");
    }
    return result;
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
    if (source.compare(pos, 7, "!import") == 0) {
        size_t after = pos + 7;
        if (after >= source.size() || source[after] == ' ' || source[after] == '\t' ||
            source[after] == '\r' || source[after] == '\n') {
            pos += 7;
            col += 7;
            return makeToken(TOKEN_PLUGIN_IMPORT, "!import", line, col - 7);
        }
    }
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
    case '*': pos++; col++; return makeToken(TOKEN_MUL, "*", tokenLine, tokenCol);
    case '/': pos++; col++; return makeToken(TOKEN_DIV, "/", tokenLine, tokenCol);
    case '%': pos++; col++; return makeToken(TOKEN_MOD, "%", tokenLine, tokenCol);
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
            if (ident == "free") return makeToken(TOKEN_FREE, "free", tokenLine, tokenCol);
            if (ident == "free_all") return makeToken(TOKEN_FREE_ALL, "free_all", tokenLine, tokenCol);
            if (ident == "import") return makeToken(TOKEN_IMPORT, "import", tokenLine, tokenCol);
            if (ident == "new") return makeToken(TOKEN_NEW, "new", tokenLine, tokenCol);
            if (ident == "else") return makeToken(TOKEN_ELSE, "else", tokenLine, tokenCol);
            if (ident == "break") return makeToken(TOKEN_BREAK, "break", tokenLine, tokenCol);
            if (ident == "continue") return makeToken(TOKEN_CONTINUE, "continue", tokenLine, tokenCol);
            if (ident == "dict") return makeToken(TOKEN_DICT, "dict", tokenLine, tokenCol);
            if (ident == "try") return makeToken(TOKEN_TRY, "try", tokenLine, tokenCol);
            if (ident == "catch") return makeToken(TOKEN_CATCH, "catch", tokenLine, tokenCol);
            if (ident == "error") return makeToken(TOKEN_ERROR, "error", tokenLine, tokenCol);
            if (ident == "class") return makeToken(TOKEN_CLASS, "class", tokenLine, tokenCol);
            if (ident == "struct") return makeToken(TOKEN_STRUCT, "struct", tokenLine, tokenCol);
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
