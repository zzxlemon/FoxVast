#include "./SystemFunctionsSocket.h"

// Socket handle management (similar to File library's pattern)
#ifdef _WIN32
using sock_t = SOCKET;
static constexpr sock_t INVALID_SOCK = INVALID_SOCKET;
#else
using sock_t = int;
static constexpr sock_t INVALID_SOCK = -1;
#endif

struct SocketHandle {
    sock_t fd;
    std::string remoteHost;
    int remotePort;
    bool isOpen;
};

static std::unordered_map<int, SocketHandle> s_sockets;
static int s_nextHandle = 1;

static SocketHandle* getHandle(int h) {
    auto it = s_sockets.find(h);
    if (it != s_sockets.end() && it->second.isOpen) return &it->second;
    return nullptr;
}

// Global Winsock init/cleanup (Windows only)
#ifdef _WIN32
static WSADATA s_wsaData;
static int s_wsaRefCount = 0;

static bool ensureWSA() {
    if (s_wsaRefCount == 0) {
        int rc = WSAStartup(MAKEWORD(2, 2), &s_wsaData);
        if (rc != 0) return false;
    }
    s_wsaRefCount++;
    return true;
}

static void releaseWSA() {
    if (s_wsaRefCount > 0) {
        s_wsaRefCount--;
        if (s_wsaRefCount == 0) WSACleanup();
    }
}
#else
static bool ensureWSA() { return true; }
static void releaseWSA() {}
#endif

// Convert sockaddr_in to readable ip:port string
static std::string formatAddr(const struct sockaddr_in& addr) {
#ifdef _WIN32
    return std::string(inet_ntoa(addr.sin_addr)) + ":" + std::to_string(ntohs(addr.sin_port));
#else
    char buf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, buf, sizeof(buf));
    return std::string(buf) + ":" + std::to_string(ntohs(addr.sin_port));
#endif
}

Value Socket::socket_create(const std::vector<Value>& args) {
    if (args.size() != 0) {
        throw std::runtime_error("socket.create: no arguments expected, returns a socket handle");
    }
    if (!ensureWSA()) {
        throw std::runtime_error("socket.create: WSAStartup failed");
    }
    sock_t fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == INVALID_SOCK) {
#ifdef _WIN32
        int err = WSAGetLastError();
        releaseWSA();
        throw std::runtime_error("socket.create: socket() failed, error=" + std::to_string(err));
#else
        releaseWSA();
        throw std::runtime_error("socket.create: socket() failed, errno=" + std::to_string(errno));
#endif
    }
    int handle = s_nextHandle++;
    s_sockets[handle] = { fd, "", 0, true };
    return Value(handle);
}

Value Socket::socket_connect(const std::vector<Value>& args) {
    if (args.size() != 3) {
        throw std::runtime_error("socket.connect: need 3 args (handle, ip, port)");
    }
    int h = args[0].asInt();
    SocketHandle* sh = getHandle(h);
    if (!sh) throw std::runtime_error("socket.connect: invalid handle");

    std::string ip = args[1].asString();
    int port = args[2].asInt();
    if (port <= 0 || port > 65535) {
        throw std::runtime_error("socket.connect: port must be 1-65535");
    }

    struct sockaddr_in servAddr;
    std::memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(static_cast<uint16_t>(port));
#ifdef _WIN32
    servAddr.sin_addr.s_addr = inet_addr(ip.c_str());
    if (servAddr.sin_addr.s_addr == INADDR_NONE) {
        throw std::runtime_error("socket.connect: invalid IP address: " + ip);
    }
#else
    if (inet_pton(AF_INET, ip.c_str(), &servAddr.sin_addr) <= 0) {
        throw std::runtime_error("socket.connect: invalid IP address: " + ip);
    }
#endif

    int rc = connect(sh->fd, reinterpret_cast<struct sockaddr*>(&servAddr), sizeof(servAddr));
    if (rc < 0) {
#ifdef _WIN32
        int err = WSAGetLastError();
        throw std::runtime_error("socket.connect: connect() failed, error=" + std::to_string(err));
#else
        throw std::runtime_error("socket.connect: connect() failed, errno=" + std::to_string(errno));
#endif
    }
    sh->remoteHost = ip;
    sh->remotePort = port;
    return Value(true);
}

Value Socket::socket_send(const std::vector<Value>& args) {
    if (args.size() != 2) {
        throw std::runtime_error("socket.send: need 2 args (handle, data)");
    }
    int h = args[0].asInt();
    SocketHandle* sh = getHandle(h);
    if (!sh) throw std::runtime_error("socket.send: invalid handle");

    std::string data = args[1].asString();
#ifdef _WIN32
    int sent = send(sh->fd, data.c_str(), static_cast<int>(data.size()), 0);
    if (sent == SOCKET_ERROR) {
        int err = WSAGetLastError();
        throw std::runtime_error("socket.send: send() failed, error=" + std::to_string(err));
    }
#else
    ssize_t sent = send(sh->fd, data.c_str(), data.size(), 0);
    if (sent < 0) {
        throw std::runtime_error("socket.send: send() failed, errno=" + std::to_string(errno));
    }
#endif
    return Value(static_cast<int>(sent));
}

Value Socket::socket_recv(const std::vector<Value>& args) {
    if (args.size() != 2) {
        throw std::runtime_error("socket.recv: need 2 args (handle, buffer_size)");
    }
    int h = args[0].asInt();
    SocketHandle* sh = getHandle(h);
    if (!sh) throw std::runtime_error("socket.recv: invalid handle");

    int bufSize = args[1].asInt();
    if (bufSize <= 0) {
        throw std::runtime_error("socket.recv: buffer_size must be positive");
    }

    std::vector<char> buf(static_cast<size_t>(bufSize));
#ifdef _WIN32
    int rc = recv(sh->fd, buf.data(), bufSize, 0);
    if (rc == SOCKET_ERROR) {
        int err = WSAGetLastError();
        throw std::runtime_error("socket.recv: recv() failed, error=" + std::to_string(err));
    }
#else
    ssize_t rc = recv(sh->fd, buf.data(), static_cast<size_t>(bufSize), 0);
    if (rc < 0) {
        throw std::runtime_error("socket.recv: recv() failed, errno=" + std::to_string(errno));
    }
#endif
    return Value(std::string(buf.data(), static_cast<size_t>(rc)));
}

Value Socket::socket_close(const std::vector<Value>& args) {
    if (args.size() != 1) {
        throw std::runtime_error("socket.close: need 1 arg (handle)");
    }
    int h = args[0].asInt();
    auto it = s_sockets.find(h);
    if (it == s_sockets.end()) {
        throw std::runtime_error("socket.close: invalid handle");
    }
    SocketHandle& sh = it->second;
    if (sh.isOpen) {
#ifdef _WIN32
        closesocket(sh.fd);
#else
        close(sh.fd);
#endif
        sh.isOpen = false;
    }
    s_sockets.erase(it);
    releaseWSA();
    return Value();
}
