#pragma once
#include <string>
#include <stdexcept>
#include <vector>
#include <unordered_map>
#include <memory>

class Value {
public:
    enum class Type { Int, String, Double, Void, Array, Bytes, Dict, Unknown };

    Value();
    Value(int v);
    Value(double v);
    Value(const std::string& v);
    Value(const char* v);
    Value(const std::vector<Value>& v);
    Value(const std::vector<uint8_t>& bytes);
    Value(const std::unordered_map<std::string, std::shared_ptr<Value>>& dict);

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
    
    int getByteSize() const;

    std::string toString() const;

    bool asBool() const {
        switch (type) {
        case Type::Int: return intVal != 0;
        case Type::Double: return doubleVal != 0.0;
        case Type::Array: return !arrVal.empty();
        case Type::Bytes: return !bytesVal.empty();
        case Type::Dict: return !dictVal.empty();
        default: throw std::runtime_error("Only int/double/array/bytes/dict types supported as condition");
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
};

bool valuesEqual(const Value& a, const Value& b);
