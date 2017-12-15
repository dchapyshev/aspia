//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/value.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/strings/string_printf.h"
#include "base/logging.h"
#include "ui/system_info/value.h"

namespace aspia {

Value::Value(Type type, ValueType&& value, std::string_view unit)
    : value_(std::move(value)),
      type_(type),
      unit_(unit)
{
    // Nothing
}

Value::Value(Type type, ValueType&& value)
    : value_(std::move(value)),
      type_(type)
{
    // Nothing
}

Value::Value(Value&& other)
    : type_(other.type_),
      value_(std::move(other.value_)),
      unit_(std::move(other.unit_))
{
    // Nothing
}

Value& Value::operator=(Value&& other)
{
    type_ = other.type_;
    value_ = std::move(other.value_);
    unit_ = std::move(other.unit_);
    return *this;
}

// static
Value Value::EmptyString()
{
    return Value(Type::STRING, std::string());
}

// static
Value Value::String(const char* value)
{
    DCHECK(value != nullptr);
    return Value(Type::STRING, std::string(value));
}

// static
Value Value::String(const std::string& value)
{
    return Value(Type::STRING, value);
}

// static
Value Value::String(std::string&& value)
{
    return Value(Type::STRING, std::move(value));
}

// static
Value Value::FormattedString(const char* format, ...)
{
    va_list args;

    va_start(args, format);
    std::string out = StringPrintfV(format, args);
    va_end(args);

    return Value(Type::STRING, std::move(out));
}

// static
Value Value::Bool(bool value)
{
    return Value(Type::BOOL, value);
}

// static
Value Value::Number(uint32_t value, std::string_view unit)
{
    return Value(Type::UINT32, value, unit);
}

// static
Value Value::Number(uint32_t value)
{
    return Value(Type::UINT32, value);
}

// static
Value Value::Number(int32_t value, std::string_view unit)
{
    return Value(Type::INT32, value, unit);
}

// static
Value Value::Number(int32_t value)
{
    return Value(Type::INT32, value);
}

// static
Value Value::Number(uint64_t value, std::string_view unit)
{
    return Value(Type::UINT64, value, unit);
}

// static
Value Value::Number(uint64_t value)
{
    return Value(Type::UINT64, value);
}

// static
Value Value::Number(int64_t value, std::string_view unit)
{
    return Value(Type::INT64, value, unit);
}

// static
Value Value::Number(int64_t value)
{
    return Value(Type::INT64, value);
}

// static
Value Value::Number(double value, std::string_view unit)
{
    return Value(Type::DOUBLE, value, unit);
}

// static
Value Value::Number(double value)
{
    return Value(Type::DOUBLE, value);
}

Value::Type Value::type() const
{
    return type_;
}

std::string Value::ToString() const
{
    switch (type())
    {
        case Type::STRING:
            return std::get<std::string>(value_);

        case Type::BOOL:
            return ToBool() ? "Yes" : "No";

        case Type::UINT32:
            return std::to_string(ToUint32());

        case Type::INT32:
            return std::to_string(ToInt32());

        case Type::UINT64:
            return std::to_string(ToUint64());

        case Type::INT64:
            return std::to_string(ToInt64());

        case Type::DOUBLE:
            return std::to_string(ToDouble());

        default:
            DLOG(FATAL) << "Unhandled value type: " << static_cast<int>(type());
            return std::string();
    }
}

bool Value::ToBool() const
{
    DCHECK(type() == Type::BOOL);
    return std::get<bool>(value_);
}

uint32_t Value::ToUint32() const
{
    DCHECK(type() == Type::UINT32);
    return std::get<uint32_t>(value_);
}

int32_t Value::ToInt32() const
{
    DCHECK(type() == Type::INT32);
    return std::get<int32_t>(value_);
}

uint64_t Value::ToUint64() const
{
    DCHECK(type() == Type::UINT64);
    return std::get<uint64_t>(value_);
}

int64_t Value::ToInt64() const
{
    DCHECK(type() == Type::INT64);
    return std::get<int64_t>(value_);
}

double Value::ToDouble() const
{
    DCHECK(type() == Type::DOUBLE);
    return std::get<double>(value_);
}

const std::string& Value::Unit() const
{
    return unit_;
}

bool Value::HasUnit() const
{
    return !unit_.empty();
}

} // namespace aspia
