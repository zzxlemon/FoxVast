#include "./util/common.hpp"
#include "./interpreter/interpreter.hpp"
#include "./util/utils.hpp"
#include "./util/error_reporter.hpp"
#include "./vm/bytecode.hpp"
#include "./vm/far.hpp"
#include <iostream>
#include <cstdio>
#include <fstream>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

std::string fz_name_file = "";
std::string fz_name_file_v = "";
const char XOR_KEY = 0x2A;
std::string encrypt_key = "";

static void print_help(){
    std::cout << "FoxVast Interpreter - Usage:" << std::endl;
    std::cout << "  -f <file>               Run FoxVast source file(Deprecated)" << std::endl;
    std::cout << "  -c <file>               Compile .fox -> .fc bytecode" << std::endl;
    std::cout << "  -fc <file>              Run .fc bytecode file" << std::endl;
    std::cout << "  -d <file>               Disassemble .fc bytecode file" << std::endl;
    std::cout << "  -p <out.far> <files>    Package files into .far archive" << std::endl;
    std::cout << "  -u <archive.far> [dir]  Extract .far archive" << std::endl;
    std::cout << "  -far <archive.far>      Run main.fc from .far archive" << std::endl;
    std::cout << "  -version                Show version info" << std::endl;
    std::cout << "  OutInfo=true            Enable verbose output(Deprecated)" << std::endl;
    std::cout << "  pack=true               Encrypt .fox -> .fz file" << std::endl;
    std::cout << "  pack=false              Decrypt .fz -> .fox file" << std::endl;
}

static void CreateFile(const std::string& source_code) {
    std::ofstream outfile(fz_name_file_v, std::ios::binary);
    if (outfile.is_open()) {
        outfile.write(source_code.c_str(), source_code.size());
        outfile.close();
        std::cout << "File " << fz_name_file_v << " created." << std::endl;
    }
    else {
        ErrorReporter::reportSimple("FileError", "File " + fz_name_file_v + " open/create failed");
    }
}

static void fz(const std::string& key) {
    encrypt_file_with_key(fz_name_file, fz_name_file_v, key);
}

static void fz1(const std::string& key) {
    std::string source_code = decrypt_file_with_key(fz_name_file, key);
    CreateFile(source_code);
}

int main(int argc, char** argv) { 
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    Interpreter interpreter;
    if (argc == 1) {
        print_help();
        return 0;  
    }

    int v1 = 1;
    std::string filename;
    std::string just_str;
    bool bfz = true;
    for (int i = 1; i < argc; i++) {
        std::string in = argv[i];
        if (in == "-version" || in == "-v") {
            printf("FoxVast Version: '%s'\n", fox_version.c_str());
            printf("FoxVast-VM 64-Bit VM (beta %s, mixed mode)\n", fox_vm_version.c_str());

        }
        else if (in == "-f" || in == "--file") {
            if (i + 1 >= argc) {
                ErrorReporter::reportSimple("ArgumentError", "'-f' requires a filename");
                return 1;
            }
            filename = argv[i + 1];
            i++;
        }
        else if (in == "-s" || in == "--str"){
            if (i + 1 >= argc) {
                ErrorReporter::reportSimple("ArgumentError", "'-s' requires an encoding format");
                return 1;
            }
            just_str = argv[i + 1];

            if (just_str == "utf-8" || just_str == "UTF-8"){
#ifdef _WIN32
                system("chcp 65001");
#endif
            }
            else if (just_str == "gbk" || just_str == "GBK"){
#ifdef _WIN32
                system("chcp 936");
#endif
            }
            else{
                ErrorReporter::reportSimple("ArgumentError", "'-s'/'--str' requires an encoding format (utf-8 or gbk)");
                return 1;
            }
            i++;
        }
        else if (in == "OutInfo=true") {
            isOutInfo = true;
        }
        else if (in == "pack=true") {
            v1 = 2;
            if (i + 3 >= argc) {
                ErrorReporter::reportSimple("ArgumentError", "pack=true requires 3 args: input output key");
                return 1;
            }
            fz_name_file = argv[i + 1];
            fz_name_file += ".fx";
            fz_name_file_v = argv[i + 2];
            fz_name_file_v += ".fz";
            encrypt_key = argv[i + 3];
            i += 3;
        }
        else if (in == "pack=false") {
            v1 = 3;
            if (i + 3 >= argc) {
                ErrorReporter::reportSimple("ArgumentError", "pack=false requires 3 args: input output key");
                return 1;
            }
            fz_name_file = argv[i + 1];
            fz_name_file += ".fz";
            fz_name_file_v = argv[i + 2];
            fz_name_file_v += ".fx";
            encrypt_key = argv[i + 3];
            i += 3;
        }
        else if (in == "-c" || in == "--compile") {
            if (i + 1 >= argc) {
                ErrorReporter::reportSimple("ArgumentError", "'-c' requires a filename");
                return 1;
            }
            std::vector<std::string> files;
            for (int j = i + 1; j < argc; j++) {
                if (argv[j][0] == '-') break;
                files.push_back(argv[j]);
            }
            if (files.empty()) {
                ErrorReporter::reportSimple("ArgumentError", "'-c' requires a filename");
                return 1;
            }
            i += static_cast<int>(files.size());

            // Compile with import inlining: !import plugins and import "file.fox"
            // are expanded into the .fc, so a single entry file is
            // self-contained (plugins resolve from the script dir, C:\FoxLibs,
            // and the working directory).
            BytecodeCompiler::s_expandImports = true;
            for (const auto& f : files) {
                std::string fullCode = read_file(f);
                if (fullCode.empty()) return 1;
                try {
                    BytecodeCompiler compiler;
                    CompiledProgram prog = compiler.compile(fullCode, f);
                    std::vector<uint8_t> fcData = prog.serialize();
                    xor_crypt(fcData, FC_XOR_KEY);
                    std::string basePath = f;
                    size_t dot = basePath.rfind('.');
                    if (dot != std::string::npos) basePath = basePath.substr(0, dot);
                    size_t sep = basePath.find_last_of("/\\");
                    std::string baseName = (sep != std::string::npos) ? basePath.substr(sep + 1) : basePath;
                    std::string parentDir = (sep != std::string::npos) ? basePath.substr(0, sep) : ".";
                    std::string outName = parentDir + "/" + baseName + ".fc";
                    std::ofstream fcout(outName, std::ios::binary);
                    if (fcout.is_open()) {
                        fcout.write(reinterpret_cast<const char*>(fcData.data()), fcData.size());
                        fcout.close();
                        std::cout << "Compiled to " << outName << " (" << fcData.size() << " bytes)" << std::endl;
                    } else {
                        ErrorReporter::reportSimple("FileError", "Cannot create .fc file: " + outName);
                        return 1;
                    }
                } catch (const std::exception& e) {
                    if (!ErrorReporter::hasError()) {
                        ErrorReporter::reportFromException("CompileError", e.what());
                    }
                    return 1;
                }
            }
            return 0;
        }
        else if (in == "-fc" || in == "--fc-run") {
            if (i + 1 >= argc) {
                ErrorReporter::reportSimple("ArgumentError", "'-fc' requires a filename");
                return 1;
            }
            std::vector<std::string> files;
            for (int j = i + 1; j < argc; j++) {
                if (argv[j][0] == '-') break;
                files.push_back(argv[j]);
            }
            if (files.empty()) {
                ErrorReporter::reportSimple("ArgumentError", "'-fc' requires a filename");
                return 1;
            }
            i += static_cast<int>(files.size());

            // Multi-module run: every resolved .fc is loaded into one VM
            // (the first is the entry program; the rest resolve cross-module
            // function/class references).
            try {
                RegFunc(); // initialize system libraries for function resolution
                VM vm;
                bool first = true;
                for (const auto& f : files) {
                    // Try direct path first, then parentDir\.fc\<basename>.fc
                    std::string fcFile = f;
                    if (access(fcFile.c_str(), 0) != 0 || f.find(".fc") == std::string::npos) {
                        size_t dot = f.rfind('.');
                        std::string base = (dot != std::string::npos) ? f.substr(0, dot) : f;
                        size_t sep = base.find_last_of("/\\");
                        std::string baseName = (sep != std::string::npos) ? base.substr(sep + 1) : base;
                        std::string parentDir = (sep != std::string::npos) ? base.substr(0, sep) : ".";
                        std::string altPath = parentDir + "/" + baseName + ".fc";
                        if (access(altPath.c_str(), 0) == 0) fcFile = altPath;
                    }
                    std::string fullCode = read_file(fcFile);
                    if (fullCode.empty()) return 1;
                    std::vector<uint8_t> fcData(fullCode.begin(), fullCode.end());
                    xor_crypt(fcData, FC_XOR_KEY);
                    CompiledProgram prog = CompiledProgram::deserialize(fcData);
                    if (first) {
                        // Attach the matching .fox source (if present) so runtime
                        // errors can highlight the offending source line.
                        std::string srcPath = fcFile;
                        size_t dotPos = srcPath.rfind('.');
                        if (dotPos != std::string::npos) srcPath = srcPath.substr(0, dotPos);
                        srcPath += ".fox";
                        std::string srcCode = read_file(srcPath);
                        if (!srcCode.empty()) {
                            ErrorReporter::setSource(srcPath, srcCode);
                        }
                        vm.loadProgram(prog);
                        first = false;
                    } else {
                        vm.addProgram(prog);
                    }
                }
                vm.run();
                if (ErrorReporter::hasError()) return 1;
            } catch (const std::exception& e) {
                if (!ErrorReporter::hasError()) {
                    ErrorReporter::reportFromException("RuntimeError", e.what());
                }
                return 1;
            }
            return 0;
        }
        else if (in == "-d" || in == "--disassemble") {
            if (i + 1 >= argc) {
                ErrorReporter::reportSimple("ArgumentError", "'-d' requires a filename");
                return 1;
            }
            filename = argv[i + 1];
            i++;
            // Same .fc path resolution as -fc
            std::string fcFile = filename;
            if (access(fcFile.c_str(), 0) != 0 || filename.find(".fc") == std::string::npos) {
                size_t dot = filename.rfind('.');
                std::string base = (dot != std::string::npos) ? filename.substr(0, dot) : filename;
                size_t sep = base.find_last_of("/\\");
                std::string baseName = (sep != std::string::npos) ? base.substr(sep + 1) : base;
                std::string parentDir = (sep != std::string::npos) ? base.substr(0, sep) : ".";
                std::string altPath = parentDir + "/" + baseName + ".fc";
                if (access(altPath.c_str(), 0) == 0) fcFile = altPath;
            }
            std::string fullCode = read_file(fcFile);
            if (fullCode.empty()) return 1;
            try {
                std::vector<uint8_t> fcData(fullCode.begin(), fullCode.end());
                xor_crypt(fcData, FC_XOR_KEY);
                CompiledProgram prog = CompiledProgram::deserialize(fcData);
                disassembleProgram(prog, std::cout);
            } catch (const std::exception& e) {
                ErrorReporter::reportFromException("DisassembleError", e.what());
                return 1;
            }
            return 0;
        }
        else if (in == "-p" || in == "--package") {
            if (i + 2 >= argc) {
                ErrorReporter::reportSimple("ArgumentError", "'-p' requires: output.far input1.fc [input2.fc ...]");
                return 1;
            }
            std::string outputPath = argv[i + 1];
            std::vector<std::string> inputs;
            for (int j = i + 2; j < argc; j++) {
                if (argv[j][0] == '-') break;
                inputs.push_back(argv[j]);
            }
            i += 1 + static_cast<int>(inputs.size());
            if (!farPackage(outputPath, inputs)) return 1;
            return 0;
        }
        else if (in == "-u" || in == "--unpack") {
            if (i + 2 >= argc) {
                ErrorReporter::reportSimple("ArgumentError", "'-u' requires: archive.far [output_dir]");
                return 1;
            }
            std::string archivePath = argv[i + 1];
            std::string outputDir = (i + 2 < argc && argv[i + 2][0] != '-') ? argv[i + 2] : "output";
            if (argv[i + 2][0] != '-') i++;
            i++;
            if (!farUnpack(archivePath, outputDir)) return 1;
            return 0;
        }
        else if (in == "-far" || in == "--far-run") {
            if (i + 1 >= argc) {
                ErrorReporter::reportSimple("ArgumentError", "'-far' requires a filename");
                return 1;
            }
            filename = argv[i + 1];
            i++;
            RegFunc();
            if (!farRun(filename)) return 1;
            if (ErrorReporter::hasError()) return 1;
            return 0;
        }
        else if (in == "-help" || in == "-h") {
            print_help();
            return 0;
        }
        else {
            ErrorReporter::reportSimple("ArgumentError", "unknown parameter '" + in + "'");
            return 1;
        }
    }
    
    if (isOutInfo)
        std::cout << "Command parsing complete." << std::endl;
    if (v1 == 2) {
        fz(encrypt_key);
    }
    else if (v1 == 3) {
        fz1(encrypt_key);
    }

    if (!filename.empty()) {
        std::string fullCode = read_file(filename);
        ErrorReporter::setSource(filename, fullCode);
        RegFunc(); 
        interpreter.parseCode(fullCode, filename);
        if (!interpreter.parse_failed) {
            interpreter.runMainFunc();
        }
        if (interpreter.parse_failed || ErrorReporter::hasError()) return 1;
    }

    return 0;
}
