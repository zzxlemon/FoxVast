#pragma once
#include <string>

// Token 类型枚举
enum TokenT {
    TOKEN_IDENTIFIER,    
    TOKEN_NUMBER,         
    TOKEN_DOUBLE_NUM,     
    TOKEN_STRING,         
    TOKEN_PLUS,           
    TOKEN_MINUS,          
    TOKEN_EQUAL,          
    TOKEN_LPAREN,         
    TOKEN_RPAREN,         
    TOKEN_PRINT,          
    TOKEN_PRINTLN,        
    TOKEN_EOF,            
    TOKEN_FUNC,           
    TOKEN_VOID,           
    TOKEN_INT,            
    TOKEN_STRING_TYPE,    
    TOKEN_DOUBLE,         
    TOKEN_ARROW,          
    TOKEN_LEFT_ARROW,     
    TOKEN_LBRACE,         
    TOKEN_RBRACE,         
    TOKEN_COLON,          
    TOKEN_RET,            
    TOKEN_NEWLINE,        
    TOKEN_INPUT,          
    TOKEN_INT_CAST,       
    TOKEN_DOUBLE_CAST,    
    TOKEN_IF,             
    TOKEN_OR,             
    TOKEN_AND,            
    TOKEN_EQ,             
    TOKEN_NE,             
    TOKEN_GT,             
    TOKEN_LT,             
    TOKEN_GE,             
    TOKEN_LE,             
    TOKEN_COMMA,          
    TOKEN_WHILE,          
    TOKEN_ENDL,           
    TOKEN_EXIT,           
    TOKEN_FREE,
    TOKEN_FREE_ALL,
    TOKEN_IMPORT,         
    TOKEN_FOR,            
    TOKEN_FN,             
    TOKEN_GOTO,           
    TOKEN_LBRACKET,       
    TOKEN_RBRACKET,       
    TOKEN_SEMICOLON,      
    TOKEN_DOT,            
    TOKEN_NEW,
    TOKEN_NOT,
    TOKEN_MUL,
    TOKEN_DIV,
    TOKEN_MOD,
    TOKEN_ELSE,
    TOKEN_BREAK,
    TOKEN_CONTINUE,
    TOKEN_DICT,
    TOKEN_TRY,
    TOKEN_CATCH,
    TOKEN_ERROR
};

struct Token {
    TokenT type;
    std::string value;
    int line;
    int col;

    Token(TokenT t, const std::string& v, int l = 1, int c = 1)
        : type(t), value(v), line(l), col(c) {}

    std::string position() const {
        return std::to_string(line) + ":" + std::to_string(col);
    }
};
