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

#ifndef CLIENT_UI_CLIENT_WINDOW_H
#define CLIENT_UI_CLIENT_WINDOW_H

#include <QByteArray>
#include <QMainWindow>

#include "ui_client_window.h"

namespace common {
class UpdateChecker;
} // namespace common

namespace client {

class ClientWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit ClientWindow(QWidget* parent = nullptr);
    ~ClientWindow() final;

protected:
    // QMainWindow implementation.
    void closeEvent(QCloseEvent* event) final;

private slots:
    void onUpdateCheckedFinished(const QByteArray& result);
    void onLanguageChanged(QAction* action);
    void onThemeChanged(QAction* action);
    void onAfterThemeChanged();
    void onSettings();
    void onHelp();
    void onAbout();
    void sessionTypeChanged(int item_index);
    void sessionConfigButtonPressed();
    void connectToHost();
    void onCheckUpdates();

private:
    void createLanguageMenu(const QString& current_locale);
    void createThemeMenu(const QString& current_theme);
    void reloadSessionTypes();

    Ui::ClientWindow ui;
    std::unique_ptr<common::UpdateChecker> update_checker_;

    Q_DISABLE_COPY_MOVE(ClientWindow)
};

} // namespace client

#endif // CLIENT_UI_CLIENT_WINDOW_H
