//
// Aspia Project
// Copyright (C) 2021 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CLIENT__UI__SYS_INFO_H
#define CLIENT__UI__SYS_INFO_H

#include "proto/desktop_extensions.pb.h"

#include <QWidget>
#include <QTreeWidgetItem>

namespace client {

class SysInfoWidget : public QWidget
{
    Q_OBJECT

public:
    ~SysInfoWidget() = default;

    virtual std::string category() const = 0;
    virtual void setSystemInfo(const proto::SystemInfo& system_info) = 0;
    virtual QTreeWidget* treeWidget() = 0;

    static QString sizeToString(int64_t size);
    static QString delayToString(uint64_t delay);
    static QString speedToString(uint64_t speed);
    static QString timeToString(time_t time);

protected:
    SysInfoWidget(QWidget* parent = nullptr);

    void copyRow(QTreeWidgetItem* item);
    void copyColumn(QTreeWidgetItem* item, int column);
};

} // namespace client

#endif // CLIENT__UI__SYS_INFO_H
