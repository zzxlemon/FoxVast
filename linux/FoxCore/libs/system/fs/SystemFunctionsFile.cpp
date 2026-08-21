#include "SystemFunctionsFile.h"
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <string>

struct FileHandle {
    std::string name;
    std::fstream stream;
    std::string mode;   // "read", "write", "append"
    bool isOpen;
};

static std::unordered_map<int, FileHandle> s_files;
static int s_nextHandle = 1;

static FileHandle* getHandle(int h) {
    auto it = s_files.find(h);
    return (it != s_files.end() && it->second.isOpen) ? &it->second : nullptr;
}

Value File::FileOpen(const std::vector<Value>& args) {
    if (args.size() != 2)
        throw std::runtime_error("FileOpen: need 2 args (filename, mode)");
    std::string name = args[0].asString();
    std::string mode = args[1].asString();

    for (auto& p : s_files) {
        if (p.second.name == name && p.second.isOpen)
            throw std::runtime_error("FileOpen: file already opened, handle=" + std::to_string(p.first));
    }

    std::ios::openmode flags;
    if (mode == "read") flags = std::ios::in | std::ios::binary;
    else if (mode == "write") flags = std::ios::out | std::ios::trunc | std::ios::binary;
    else if (mode == "append") flags = std::ios::out | std::ios::app | std::ios::binary;
    else throw std::runtime_error("FileOpen: mode must be read/write/append");

    std::fstream fs;
    fs.open(name, flags);
    if (!fs.is_open())
        throw std::runtime_error("FileOpen: cannot open file " + name);

    int handle = s_nextHandle++;
    s_files[handle] = { name, std::move(fs), mode, true };
    return Value(handle);
}

Value File::FileRead(const std::vector<Value>& args) {
    if (args.size() != 1)
        throw std::runtime_error("FileRead: need 1 arg (handle)");
    int h = args[0].asInt();
    FileHandle* f = getHandle(h);
    if (!f) throw std::runtime_error("FileRead: invalid handle");
    if (f->mode != "read")
        throw std::runtime_error("FileRead: file not opened in read mode");

    f->stream.clear();
    f->stream.seekg(0, std::ios::beg);
    std::ostringstream oss;
    oss << f->stream.rdbuf();

    if (!f->stream.good()) {
        throw std::runtime_error("FileRead: read failed (disk error or file corrupted)");
    }
    return Value(oss.str());
}

Value File::FileWrite(const std::vector<Value>& args) {
    if (args.size() < 2 || args.size() > 3)
        throw std::runtime_error("FileWrite: need 2 or 3 args (handle, content, newline(true/false))");
    int h = args[0].asInt();
    FileHandle* f = getHandle(h);
    if (!f) throw std::runtime_error("FileWrite: invalid handle");
    if (f->mode == "read")
        throw std::runtime_error("FileWrite: file opened in read mode");

    std::string content = args[1].asString();
    bool newline = (args.size() == 3 && args[2].asBool());

    f->stream << content;
    if (newline) f->stream << std::endl;
    f->stream.flush();

    if (!f->stream.good()) {
        throw std::runtime_error("FileWrite: write failed (disk full or permission denied)");
    }
    return Value();
}

Value File::FileClose(const std::vector<Value>& args) {
    if (args.size() != 1)
        throw std::runtime_error("FileClose: need 1 arg (handle)");
    int h = args[0].asInt();
    auto it = s_files.find(h);
    if (it == s_files.end())
        throw std::runtime_error("FileClose: invalid handle");
    it->second.stream.close();
    s_files.erase(it);
    return Value();
}

Value File::FileDelete(const std::vector<Value>& args) {
    if (args.size() != 1)
        throw std::runtime_error("FileDelete: need 1 arg (filename)");
    std::string name = args[0].asString();

    for (auto& p : s_files) {
        if (p.second.name == name && p.second.isOpen) {
            throw std::runtime_error("FileDelete: file is still open, handle=" + std::to_string(p.first) + ". Close it first.");
        }
    }

    if (std::remove(name.c_str()) != 0)
        throw std::runtime_error("FileDelete: cannot delete file " + name);
    return Value();
}
