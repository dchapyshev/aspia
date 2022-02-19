//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CLIENT__UI__QT_SYSTEM_INFO_WINDOW_H
#define CLIENT__UI__QT_SYSTEM_INFO_WINDOW_H

#include "base/macros_magic.h"
#include "client/system_info_window_proxy.h"
#include "client/ui/session_window.h"
#include "proto/system_info.pb.h"

#include <QTreeWidget>

namespace Ui {
class SystemInfoWindow;
} // namespace Ui

class QHBoxLayout;

namespace client {

class SysInfoWidget;

class QtSystemInfoWindow
    : public SessionWindow,
      public SystemInfoWindow
{
    Q_OBJECT

public:
    explicit QtSystemInfoWindow(QWidget* parent = nullptr);
    ~QtSystemInfoWindow() override;

    // SessionWindow implementation.
    std::unique_ptr<Client> createClient() override;

    // SystemInfoWindow implementation.
    void start(std::shared_ptr<SystemInfoControlProxy> system_info_control_proxy) override;
    void setSystemInfo(const proto::system_info::SystemInfo& system_info) override;

signals:
    void systemInfoRequired(const proto::system_info::SystemInfoRequest& request);

private slots:
    void onCategoryItemClicked(QTreeWidgetItem* item, int column);
    void onRefresh();

private:
    std::unique_ptr<Ui::SystemInfoWindow> ui;
    QHBoxLayout* layout_ = nullptr;
    QList<SysInfoWidget*> sys_info_widgets_;
    int current_widget_ = 0;

    std::shared_ptr<SystemInfoControlProxy> system_info_control_proxy_;
    std::shared_ptr<SystemInfoWindowProxy> system_info_window_proxy_;

    DISALLOW_COPY_AND_ASSIGN(QtSystemInfoWindow);
};

} // namespace client

#endif // CLIENT__UI__QT_SYSTEM_INFO_WINDOW_H
