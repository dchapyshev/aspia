//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/version.h"
#include "client/ui/sys_info/sys_info_widget.h"
#include "ui_sys_info_widget_summary.h"

namespace client {

class SysInfoWidgetSummary : public SysInfoWidget
{
    Q_OBJECT

public:
    explicit SysInfoWidgetSummary(QWidget* parent = nullptr);
    ~SysInfoWidgetSummary() override;

    // SysInfo implementation.
    std::string category() const override;
    void setSystemInfo(const proto::system_info::SystemInfo& system_info) override;
    QTreeWidget* treeWidget() override;

    void setRouterVersion(const base::Version& router_version);
    void setHostVersion(const base::Version& host_version);
    void setClientVersion(const base::Version& client_version);

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
