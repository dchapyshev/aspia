//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#ifndef CLIENT_ANDROID_SETTINGS_WIDGET_H
#define CLIENT_ANDROID_SETTINGS_WIDGET_H

#include <functional>

#include "client/settings.h"
#include "common/android/scroll_area.h"
#include "proto/desktop_control.h"

class QVBoxLayout;

// Settings screen for the Android client: the same preferences as the desktop client, applied
// immediately on change. Backed by the shared Settings storage.
class SettingsWidget final : public ScrollArea
{
    Q_OBJECT

public:
    explicit SettingsWidget(QWidget* parent = nullptr);
    ~SettingsWidget() final;

    // Rebuilds the content with the current language. Called by the parent on a language change,
    // because only top-level widgets receive QEvent::LanguageChange.
    void retranslate();

private:
    void buildContent();
    void addSectionHeader(QVBoxLayout* layout, const QString& text);
    void addBoolSetting(QVBoxLayout* layout, const QString& text, bool value,
                        const std::function<void(bool)>& on_changed);
    void buildInterfaceSection(QVBoxLayout* layout);
    void buildSecuritySection(QVBoxLayout* layout);
    void buildUdpSection(QVBoxLayout* layout);
    void buildDesktopSection(QVBoxLayout* layout);

    // Enables or disables biometric unlock, returning whether the requested state was reached so the
    // caller can revert the switch on cancellation or failure.
    bool setBiometricEnabled(bool enable);

    Settings settings_;
    proto::control::Config desktop_config_;
    quint32 udp_methods_;

    Q_DISABLE_COPY_MOVE(SettingsWidget)
};

#endif // CLIENT_ANDROID_SETTINGS_WIDGET_H
