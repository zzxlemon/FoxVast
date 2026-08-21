#include "./SystemFunctionsRandom.h"
#include <random>
#include <string>
#include <cmath>
#include <sstream>
#include <climits>   // for INT_MIN, INT_MAX
#include <chrono>

std::mt19937& Random::rng() {
    static std::mt19937 gen(static_cast<unsigned int>(
        std::chrono::system_clock::now().time_since_epoch().count()));
    return gen;
}

static int toIntSafe(const Value& v, const char* paramName) {
    if (v.getType() == Value::Type::Int) return v.asInt();
    if (v.getType() == Value::Type::Double) {
        double d = v.asDouble();
        if (d < static_cast<double>(INT_MIN) || d > static_cast<double>(INT_MAX)) {
            std::ostringstream oss;
            oss << "random function argument error: " << paramName << " value " << d << " exceeds int range";
            throw std::runtime_error(oss.str());
        }
        int i = static_cast<int>(d);
        if (std::fabs(d - i) > 1e-9) {
            std::ostringstream oss;
            oss << "random function argument error: " << paramName << " requires integer, but got non-integer double " << d;
            throw std::runtime_error(oss.str());
        }
        return i;
    }
    std::ostringstream oss;
    oss << "random function argument error: " << paramName << " must be a number (int or double)";
    throw std::runtime_error(oss.str());
}

static double toDoubleSafe(const Value& v, const char* paramName) {
    if (v.getType() == Value::Type::Int) return static_cast<double>(v.asInt());
    if (v.getType() == Value::Type::Double) return v.asDouble();
    std::ostringstream oss;
    oss << "random function argument error: " << paramName << " must be a number (int or double)";
    throw std::runtime_error(oss.str());
}

Value Random::RandomStart(const std::vector<Value>& args) {
    if (args.size() != 3) {
        throw std::runtime_error("random function requires 3 arguments: random('int', min, max) or random(\"double\", min, max)");
    }

    const Value& random_type_v = args[0];
    const Value& random_min = args[1];  
    const Value& random_max = args[2];

    if (random_type_v.asString() == "int") {
        int min = toIntSafe(random_min, "min");
        int max = toIntSafe(random_max, "max");
        return RandomInt(min, max);
    }
    else if (random_type_v.asString() == "double") {
        double min = toDoubleSafe(random_min, "min");
        double max = toDoubleSafe(random_max, "max");
        return RandomDouble(min, max);
    }
    else {
        throw std::runtime_error("random function's first argument must be string 'int' or 'double'");
    }
}

Value Random::RandomInt(const int min, const int max) {
    if (min > max) {
        std::ostringstream oss;
        oss << "random function argument error: RandomInt requires min <= max, actual min=" << min << ", max=" << max;
        throw std::runtime_error(oss.str());
    }
    return Value(std::uniform_int_distribution<int>(min, max)(rng()));
}

Value Random::RandomDouble(const double min, const double max) {
    if (min > max) {
        std::ostringstream oss;
        oss << "random function argument error: RandomDouble requires min <= max, actual min=" << min << ", max=" << max;
        throw std::runtime_error(oss.str());
    }
    return Value(std::uniform_real_distribution<double>(min, max)(rng()));
}