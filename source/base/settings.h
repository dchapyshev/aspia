//
// PROJECT:         Aspia
// FILE:            base/settings.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__SETTINGS_H
#define _ASPIA_BASE__SETTINGS_H

#include <cstdint>
#include <memory>
#include <string>

namespace aspia {

class Settings
{
public:
    virtual ~Settings() = default;

    static std::unique_ptr<Settings> Open();

    virtual int32_t GetInt32(std::wstring_view name, int32_t def_value) const = 0;
    virtual uint32_t GetUInt32(std::wstring_view name, uint32_t def_value) const = 0;
    virtual bool GetBool(std::wstring_view name, bool def_value) const = 0;
    virtual std::wstring GetString(std::wstring_view name, std::wstring_view def_value) const = 0;

    virtual void SetInt32(std::wstring_view name, int32_t value) = 0;
    virtual void SetUInt32(std::wstring_view name, uint32_t value) = 0;
    virtual void SetBool(std::wstring_view name, bool value) = 0;
    virtual void SetString(std::wstring_view name, std::wstring_view value) = 0;
};

} // namespace aspia

#endif // _ASPIA_BASE__SETTINGS_H
