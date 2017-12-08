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

Value::Value() = default;

Value::Value(std::string_view value, std::string_view unit)
    : string_(value),
      unit_(unit)
{
    // Nothing
}

Value::Value(Value&& other)
    : string_(std::move(other.string_)),
      unit_(std::move(other.unit_))
{
    // Nothing
}

Value& Value::operator=(Value&& other)
{
    string_ = std::move(other.string_);
    unit_ = std::move(other.unit_);
    return *this;
}

// static
Value Value::Empty()
{
    return Value();
}

// static
Value Value::String(const std::string_view value)
{
    return Value(value, std::string());
}

// static
Value Value::String(const char* format, ...)
{
    va_list args;

    va_start(args, format);

    int len = _vscprintf(format, args);
    CHECK(len >= 0) << errno;

    std::string out;
    out.resize(len);

    CHECK(SUCCEEDED(StringCchVPrintfA(&out[0], len + 1, format, args)));

    va_end(args);

    return Value(out, std::string());
}

// static
Value Value::Bool(bool value, std::string_view if_true, std::string_view if_false)
{
    return Value(value ? if_true : if_false, std::string());
}

// static
Value Value::Bool(bool value)
{
    return Bool(value, "Yes", "No");
}

// static
Value Value::Number(uint32_t value, std::string_view unit)
{
    return Value(std::to_string(value), unit);
}

// static
Value Value::Number(uint32_t value)
{
    return Number(value, std::string());
}

// static
Value Value::Number(int32_t value, std::string_view unit)
{
    return Value(std::to_string(value), unit);
}

// static
Value Value::Number(int32_t value)
{
    return Number(value, std::string());
}

// static
Value Value::Number(uint64_t value, std::string_view unit)
{
    return Value(std::to_string(value), unit);
}

// static
Value Value::Number(uint64_t value)
{
    return Number(value, std::string());
}

// static
Value Value::Number(int64_t value, std::string_view unit)
{
    return Value(std::to_string(value), unit);
}

// static
Value Value::Number(int64_t value)
{
    return Number(value, std::string());
}

// static
Value Value::Number(double value, std::string_view unit)
{
    return Value(std::to_string(value), unit);
}

// static
Value Value::Number(double value)
{
    return Number(value, std::string());
}

const std::string& Value::ToString() const
{
    return string_;
}
const std::string& Value::Unit() const
{
    return unit_;
}

bool Value::IsEmpty() const
{
    return string_.empty() && unit_.empty();
}

bool Value::HasUnit() const
{
    return !unit_.empty();
}

} // namespace aspia
