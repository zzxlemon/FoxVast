#include "value.hpp"
#include "../util/common.hpp"

Value::Value() : type(Type::Void), intVal(0), doubleVal(0.0), strVal(""), arrVal(), bytesVal(), dictVal() {}
Value::Value(int v) : type(Type::Int), intVal(v), doubleVal(0.0), strVal(""), arrVal(), bytesVal(), dictVal() {}
Value::Value(double v) : type(Type::Double), intVal(0), doubleVal(v), strVal(""), arrVal(), bytesVal(), dictVal() {}
Value::Value(const std::string& v) : type(Type::String), intVal(0), doubleVal(0.0), strVal(v), arrVal(), bytesVal(), dictVal() {}
Value::Value(const char* v) : type(Type::String), intVal(0), doubleVal(0.0), strVal(v), arrVal(), bytesVal(), dictVal() {}
Value::Value(const std::vector<Value>& v) : type(Type::Array), intVal(0), doubleVal(0.0), strVal(""), arrVal(v), bytesVal(), dictVal() {}
Value::Value(const std::vector<uint8_t>& bytes) : type(Type::Bytes), intVal(0), doubleVal(0.0), strVal(""), arrVal(), bytesVal(bytes), dictVal() {}
Value::Value(const std::unordered_map<std::string, std::shared_ptr<Value>>& dict)
    : type(Type::Dict), intVal(0), doubleVal(0.0), strVal(""), arrVal(), bytesVal(), dictVal(dict) {}

std::vector<Value>& Value::asArrayRef() {
    if (type != Type::Array) throw std::runtime_error("Value is not an array type");
    return arrVal;
}

Value::Type Value::getType() const { return type; }

int Value::asInt() const {
    if (type != Type::Int) throw std::runtime_error("Value is not an integer type");
    return intVal;
}

double Value::asDouble() const {
    if (type != Type::Double) throw std::runtime_error("Value is not a double type");
    return doubleVal;
}

const std::string& Value::asString() const {
    if (type != Type::String) throw std::runtime_error("Value is not a string type");
    return strVal;
}

const std::vector<Value>& Value::asArray() const {
    if (type != Type::Array) throw std::runtime_error("Value is not an array type");
    return arrVal;
}

const std::vector<uint8_t>& Value::asBytes() const {
    if (type != Type::Bytes) throw std::runtime_error("Value is not a bytes type");
    return bytesVal;
}

std::vector<uint8_t>& Value::asBytesRef() {
    if (type != Type::Bytes) throw std::runtime_error("Value is not a bytes type");
    return bytesVal;
}

const std::unordered_map<std::string, std::shared_ptr<Value>>& Value::asDict() const {
    if (type != Type::Dict) throw std::runtime_error("Value is not a dict type");
    return dictVal;
}

std::unordered_map<std::string, std::shared_ptr<Value>>& Value::asDictRef() {
    if (type != Type::Dict) throw std::runtime_error("Value is not a dict type");
    return dictVal;
}

int Value::getByteSize() const {
    switch (type) {
    case Type::Int: return INT_BYTE_SIZE;
    case Type::Double: return DOUBLE_BYTE_SIZE;
    case Type::String: return STRING_BYTE_SIZE;
    case Type::Array: return STRING_BYTE_SIZE;
    case Type::Bytes: return static_cast<int>(bytesVal.size());
    case Type::Dict: return STRING_BYTE_SIZE;
    case Type::Void: return VOID_BYTE_SIZE;
    default: return 0;
    }
}

std::string Value::toString() const {
    switch (type) {
    case Type::Int: return std::to_string(intVal);
    case Type::Double: return std::to_string(doubleVal);
    case Type::String: return strVal;
    case Type::Array: return "[array]";
    case Type::Bytes: return "[bytes]";
    case Type::Dict: return "[dict]";
    case Type::Void: return "";
    default: return "<?>";
    }
}

bool valuesEqual(const Value& a, const Value& b) {
    if (a.getType() != b.getType()) return false;
    switch (a.getType()) {
    case Value::Type::Int: return a.asInt() == b.asInt();
    case Value::Type::Double: return a.asDouble() == b.asDouble();
    case Value::Type::String: return a.asString() == b.asString();
    case Value::Type::Array: {
        const auto& aa = a.asArray();
        const auto& bb = b.asArray();
        if (aa.size() != bb.size()) return false;
        for (size_t i = 0; i < aa.size(); i++) {
            if (!valuesEqual(aa[i], bb[i])) return false;
        }
        return true;
    }
    case Value::Type::Dict: {
        const auto& ad = a.asDict();
        const auto& bd = b.asDict();
        if (ad.size() != bd.size()) return false;
        for (const auto& [key, val] : ad) {
            auto it = bd.find(key);
            if (it == bd.end() || !valuesEqual(*val, *it->second)) return false;
        }
        return true;
    }
    case Value::Type::Bytes: return a.asBytes() == b.asBytes();
    default: return false;
    }
}
