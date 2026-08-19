#pragma once
#include <string>
#include <vector>
#include <memory>

class Stmt; // forward decl from ast.hpp

struct Parameter {
    std::string name;
    std::string type;
};

struct Function {
    std::string name;              
    std::string returnType;        
    std::vector<Parameter> parameters; 
    std::vector<std::string> body;              // raw source lines (used by bytecode compiler)
    std::vector<int> bodyLines;                 // file-absolute line number of each body entry
    std::vector<std::shared_ptr<Stmt>> compiledBody; // pre-compiled AST (used by interpreter)
};