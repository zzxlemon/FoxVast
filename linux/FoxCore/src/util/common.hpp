#pragma once
#include <string>

extern bool isOutInfo;
extern std::string fox_version;
extern std::string fox_vm_version;

const int INT_BYTE_SIZE = 4;     // int
const int DOUBLE_BYTE_SIZE = 8;  // double
const int STRING_BYTE_SIZE = 8;  // String
const int VOID_BYTE_SIZE = 0;    // void

// XOR encryption key
const uint8_t FC_XOR_KEY = 0x2A;
