//
// PROJECT:         Aspia
// FILE:            base/settings_registry.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__SETTINGS_REGISTRY_H
#define _ASPIA_BASE__SETTINGS_REGISTRY_H

#include "base/macros.h"
#include "base/registry.h"
#include "base/settings.h"

namespace aspia {

class SettingsRegistry : public Settings
{
public:
    SettingsRegistry();
    virtual ~SettingsRegistry() = default;

    int32_t GetInt32(std::wstring_view name, int32_t def_value) const override;
    uint32_t GetUInt32(std::wstring_view name, uint32_t def_value) const override;
    bool GetBool(std::wstring_view name, bool def_value) const override;
    std::wstring GetString(std::wstring_view name, std::wstring_view def_value) const override;

    void SetInt32(std::wstring_view name, int32_t value) override;
    void SetUInt32(std::wstring_view name, uint32_t value) override;
    void SetBool(std::wstring_view name, bool value) override;
    void SetString(std::wstring_view name, std::wstring_view value) override;

private:
    RegistryKey key_;

    DISALLOW_COPY_AND_ASSIGN(SettingsRegistry);
};

} // namespace aspia

#endif // _ASPIA_BASE__SETTINGS_REGISTRY_H
