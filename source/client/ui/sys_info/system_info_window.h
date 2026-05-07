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

#ifndef CLIENT_UI_SYS_INFO_SYSTEM_INFO_WINDOW_H
#define CLIENT_UI_SYS_INFO_SYSTEM_INFO_WINDOW_H

#include "client/ui/client_window.h"
#include "proto/system_info.h"

namespace Ui {
class SystemInfoWindow;
} // namespace Ui

class QHBoxLayout;
class QTreeWidgetItem;
class SysInfoWidget;

class SystemInfoWindow final : public ClientWindow
{
    Q_OBJECT

public:
    explicit SystemInfoWindow(QWidget* parent = nullptr);
    ~SystemInfoWindow() final;

    // ClientWindow implementation.
    Client* createClient() final;
    void setTabbedMode(bool tabbed) final;
    QList<QPair<Tab::ActionRole, QList<QAction*>>> tabActionGroups() const final;
    QByteArray saveState() const final;
    void restoreState(const QByteArray& state) final;

public slots:
    void onShowWindow();
    void onSystemInfoChanged(const proto::system_info::SystemInfo& system_info);

signals:
    void sig_systemInfoRequired(const proto::system_info::SystemInfoRequest& request);

protected:
    // ClientWindow implementation.
    void onInternalReset() final;

    // QWidget implementation.
    void changeEvent(QEvent* event) final;

private slots:
    void onCategoryItemClicked(QTreeWidgetItem* item, int column);
    void onRefresh();

private:
    void buildCategoryTree();

    std::unique_ptr<Ui::SystemInfoWindow> ui;
    QHBoxLayout* layout_ = nullptr;
    QList<SysInfoWidget*> sys_info_widgets_;
    int current_widget_ = 0;
    QAction* action_desktop_ = nullptr;
    QAction* action_file_transfer_ = nullptr;
    QAction* action_text_chat_ = nullptr;

    Q_DISABLE_COPY_MOVE(SystemInfoWindow)
};

#endif // CLIENT_UI_SYS_INFO_SYSTEM_INFO_WINDOW_H
