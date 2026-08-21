#include "utils.hpp"
#include "../frontend/token.hpp"
#include "common.hpp"
#include "../frontend/lexer.hpp"   
#include "error_reporter.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

std::string read_file(const std::string& filename) {
    std::ifstream inFile(filename, std::ios::binary | std::ios::ate);
    if (!inFile.is_open()) {
        ErrorReporter::reportSimple("FileError", "Cannot open file: " + filename);
        std::exit(1);
    }
    
    // pre-allocate memory, read all at once
    std::streamsize size = inFile.tellg();
    inFile.seekg(0, std::ios::beg);
    
    std::string fullCode(size, '\0');
    if (!inFile.read(&fullCode[0], size)) {
        ErrorReporter::reportSimple("FileError", "Failed to read file: " + filename);
        std::exit(1);
    }
    inFile.close();

    if (isOutInfo) {
        std::cout << "Successfully read file: " << filename << " size: " << fullCode.size() << " bytes" << std::endl;
    }
    return fullCode;
}

void xor_crypt(std::vector<uint8_t>& data, uint8_t key) {
    for (size_t i = 0; i < data.size(); i++) {
        data[i] ^= key;
    }
}

// single-char key encrypt/decrypt
std::string xor_encrypt_decrypt(const std::string& input, char key) {
    std::string output = input;
    for (size_t i = 0; i < input.size(); ++i) {
        output[i] = input[i] ^ key; 
    }
    return output;
}

// string key encrypt/decrypt
std::string xor_encrypt_decrypt_str_key(const std::string& input, const std::string& key) {
    std::string output = input;
    size_t key_len = key.size();
    if (key_len == 0) return input; 

    for (size_t i = 0; i < input.size(); ++i) {
        output[i] = input[i] ^ key[i % key_len]; 
    }
    return output;
}

// encrypt file with string key
void encrypt_file_with_key(const std::string& src_path, const std::string& dst_path, const std::string& key) {
    std::ifstream src_file(src_path, std::ios::binary);
    if (!src_file) {
        ErrorReporter::reportSimple("FileError", "Cannot open source file: " + src_path);
        return;
    }
    std::string content((std::istreambuf_iterator<char>(src_file)), std::istreambuf_iterator<char>());
    src_file.close();

    std::string encrypted = xor_encrypt_decrypt_str_key(content, key);

    std::ofstream dst_file(dst_path, std::ios::binary);
    if (!dst_file) {
        ErrorReporter::reportSimple("FileError", "Cannot create encrypted file: " + dst_path);
        return;
    }
    // use .data() to avoid truncation at 0x00
    dst_file.write(encrypted.data(), encrypted.size());
    dst_file.close();
    std::cout << "Encrypted file: " << dst_path << " (key: " << key << ")" << std::endl;
}

// decrypt file with string key
std::string decrypt_file_with_key(const std::string& fz_path, const std::string& key) {
    std::ifstream fz_file(fz_path, std::ios::binary);
    if (!fz_file) {
        ErrorReporter::reportSimple("FileError", "Cannot open encrypted file: " + fz_path);
        return "";
    }
    std::string encrypted((std::istreambuf_iterator<char>(fz_file)), std::istreambuf_iterator<char>());
    fz_file.close();
    return xor_encrypt_decrypt_str_key(encrypted, key);
}

// encrypt file with single char key
void encrypt_file(const std::string& src_path, const std::string& dst_path, char key) {
    std::ifstream src_file(src_path, std::ios::binary);
    if (!src_file) {
        ErrorReporter::reportSimple("FileError", "Cannot open source file: " + src_path);
        return;
    }
    std::string content((std::istreambuf_iterator<char>(src_file)), std::istreambuf_iterator<char>());
    src_file.close();
    std::string encrypted = xor_encrypt_decrypt(content, key);
    std::ofstream dst_file(dst_path, std::ios::binary);
    if (!dst_file) {
        ErrorReporter::reportSimple("FileError", "Cannot create encrypted file: " + dst_path);
        return;
    }
    // use .data() to avoid truncation bug
    dst_file.write(encrypted.data(), encrypted.size());
    dst_file.close();

    std::cout << "Encrypted file: " << dst_path << std::endl;
}

// decrypt file with single char key
std::string decrypt_file(const std::string& fz_path, char key) {
    std::ifstream fz_file(fz_path, std::ios::binary);
    if (!fz_file) {
        ErrorReporter::reportSimple("FileError", "Cannot open encrypted file: " + fz_path);
        return "";
    }
    std::string encrypted((std::istreambuf_iterator<char>(fz_file)), std::istreambuf_iterator<char>());
    fz_file.close();
    return xor_encrypt_decrypt(encrypted, key);
}
