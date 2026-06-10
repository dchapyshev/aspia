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

#ifndef CLIENT_DESKTOP_SYS_INFO_SYS_INFO_SERVICES_H
#define CLIENT_DESKTOP_SYS_INFO_SYS_INFO_SERVICES_H

#include <memory>

#include "client/desktop/sys_info/sys_info_widget.h"
#include "proto/system_info.h"

namespace Ui {
class SysInfoServices;
} // namespace Ui

class SysInfoWidgetServices final : public SysInfoWidget
{
    Q_OBJECT

public:
    explicit SysInfoWidgetServices(QWidget* parent = nullptr);
    ~SysInfoWidgetServices() final;

    // SysInfo implementation.
    std::string category() const final;
    void setSystemInfo(const proto::system_info::SystemInfo& system_info) final;
    QTreeWidget* treeWidget() final;

protected:
    // SysInfoWidget implementation.
    void retranslate() final;

private slots:
    void onContextMenu(const QPoint& point);

private:
    static QString statusToString(proto::system_info::Services::Service::Status status);
    static QString startupTypeToString(
        proto::system_info::Services::Service::StartupType startup_type);

    std::unique_ptr<Ui::SysInfoServices> ui;
    int current_column_ = 0;
};

#endif // CLIENT_DESKTOP_SYS_INFO_SYS_INFO_SERVICES_H
