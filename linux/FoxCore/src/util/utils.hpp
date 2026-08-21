#pragma once 
#include <string> 
#include <vector> 

// read file 
std::string read_file(const std::string& filename); 

// XOR encrypt/decrypt byte data (in-place) 
void xor_crypt(std::vector<uint8_t>& data, uint8_t key); 

// single-char encrypt/decrypt 
std::string xor_encrypt_decrypt(const std::string& input, char key); 
std::string xor_encrypt_decrypt_str_key(const std::string& input, const std::string& key); 

// file encrypt/decrypt with string key 
void encrypt_file_with_key(const std::string& src_path, const std::string& dst_path, const std::string& key); 
std::string decrypt_file_with_key(const std::string& fz_path, const std::string& key); 

// single-char file encrypt/decrypt 
void encrypt_file(const std::string& src_path, const std::string& dst_path, char key); 
std::string decrypt_file(const std::string& fz_path, char key);
