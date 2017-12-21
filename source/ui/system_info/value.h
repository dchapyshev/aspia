//
// PROJECT:         Aspia
// FILE:            ui/system_info/value.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__SYSTEM_INFO__VALUE_H
#define _ASPIA_UI__SYSTEM_INFO__VALUE_H

#include <cstdint>
#include <string>
#include <variant>

namespace aspia {

class Value
{
public:
    Value(const Value& other);
    Value& operator=(const Value& other);

    static Value EmptyString();
    static Value String(std::string_view value);
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

    static Value MemorySize(uint64_t value);

    enum class Type
    {
        STRING      = 0,
        BOOL        = 1,
        UINT32      = 2,
        INT32       = 3,
        UINT64      = 4,
        INT64       = 5,
        DOUBLE      = 6,
        MEMORY_SIZE = 7
    };

    Type type() const;
    std::string_view ToString() const;
    bool ToBool() const;
    uint32_t ToUint32() const;
    int32_t ToInt32() const;
    uint64_t ToUint64() const;
    int64_t ToInt64() const;
    double ToDouble() const;
    double ToMemorySize() const;
    std::string_view Unit() const;
    bool HasUnit() const;

private:
    using ValueType = std::variant<std::string_view,
                                   bool,
                                   uint32_t,
                                   int32_t,
                                   uint64_t,
                                   int64_t,
                                   double>;

    Value(Type type, const ValueType& value, std::string_view unit);
    Value(Type type, const ValueType& value);

    Type type_;
    ValueType value_;
    std::string_view unit_;
};

} // namespace aspia

#endif // _ASPIA_UI__SYSTEM_INFO__VALUE_H
