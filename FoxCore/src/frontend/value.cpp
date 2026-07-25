#include "value.hpp"
#include "../util/common.hpp"

Value::Value() : type(Type::Void), intVal(0), doubleVal(0.0), strVal(""), arrVal(), bytesVal() {}
Value::Value(int v) : type(Type::Int), intVal(v), doubleVal(0.0), strVal(""), arrVal(), bytesVal() {}
Value::Value(double v) : type(Type::Double), intVal(0), doubleVal(v), strVal(""), arrVal(), bytesVal() {}
Value::Value(const std::string& v) : type(Type::String), intVal(0), doubleVal(0.0), strVal(v), arrVal(), bytesVal() {}
Value::Value(const char* v) : type(Type::String), intVal(0), doubleVal(0.0), strVal(v), arrVal(), bytesVal() {}
Value::Value(const std::vector<Value>& v) : type(Type::Array), intVal(0), doubleVal(0.0), strVal(""), arrVal(v), bytesVal() {}
Value::Value(const std::vector<uint8_t>& bytes) : type(Type::Bytes), intVal(0), doubleVal(0.0), strVal(""), arrVal(), bytesVal(bytes) {}

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

int Value::getByteSize() const {
    switch (type) {
    case Type::Int: return INT_BYTE_SIZE;
    case Type::Double: return DOUBLE_BYTE_SIZE;
    case Type::String: return STRING_BYTE_SIZE;
    case Type::Array: return STRING_BYTE_SIZE;
    case Type::Bytes: return static_cast<int>(bytesVal.size());
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
    case Type::Void: return "";
    default: return "<?>";
    }
}
