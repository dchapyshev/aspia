//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/value.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__SYSTEM_INFO__VALUE_H
#define _ASPIA_UI__SYSTEM_INFO__VALUE_H

#include <cstdint>
#include <string>

namespace aspia {

class Value
{
public:
    Value(Value&& other);
    Value& operator=(Value&& other);

    static Value Empty();
    static Value String(const std::string_view value);
    static Value String(const char* format, ...);
    static Value Bool(bool value, std::string_view if_true, std::string_view if_false);
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

    const std::string& ToString() const;
    const std::string& Unit() const;
    bool IsEmpty() const;
    bool HasUnit() const;

private:
    Value();
    Value(std::string_view value, std::string_view unit);

    std::string string_;
    std::string unit_;
};

} // namespace aspia

#endif // _ASPIA_UI__SYSTEM_INFO__VALUE_H
