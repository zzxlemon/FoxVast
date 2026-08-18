#include "SystemFunctionsMath.h"
#include <cmath>
/**
* Note: The tan expression in this math library will explode near ¦Ð/2.
*/
static double toDouble(const Value& v) {
    return (v.getType() == Value::Type::Int) ? static_cast<double>(v.asInt()) : v.asDouble();
}

Value Math::sin(const std::vector<Value>& args) {
	if (args.size() != 1) {
		throw std::runtime_error("math: need 1 argument (x:number)");
	}
	const Value v = args[0];
	if (v.getType() != Value::Type::Int && v.getType() != Value::Type::Double) {
		throw std::runtime_error("math: The parameter must be a number (int or double)");
	}
	const double sin_res = std::sin(toDouble(v));
	return Value(sin_res);
}

Value Math::cos(const std::vector<Value>& args) {
	if (args.size() != 1) {
		throw std::runtime_error("math: need 1 argument (x:number)");
	}
	const Value v = args[0];
	if (v.getType() != Value::Type::Int && v.getType() != Value::Type::Double) {
		throw std::runtime_error("math: The parameter must be a number (int or double)");
	}
	const double cos_res = std::cos(toDouble(v));
	return Value(cos_res);
}

Value Math::tan(const std::vector<Value>& args) {
	if (args.size() != 1) {
		throw std::runtime_error("math: need 1 argument (x:number)");
	}
	const Value v = args[0];
	if (v.getType() != Value::Type::Int && v.getType() != Value::Type::Double) {
		throw std::runtime_error("math: The parameter must be a number (int or double)");
	}
	const double tan_res = std::tan(toDouble(v));
	return Value(tan_res);
}
