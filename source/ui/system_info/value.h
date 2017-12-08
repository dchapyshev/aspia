//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/value.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__SYSTEM_INFO__VALUE_H
#define _ASPIA_UI__SYSTEM_INFO__VALUE_H

#include "base/macros.h"

#include <cstdint>
#include <string>
#include <variant>

namespace aspia {

class Value
{
public:
    Value(Value&& other);
    Value& operator=(Value&& other);

    static Value Empty();
    static Value String(const std::string_view value);
    static Value String(const char* format, ...);
    static Value Bool(bool value);
    static Value Number(uint32_t value, std::string_view unit);
    static Value Number(uint32_t value);
    static Value Number(int32_t value, std::string_view unit);
    static Value Number(int32_t value);
    static Value Number(uint64_t value, std::string_view unit);
    static Value Number(uint64_t value);
    static Value Number(int64_t value, std::string_view unit);
    static Value Number(int64_t value);
    static Value Number(double value, std::string_view unit);
    static Value Number(double value);

    enum class Type
    {
        EMPTY  = 0,
        STRING = 1,
        BOOL   = 2,
        UINT32 = 3,
        INT32  = 4,
        UINT64 = 5,
        INT64  = 6,
        DOUBLE = 7
    };

    Type type() const;
    std::string ToString() const;
    bool ToBool() const;
    uint32_t ToUint32() const;
    int32_t ToInt32() const;
    uint64_t ToUint64() const;
    int64_t ToInt64() const;
    double ToDouble() const;
    const std::string& Unit() const;
    bool HasUnit() const;

private:
    Value();
    Value(std::string&& value, std::string&& unit);
    Value(bool value);
    Value(uint32_t value, std::string_view unit);
    Value(int32_t value, std::string_view unit);
    Value(uint64_t value, std::string_view unit);
    Value(int64_t value, std::string_view unit);
    Value(double value, std::string_view unit);

    using ValueType =
        std::variant<std::string, bool, uint32_t, int32_t, uint64_t, int64_t, double>;

    const Type type_ = Type::EMPTY;
    ValueType value_;
    std::string unit_;

    DISALLOW_COPY_AND_ASSIGN(Value);
};

} // namespace aspia

#endif // _ASPIA_UI__SYSTEM_INFO__VALUE_H
