//
// SmartCafe Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CLIENT_UI_SYS_INFO_SYS_INFO_SUMMARY_H
#define CLIENT_UI_SYS_INFO_SYS_INFO_SUMMARY_H

#include "client/ui/sys_info/sys_info_widget.h"
#include "ui_sys_info_widget_summary.h"

#include <QVersionNumber>

namespace client {

class SysInfoWidgetSummary final : public SysInfoWidget
{
    Q_OBJECT

public:
    explicit SysInfoWidgetSummary(QWidget* parent = nullptr);
    ~SysInfoWidgetSummary() final;

    // SysInfo implementation.
    std::string category() const final;
    void setSystemInfo(const proto::system_info::SystemInfo& system_info) final;
    QTreeWidget* treeWidget() final;

    void setRouterVersion(const QVersionNumber& router_version);
    void setHostVersion(const QVersionNumber& host_version);
    void setClientVersion(const QVersionNumber& client_version);

private slots:
    void onContextMenu(const QPoint& point);

private:
    Ui::SysInfoSummary ui;

    QString router_version_;
    QString host_version_;
    QString client_version_;
};

} // namespace client

#endif // CLIENT_UI_SYS_INFO_SYS_INFO_SUMMARY_H
