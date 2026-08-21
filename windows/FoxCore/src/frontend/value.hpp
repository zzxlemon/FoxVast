#pragma once
#include <string>
#include <stdexcept>
#include <vector>
#include <unordered_map>
#include "gc.hpp"

class Value {
public:
    enum class Type { Int, String, Double, Void, Array, Bytes, Dict, Object, Coroutine, Return, Unknown };

    Value();
    Value(int v);
    Value(double v);
    Value(const std::string& v);
    Value(const char* v);
    Value(const std::vector<Value>& v);
    Value(const std::vector<uint8_t>& bytes);
    Value(const std::unordered_map<std::string, GcHandle>& dict);

    // Union-backed storage needs real copy/move/dtor semantics.
    Value(const Value& other);
    Value(Value&& other) noexcept;
    Value& operator=(const Value& other);
    Value& operator=(Value&& other) noexcept;
    ~Value();

    // Marker returned by a bare "ret" (void return). Executors treat it as
    // "exit the current function" while still reporting no value. Never
    // serialized; exists only at runtime.
    static Value makeReturnMarker() {
        Value v;
        v.type = Type::Return;
        return v;
    }

    // Coroutine handle: index into the VM's coroutine registry. Runtime-only
    // value; never serialized.
    static Value makeCoroutine(int coroId) {
        Value v;
        v.type = Type::Coroutine;
        v.coroId = coroId;
        return v;
    }
    int asCoroutineId() const { return coroId; }

    Type getType() const;

    std::vector<Value>& asArrayRef();

    int asInt() const;
    double asDouble() const;
    const std::string& asString() const;
    const std::vector<Value>& asArray() const;
    const std::vector<uint8_t>& asBytes() const;
    std::vector<uint8_t>& asBytesRef();
    const std::unordered_map<std::string, GcHandle>& asDict() const;
    std::unordered_map<std::string, GcHandle>& asDictRef();

    // Object (class/struct instance): members are held in a GC'd member table
    // so that copying an object Value keeps reference semantics (this binding
    // works). objDictVal is a handle to a Dict-typed heap slot.
    static Value makeObject(const std::string& className);
    const std::string& asObjectClass() const;
    GcHandle asObjectDictHandle() const;
    const std::unordered_map<std::string, GcHandle>& asObjectDict() const;
    std::unordered_map<std::string, GcHandle>& asObjectDictRef();

    // GC support: mark every heap handle reachable from this value.
    void traceGC() const;

    int getByteSize() const;

    std::string toString() const;

    bool asBool() const {
        switch (type) {
        case Type::Int: return st_.i != 0;
        case Type::Double: return st_.d != 0.0;
        case Type::Array: return !st_.arr.empty();
        case Type::Bytes: return !st_.bytes.empty();
        case Type::Dict: return !st_.dict->empty();
        case Type::Object: return true;
        default: throw std::runtime_error("Only int/double/array/bytes/dict/types supported as condition");
        }
    }

private:
    // Only one payload member is live at a time, selected by `type`.
    // dictVal lives on the heap (single pointer) so the whole Value stays
    // small (48 bytes); dict copies stay deep copies.
    union Storage {
        int i;
        double d;
        std::string s;
        std::vector<Value> arr;
        std::vector<uint8_t> bytes;
        std::unordered_map<std::string, GcHandle>* dict;
        std::string className;
        Storage() {}
        ~Storage() {}
    } st_;
    Type type;
    GcHandle objDictVal = kNoGcHandle; // Object: member-table handle
    int coroId = -1;                   // Coroutine: VM registry index

    void destroyStorage() noexcept;
    void copyStorage(const Value& other);
    void moveStorage(Value&& other) noexcept;
};

bool valuesEqual(const Value& a, const Value& b);
