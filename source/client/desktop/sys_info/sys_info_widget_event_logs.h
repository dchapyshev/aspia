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

#ifndef CLIENT_DESKTOP_SYS_INFO_SYS_INFO_EVENT_LOGS_H
#define CLIENT_DESKTOP_SYS_INFO_SYS_INFO_EVENT_LOGS_H

#include <QByteArray>

#include <memory>

#include "client/desktop/sys_info/sys_info_widget.h"
#include "proto/system_info.h"

namespace Ui {
class SysInfoEventLogs;
} // namespace Ui

class SysInfoWidgetEventLogs final : public SysInfoWidget
{
    Q_OBJECT

public:
    explicit SysInfoWidgetEventLogs(QWidget* parent = nullptr);
    ~SysInfoWidgetEventLogs() final;

    // SysInfo implementation.
    std::string category() const final;
    proto::system_info::SystemInfoRequest request() const final;
    void setSystemInfo(const proto::system_info::SystemInfo& system_info) final;
    QTreeWidget* treeWidget() final;

private slots:
    void onContextMenu(const QPoint& point);

private:
    proto::system_info::SystemInfoRequest createRequest(
        proto::system_info::EventLogs::Event::Type type, const QByteArray& cursor,
        proto::system_info::EventLogsData::Direction direction) const;
    static QString levelToString(proto::system_info::EventLogs::Event::Level value);

    std::unique_ptr<Ui::SysInfoEventLogs> ui;
    int current_column_ = 0;

    static const quint32 kRecordsPerPage = 1000;

    QByteArray first_cursor_;
    QByteArray last_cursor_;
    QByteArray pending_cursor_;
    proto::system_info::EventLogsData::Direction pending_direction_ =
        proto::system_info::EventLogsData::DIRECTION_OLDER;
};

#endif // CLIENT_DESKTOP_SYS_INFO_SYS_INFO_EVENT_LOGS_H
