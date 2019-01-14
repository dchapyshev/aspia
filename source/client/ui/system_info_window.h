//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef ASPIA_CLIENT__UI__SYSTEM_INFO_WINDOW_H
#define ASPIA_CLIENT__UI__SYSTEM_INFO_WINDOW_H

#include "base/macros_magic.h"
#include "protocol/system_info.pb.h"
#include "ui_system_info_window.h"

namespace client {

class SystemInfoWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit SystemInfoWindow(QWidget* parent = nullptr);
    ~SystemInfoWindow();

    void setSystemInfo(const proto::system_info::SystemInfo& system_info);

private:
    static QString sizeToString(int64_t size);
    static QString delayToString(uint64_t delay);
    static QString speedToString(uint64_t speed);

    Ui::SystemInfoWindow ui;

    DISALLOW_COPY_AND_ASSIGN(SystemInfoWindow);
};

} // namespace client

#endif // ASPIA_CLIENT__UI__SYSTEM_INFO_WINDOW_H
