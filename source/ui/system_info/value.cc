//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/value.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/logging.h"
#include "ui/system_info/value.h"

#include <strsafe.h>

namespace aspia {

Value::Value(std::string&& value, std::string&& unit)
    : type_(Type::STRING),
      value_(std::move(value)),
      unit_(std::move(unit))
{
    // Nothing
}

Value::Value(bool value)
    : type_(Type::BOOL),
      value_(value)
{
    // Nothing
}

Value::Value(uint32_t value, std::string_view unit)
    : type_(Type::UINT32),
      value_(value),
      unit_(unit)
{
    // Nothing
}

Value::Value(int32_t value, std::string_view unit)
    : type_(Type::INT32),
      value_(value),
      unit_(unit)
{
    // Nothing
}

Value::Value(uint64_t value, std::string_view unit)
    : type_(Type::UINT64),
      value_(value),
      unit_(unit)
{
    // Nothing
}

Value::Value(int64_t value, std::string_view unit)
    : type_(Type::INT64),
      value_(value),
      unit_(unit)
{
    // Nothing
}

Value::Value(double value, std::string_view unit)
    : type_(Type::DOUBLE),
      value_(value),
      unit_(unit)
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
    return Value(std::string(), std::string());
}

// static
Value Value::String(std::string_view value)
{
    return Value(std::string(value), std::string());
}

// static
Value Value::FormattedString(const char* format, ...)
{
    va_list args;

    va_start(args, format);

    int len = _vscprintf(format, args);
    CHECK(len >= 0) << errno;

    std::string out;
    out.resize(len);

    CHECK(SUCCEEDED(StringCchVPrintfA(&out[0], len + 1, format, args)));

    va_end(args);

    return Value(std::move(out), std::string());
}

// static
Value Value::Bool(bool value)
{
    return Value(value);
}

// static
Value Value::Number(uint32_t value, std::string_view unit)
{
    return Value(value, unit);
}

// static
Value Value::Number(uint32_t value)
{
    return Number(value, std::string());
}

// static
Value Value::Number(int32_t value, std::string_view unit)
{
    return Value(value, unit);
}

// static
Value Value::Number(int32_t value)
{
    return Number(value, std::string());
}

// static
Value Value::Number(uint64_t value, std::string_view unit)
{
    return Value(value, unit);
}

// static
Value Value::Number(uint64_t value)
{
    return Number(value, std::string());
}

// static
Value Value::Number(int64_t value, std::string_view unit)
{
    return Value(value, unit);
}

// static
Value Value::Number(int64_t value)
{
    return Number(value, std::string());
}

// static
Value Value::Number(double value, std::string_view unit)
{
    return Value(value, unit);
}

// static
Value Value::Number(double value)
{
    return Number(value, std::string());
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
