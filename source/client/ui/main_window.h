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

#ifndef CLIENT_UI_MAIN_WINDOW_H
#define CLIENT_UI_MAIN_WINDOW_H

#include <QByteArray>
#include <QMainWindow>

#include "client/ui/client_tab.h"
#include "proto/peer.h"
#include "ui_main_window.h"

class QLineEdit;

namespace common {
class UpdateChecker;
} // namespace common

namespace client {

class ClientTab;
struct ComputerConfig;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() final;

protected:
    // QMainWindow implementation.
    void closeEvent(QCloseEvent* event) final;

private slots:
    void onUpdateCheckedFinished(const QByteArray& result);
    void onAfterThemeChanged();
    void onSettings();
    void onHelp();
    void onAbout();
    void onCurrentTabChanged(int index);
    void onCloseTab(int index);
    void onSearchTextChanged(const QString& text);
    void onConnect(qint64 computer_id,
                   const client::ComputerConfig& computer,
                   proto::peer::SessionType session_type);

private:
    void addTab(ClientTab* tab, const QString& title, const QIcon& icon);
    void hideCloseButtonForTab(int index);
    ClientTab* tabAt(int index);
    void updateSearchFieldVisibility();
    void installTabActions(ClientTab* tab);
    void removeTabActions();
    void updateSeparatorVisibility();
    QMenu* menuForActionGroup(ClientTab::ActionRole group) const;

    Ui::MainWindow ui;
    std::unique_ptr<common::UpdateChecker> update_checker_;

    QLineEdit* search_field_ = nullptr;
    QAction* search_action_ = nullptr;
    ClientTab* active_tab_ = nullptr;
    QList<QAction*> tab_toolbar_actions_;
    QList<QPair<QMenu*, QList<QAction*>>> tab_menu_actions_;

    Q_DISABLE_COPY_MOVE(MainWindow)
};

} // namespace client

#endif // CLIENT_UI_MAIN_WINDOW_H
