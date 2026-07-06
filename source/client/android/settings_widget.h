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

#include <QList>
#include <QWidget>

#include <functional>

#include "client/settings.h"
#include "proto/desktop_control.h"

class QVBoxLayout;
class QStackedWidget;
class AboutWidget;
class IconButton;
class ScrollArea;

// Settings screen for the Android client: the same preferences as the desktop client, applied
// immediately on change. Backed by the shared Settings storage. The about screen is hosted as a
// sub-page reached from the top app bar action.
class SettingsWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsWidget(QWidget* parent = nullptr);
    ~SettingsWidget() final;

    // The app bar action (opens the about screen). Empty while the about screen is shown.
    QList<QWidget*> appBarActions() const;

    // Returns to the settings page from the about screen. Driven by the app bar back button.
    void goBack();

    // Returns to the settings page without animation, used when the tab is left.
    void resetToSettings();

signals:
    // Requests the host bar to show |title| with a back button (about) or the default state.
    void sig_titleChanged(const QString& title, bool back_visible);

    // Emitted when the set returned by appBarActions() changes (the about screen hides the action).
    void sig_appBarActionsChanged();

private:
    void showAbout();
    bool isAboutPage() const;

    void buildSettings();
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

    QStackedWidget* stack_;
    ScrollArea* settings_page_;
    AboutWidget* about_page_;
    IconButton* about_button_;

    Settings settings_;
    proto::control::Config desktop_config_;
    quint32 udp_methods_;

    Q_DISABLE_COPY_MOVE(SettingsWidget)
};

#endif // CLIENT_ANDROID_SETTINGS_WIDGET_H
