#pragma once

#include "../../../../src/interpreter/interpreter.hpp"
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <WS2tcpip.h>
#endif
#ifdef __linux__
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#endif

class Socket {
public:
    Value socket_create(const std::vector<Value>& args);
    Value socket_connect(const std::vector<Value>& args);
    Value socket_send(const std::vector<Value>& args);
    Value socket_recv(const std::vector<Value>& args);
    Value socket_close(const std::vector<Value>& args);
};
