//
// PROJECT:         Aspia
// FILE:            base/settings_manager.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__SETTINGS_MANAGER_H
#define _ASPIA_BASE__SETTINGS_MANAGER_H

#include "base/settings.h"
#include "base/macros.h"

namespace aspia {

class SettingsManager
{
public:
    SettingsManager();
    ~SettingsManager() = default;

    LANGID GetUILanguage() const;
    void SetUILanguage(LANGID langid);

private:
    std::unique_ptr<Settings> settings_;

    DISALLOW_COPY_AND_ASSIGN(SettingsManager);
};

} // namespace aspia

#endif // _ASPIA_BASE__SETTINGS_MANAGER_H
