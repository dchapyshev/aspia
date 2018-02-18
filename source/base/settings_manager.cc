//
// PROJECT:         Aspia
// FILE:            base/settings_manager.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/settings_manager.h"
#include "base/logging.h"

namespace aspia {

namespace {

constexpr wchar_t kLanguage[] = L"Language";

} // namespace

SettingsManager::SettingsManager()
    : settings_(Settings::Open())
{
    DCHECK(settings_ != nullptr);
}

LANGID SettingsManager::GetUILanguage() const
{
    return static_cast<LANGID>(settings_->GetUInt32(kLanguage, GetUserDefaultLangID()));
}

void SettingsManager::SetUILanguage(LANGID langid)
{
    settings_->SetUInt32(kLanguage, langid);
}

} // namespace aspia
