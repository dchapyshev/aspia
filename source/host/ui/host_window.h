//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef HOST__UI__HOST_WINDOW_H
#define HOST__UI__HOST_WINDOW_H

#include <QMainWindow>
#include <QPointer>
#include <QSystemTrayIcon>

#include "base/macros_magic.h"
#include "common/locale_loader.h"
#include "host/host_settings.h"
#include "ui_host_window.h"

namespace host {

class HostNotifierWindow;
class UiClient;

class HostWindow : public QMainWindow
{
    Q_OBJECT

public:
    HostWindow(Settings& settings, common::LocaleLoader& locale_loader, QWidget* parent = nullptr);
    ~HostWindow();

    void hideToTray();

private slots:
    void refreshIpList();
    void newPassword();

    void onLanguageChanged(QAction* action);
    void onSettings();
    void onShowHide();
    void onHelp();
    void onAbout();

private:
    void createLanguageMenu(const QString& current_locale);

    Ui::HostWindow ui;

    Settings& settings_;
    common::LocaleLoader& locale_loader_;

    QSystemTrayIcon tray_icon_;
    QMenu tray_menu_;

    QPointer<HostNotifierWindow> notifier_;
    QPointer<UiClient> client_;

    DISALLOW_COPY_AND_ASSIGN(HostWindow);
};

} // namespace host

#endif // HOST__UI__HOST_WINDOW_H
