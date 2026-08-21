#include "SystemFunctionUtil.h"
#include <algorithm>
#include <cctype>
#include <cmath>

Value Util::length(const std::vector<Value>& args) {
    if (args.size() != 1) {
            throw std::runtime_error("length() requires one argument");
    }
    const Value& arg = args[0];
    if (arg.getType() == Value::Type::Array) {
        return Value(static_cast<int>(arg.asArray().size()));
    }
    if (arg.getType() == Value::Type::String) {
        return Value(static_cast<int>(arg.asString().size()));
    }
    if (arg.getType() == Value::Type::Dict) {
        return Value(static_cast<int>(arg.asDict().size()));
    }
    throw std::runtime_error("length() supports array, string or dict type");
}

Value Util::arr_append(const std::vector<Value>& args) {
    if (args.size() != 2) throw std::runtime_error("arr_append: need 2 args (array, value)");
    if (args[0].getType() != Value::Type::Array) throw std::runtime_error("arr_append: first arg must be an array");
    std::vector<Value> result = args[0].asArray();
    result.push_back(args[1]);
    return Value(result);
}

Value Util::arr_pop(const std::vector<Value>& args) {
    if (args.size() != 1) throw std::runtime_error("arr_pop: need 1 arg (array)");
    if (args[0].getType() != Value::Type::Array) throw std::runtime_error("arr_pop: arg must be an array");
    std::vector<Value> result = args[0].asArray();
    if (result.empty()) throw std::runtime_error("arr_pop: cannot pop from empty array");
    result.pop_back();
    return Value(result);
}

Value Util::arr_contains(const std::vector<Value>& args) {
    if (args.size() != 2) throw std::runtime_error("arr_contains: need 2 args (array, value)");
    if (args[0].getType() != Value::Type::Array) throw std::runtime_error("arr_contains: first arg must be an array");
    for (const auto& elem : args[0].asArray()) {
        if (valuesEqual(elem, args[1])) return Value(1);
    }
    return Value(0);
}

Value Util::arr_slice(const std::vector<Value>& args) {
    if (args.size() != 3) throw std::runtime_error("arr_slice: need 3 args (array, start, end)");
    if (args[0].getType() != Value::Type::Array) throw std::runtime_error("arr_slice: first arg must be an array");
    if (args[1].getType() != Value::Type::Int || args[2].getType() != Value::Type::Int) {
        throw std::runtime_error("arr_slice: start and end must be int");
    }
    const auto& arr = args[0].asArray();
    int start = args[1].asInt();
    int end = args[2].asInt();
    if (start < 0) start = 0;
    if (end > static_cast<int>(arr.size())) end = static_cast<int>(arr.size());
    if (start >= end) return Value(std::vector<Value>{});
    std::vector<Value> result(arr.begin() + start, arr.begin() + end);
    return Value(result);
}

Value Util::arr_sort(const std::vector<Value>& args) {
    if (args.size() != 1) throw std::runtime_error("arr_sort: need 1 arg (array)");
    if (args[0].getType() != Value::Type::Array) throw std::runtime_error("arr_sort: arg must be an array");
    std::vector<Value> result = args[0].asArray();
    bool allNumbers = std::all_of(result.begin(), result.end(), [](const Value& v) {
        return v.getType() == Value::Type::Int || v.getType() == Value::Type::Double;
    });
    bool allStrings = std::all_of(result.begin(), result.end(), [](const Value& v) {
        return v.getType() == Value::Type::String;
    });
    if (allNumbers) {
        std::sort(result.begin(), result.end(), [](const Value& a, const Value& b) {
            double da = (a.getType() == Value::Type::Int) ? static_cast<double>(a.asInt()) : a.asDouble();
            double db = (b.getType() == Value::Type::Int) ? static_cast<double>(b.asInt()) : b.asDouble();
            return da < db;
        });
    } else if (allStrings) {
        std::sort(result.begin(), result.end(), [](const Value& a, const Value& b) {
            return a.asString() < b.asString();
        });
    } else {
        throw std::runtime_error("arr_sort: array must contain only numbers or only strings");
    }
    return Value(result);
}

Value Util::arr_length(const std::vector<Value>& args) {
    if (args.size() != 1) throw std::runtime_error("arr_length: need 1 arg (array)");
    if (args[0].getType() != Value::Type::Array) throw std::runtime_error("arr_length: arg must be an array");
    return Value(static_cast<int>(args[0].asArray().size()));
}

Value Util::str_contains(const std::vector<Value>& args) {
    if (args.size() != 2) throw std::runtime_error("str_contains: need 2 args (string, substring)");
    if (args[0].getType() != Value::Type::String || args[1].getType() != Value::Type::String) {
        throw std::runtime_error("str_contains: both args must be strings");
    }
    return Value(args[0].asString().find(args[1].asString()) != std::string::npos ? 1 : 0);
}

Value Util::str_replace(const std::vector<Value>& args) {
    if (args.size() != 3) throw std::runtime_error("str_replace: need 3 args (string, old, new)");
    for (const auto& a : args) {
        if (a.getType() != Value::Type::String) throw std::runtime_error("str_replace: all args must be strings");
    }
    std::string s = args[0].asString();
    const std::string& oldStr = args[1].asString();
    const std::string& newStr = args[2].asString();
    if (oldStr.empty()) return Value(s);
    size_t pos = 0;
    while ((pos = s.find(oldStr, pos)) != std::string::npos) {
        s.replace(pos, oldStr.size(), newStr);
        pos += newStr.size();
    }
    return Value(s);
}

Value Util::str_split(const std::vector<Value>& args) {
    if (args.size() != 2) throw std::runtime_error("str_split: need 2 args (string, delimiter)");
    if (args[0].getType() != Value::Type::String || args[1].getType() != Value::Type::String) {
        throw std::runtime_error("str_split: both args must be strings");
    }
    std::vector<Value> parts;
    const std::string& s = args[0].asString();
    const std::string& sep = args[1].asString();
    if (sep.empty()) {
        for (char c : s) parts.push_back(Value(std::string(1, c)));
        return Value(parts);
    }
    size_t start = 0;
    while (true) {
        size_t pos = s.find(sep, start);
        if (pos == std::string::npos) {
            parts.push_back(Value(s.substr(start)));
            break;
        }
        parts.push_back(Value(s.substr(start, pos - start)));
        start = pos + sep.size();
    }
    return Value(parts);
}

Value Util::str_trim(const std::vector<Value>& args) {
    if (args.size() != 1) throw std::runtime_error("str_trim: need 1 arg (string)");
    if (args[0].getType() != Value::Type::String) throw std::runtime_error("str_trim: arg must be a string");
    const std::string& s = args[0].asString();
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return Value(std::string());
    size_t end = s.find_last_not_of(" \t\r\n");
    return Value(s.substr(start, end - start + 1));
}

Value Util::str_lower(const std::vector<Value>& args) {
    if (args.size() != 1) throw std::runtime_error("str_lower: need 1 arg (string)");
    if (args[0].getType() != Value::Type::String) throw std::runtime_error("str_lower: arg must be a string");
    std::string s = args[0].asString();
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return Value(s);
}

Value Util::str_upper(const std::vector<Value>& args) {
    if (args.size() != 1) throw std::runtime_error("str_upper: need 1 arg (string)");
    if (args[0].getType() != Value::Type::String) throw std::runtime_error("str_upper: arg must be a string");
    std::string s = args[0].asString();
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return Value(s);
}

Value Util::str_substring(const std::vector<Value>& args) {
    if (args.size() != 3) throw std::runtime_error("str_substring: need 3 args (string, start, end)");
    if (args[0].getType() != Value::Type::String) throw std::runtime_error("str_substring: first arg must be a string");
    if (args[1].getType() != Value::Type::Int || args[2].getType() != Value::Type::Int) {
        throw std::runtime_error("str_substring: start and end must be int");
    }
    const std::string& s = args[0].asString();
    int start = args[1].asInt();
    int end = args[2].asInt();
    if (start < 0) start = 0;
    if (end > static_cast<int>(s.size())) end = static_cast<int>(s.size());
    if (start >= end) return Value(std::string());
    return Value(s.substr(static_cast<size_t>(start), static_cast<size_t>(end - start)));
}

Value Util::str_length(const std::vector<Value>& args) {
    if (args.size() != 1) throw std::runtime_error("str_length: need 1 arg (string)");
    if (args[0].getType() != Value::Type::String) throw std::runtime_error("str_length: arg must be a string");
    return Value(static_cast<int>(args[0].asString().size()));
}

Value Util::IntChangeString(const std::vector<Value>& args) {
    if (args.size() != 1) {
        throw std::runtime_error("IntChangeString: need 1 arg (int)");
    }
    const Value& a = args[0];
    if (a.getType() != Value::Type::Int) {
        throw std::runtime_error("IntChangeString: arg must be an int");
    }

    return Value(std::to_string(a.asInt()));
}

Value Util::StringChangeInt(const std::vector<Value>& args) {
    if (args.size() != 1){
        throw std::runtime_error("StringChangeInt: need 1 arg (string)");
    }
    const Value& a = args[0];
    if (a.getType() != Value::Type::String) {
        throw std::runtime_error("StringChangeInt: arg must be a string");
    }

    int result = std::stoi(a.asString());
    return Value(result);
}

Value Util::StringChangeDouble(const std::vector<Value>& args) {
    if (args.size() != 1) {
        throw std::runtime_error("StringChangeDouble: need 1 arg (string)");
    }
    const Value& a = args[0];
    if (a.getType() != Value::Type::String) {
        throw std::runtime_error("StringChangeDouble: arg must be a string");
    }

    double result = std::stod(a.asString());
    return Value(result);
}

Value Util::DoubleChangeString(const std::vector<Value>& args) {
    if (args.size() != 1) {
        throw std::runtime_error("DoubleChangeString: need 1 arg (double)");
    }
    const Value& a = args[0];
    if (a.getType() != Value::Type::Double) {
        throw std::runtime_error("DoubleChangeString: arg must be a double");
    }

    return Value(std::to_string(a.asDouble()));
}

Value Util::DoubleChangeInt(const std::vector<Value>& args) {
    if (args.size() != 1) {
        throw std::runtime_error("DoubleChangeInt: need 1 arg (double)");
    }
    const Value& a = args[0];
    if (a.getType() != Value::Type::Double) {
        throw std::runtime_error("DoubleChangeInt: arg must be a double");
    }

    return Value(static_cast<int>(a.asDouble()));
}

Value Util::IntChangeDouble(const std::vector<Value>& args) {
    if (args.size() != 1) {
        throw std::runtime_error("IntChangeDouble: need 1 arg (int)");
    }
    const Value& a = args[0];
    if (a.getType() != Value::Type::Int) {
        throw std::runtime_error("IntChangeDouble: arg must be an int");
    }

    return Value(static_cast<double>(a.asInt()));
}

Value Util::TypeOf(const std::vector<Value>& args) {
    if (args.size() != 1) {
        throw std::runtime_error("typeof() requires one argument");
    }
    const char* name = "unknown";
    switch (args[0].getType()) {
    case Value::Type::Int: name = "int"; break;
    case Value::Type::Double: name = "double"; break;
    case Value::Type::String: name = "string"; break;
    case Value::Type::Array: name = "array"; break;
    case Value::Type::Dict: name = "dict"; break;
    case Value::Type::Object: name = "object"; break;
    case Value::Type::Bytes: name = "bytes"; break;
    default: break;
    }
    return Value(std::string(name));
}
