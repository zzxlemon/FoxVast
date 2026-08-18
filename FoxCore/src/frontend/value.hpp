#pragma once
#include <string>
#include <stdexcept>
#include <vector>
#include <unordered_map>
#include <memory>

class Value {
public:
    enum class Type { Int, String, Double, Void, Array, Bytes, Dict, Object, Return, Unknown };

    Value();
    Value(int v);
    Value(double v);
    Value(const std::string& v);
    Value(const char* v);
    Value(const std::vector<Value>& v);
    Value(const std::vector<uint8_t>& bytes);
    Value(const std::unordered_map<std::string, std::shared_ptr<Value>>& dict);

    // Marker returned by a bare "ret" (void return). Executors treat it as
    // "exit the current function" while still reporting no value. Never
    // serialized; exists only at runtime.
    static Value makeReturnMarker() {
        Value v;
        v.type = Type::Return;
        return v;
    }

    Type getType() const;

    std::vector<Value>& asArrayRef();

    int asInt() const;
    double asDouble() const;
    const std::string& asString() const;
    const std::vector<Value>& asArray() const;
    const std::vector<uint8_t>& asBytes() const;
    std::vector<uint8_t>& asBytesRef();
    const std::unordered_map<std::string, std::shared_ptr<Value>>& asDict() const;
    std::unordered_map<std::string, std::shared_ptr<Value>>& asDictRef();

    // Object (class/struct instance): members are held in a shared map so that
    // copying an object Value keeps reference semantics (this binding works).
    static Value makeObject(const std::string& className);
    const std::string& asObjectClass() const;
    const std::unordered_map<std::string, std::shared_ptr<Value>>& asObjectDict() const;
    std::unordered_map<std::string, std::shared_ptr<Value>>& asObjectDictRef();

    int getByteSize() const;

    std::string toString() const;

    bool asBool() const {
        switch (type) {
        case Type::Int: return intVal != 0;
        case Type::Double: return doubleVal != 0.0;
        case Type::Array: return !arrVal.empty();
        case Type::Bytes: return !bytesVal.empty();
        case Type::Dict: return !dictVal.empty();
        case Type::Object: return true;
        default: throw std::runtime_error("Only int/double/array/bytes/dict/types supported as condition");
        }
    }

private:
    Type type;
    int intVal;
    double doubleVal;
    std::string strVal;
    std::vector<Value> arrVal;
    std::vector<uint8_t> bytesVal;
    std::unordered_map<std::string, std::shared_ptr<Value>> dictVal;
    std::string objectClassName = "";
    std::shared_ptr<std::unordered_map<std::string, std::shared_ptr<Value>>> objDictVal;
};

bool valuesEqual(const Value& a, const Value& b);
