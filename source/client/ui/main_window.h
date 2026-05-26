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
#include <QPointer>

#include <memory>

#include "client/ui/tab.h"

namespace Ui {
class MainWindow;
} // namespace Ui

namespace proto::peer {
enum SessionType : int;
} // namespace proto::peer

class Tab;
class QLineEdit;
class UpdateChecker;
class HostConfig;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() final;

public slots:
    void showAndActivate();

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
    void onConnect(const HostConfig& host, proto::peer::SessionType session_type);
    void onTabDetachRequested(int index, const QPoint& global_pos);
    void onTabDragMove(const QPoint& global_pos);
    void onTabDragFinished(const QPoint& global_pos);
    void onTabFullscreenRequested(bool enabled);
    void onTabMinimizeRequested();
    void onTabShowRequested();
    void onAlwaysOnTop(bool checked);

private:
    void addTab(Tab* tab, const QString& title, const QIcon& icon);
    bool tabBarHitTest(const QPoint& global_pos) const;
    void hideCloseButtonForTab(int index);
    Tab* tabAt(int index);
    void updateSearchFieldVisibility();
    void updateStatusBarVisibility();
    void installTabActions(Tab* tab);
    void removeTabActions();
    void updateSeparatorVisibility();
    QMenu* menuForActionGroup(Tab::ActionRole group) const;

    std::unique_ptr<Ui::MainWindow> ui;
    std::unique_ptr<UpdateChecker> update_checker_;

    QLineEdit* search_field_ = nullptr;
    QAction* search_action_ = nullptr;
    Tab* active_tab_ = nullptr;
    QList<QPointer<QAction>> tab_toolbar_actions_;
    QList<QPair<QMenu*, QList<QPointer<QAction>>>> tab_menu_actions_;

    Q_DISABLE_COPY_MOVE(MainWindow)
};

#endif // CLIENT_UI_MAIN_WINDOW_H
