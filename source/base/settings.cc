//
// PROJECT:         Aspia
// FILE:            base/settings.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/settings.h"
#include "base/settings_registry.h"

namespace aspia {

// static
std::unique_ptr<Settings> Settings::Open()
{
    return std::make_unique<SettingsRegistry>();
}

} // namespace aspia
