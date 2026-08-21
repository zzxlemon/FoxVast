#include "./SystemFunctionsTime.h"
#include <ctime>
#include <string>
#include <sstream>

// FoxVast time library (fox.time.dll).
// The VM stores ints as int32, so only whole seconds are exposed; the epoch
// fits comfortably until 2038.

static bool toTmLocal(time_t t, struct tm& out) {
#ifdef _WIN32
    if (localtime_s(&out, &t) != 0) return false;
#else
    if (localtime_r(&t, &out) == nullptr) return false;
#endif
    return true;
}

// time.now() -> int (epoch seconds since 1970-01-01 UTC)
Value SystemFunctionsTime::now(const std::vector<Value>& args) {
    if (args.size() != 0) {
        throw std::runtime_error("time.now function argument error: expects 0 arguments, got "
            + std::to_string(args.size()));
    }
    return Value(static_cast<int>(std::time(nullptr)));
}

// time.format([fmt]) -> string, current local time formatted by strftime.
// Default fmt is "%Y-%m-%d %H:%M:%S". A trailing format token may be embedded
// in the string as well; strftime specifiers like %Y %m %d %H %M %S pass
// through unchanged.
Value SystemFunctionsTime::format(const std::vector<Value>& args) {
    if (args.size() > 1) {
        throw std::runtime_error("time.format function argument error: expects 0 or 1 arguments, got "
            + std::to_string(args.size()));
    }
    std::string fmt = "%Y-%m-%d %H:%M:%S";
    if (args.size() == 1) {
        if (args[0].getType() != Value::Type::String) {
            throw std::runtime_error("time.format function argument error: fmt must be a string");
        }
        fmt = args[0].asString();
    }
    time_t t = std::time(nullptr);
    struct tm stm;
    if (!toTmLocal(t, stm)) {
        throw std::runtime_error("time.format: failed to read system clock");
    }
    char buf[256];
    size_t n = std::strftime(buf, sizeof(buf), fmt.c_str(), &stm);
    if (n == 0) {
        throw std::runtime_error("time.format: strftime failed (format too long or invalid)");
    }
    return Value(std::string(buf, n));
}

// time.field(name) -> int, one component of the current local time.
// name: "year" "month" (1-12) "day" "hour" "minute" "second" "weekday" (0-6,
// Sunday=0) "dayofyear" (0-365) "yearday" alias of dayofyear.
Value SystemFunctionsTime::field(const std::vector<Value>& args) {
    if (args.size() != 1) {
        throw std::runtime_error("time.field function argument error: expects 1 argument, got "
            + std::to_string(args.size()));
    }
    if (args[0].getType() != Value::Type::String) {
        throw std::runtime_error("time.field function argument error: name must be a string");
    }
    time_t t = std::time(nullptr);
    struct tm stm;
    if (!toTmLocal(t, stm)) {
        throw std::runtime_error("time.field: failed to read system clock");
    }
    const std::string& name = args[0].asString();
    if (name == "year") return Value(stm.tm_year + 1900);
    if (name == "month") return Value(stm.tm_mon + 1);
    if (name == "day") return Value(stm.tm_mday);
    if (name == "hour") return Value(stm.tm_hour);
    if (name == "minute") return Value(stm.tm_min);
    if (name == "second") return Value(stm.tm_sec);
    if (name == "weekday") return Value(stm.tm_wday);
    if (name == "dayofyear" || name == "yearday") return Value(stm.tm_yday);
    throw std::runtime_error("time.field function argument error: unknown name '" + name
        + "' (expected year/month/day/hour/minute/second/weekday/dayofyear)");
}