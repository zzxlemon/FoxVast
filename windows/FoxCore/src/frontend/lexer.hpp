#pragma once
#include "token.hpp"
#include <string>
#include <stdexcept>
#include <cctype>

class Lexer {
private:
    std::string source;
    size_t pos = 0;
    int line = 1;
    int col = 1;

    void skipWhitespaceExceptNewline();
    void skipComments();
    void skipLineComment();
    void skipBlockComment();
    std::string readIdentifier();
    std::string readNumber();
    std::string readString();
    bool readArrow();
	bool readLeftArrow();
    Token makeToken(TokenT type, const std::string& value);
    Token makeToken(TokenT type, const std::string& value, int tokenLine, int tokenCol);

public:
    explicit Lexer(const std::string& src);
    Token nextToken();
    void ungetToken();
};