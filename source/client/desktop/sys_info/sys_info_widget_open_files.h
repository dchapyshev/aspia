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

#ifndef CLIENT_DESKTOP_SYS_INFO_SYS_INFO_OPEN_FILES_H
#define CLIENT_DESKTOP_SYS_INFO_SYS_INFO_OPEN_FILES_H

#include <memory>

#include "client/desktop/sys_info/sys_info_widget.h"

namespace Ui {
class SysInfoOpenFiles;
} // namespace Ui

class SysInfoWidgetOpenFiles final : public SysInfoWidget
{
    Q_OBJECT

public:
    explicit SysInfoWidgetOpenFiles(QWidget* parent = nullptr);
    ~SysInfoWidgetOpenFiles() final;

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
    std::unique_ptr<Ui::SysInfoOpenFiles> ui;
    int current_column_ = 0;
};

#endif // CLIENT_DESKTOP_SYS_INFO_SYS_INFO_OPEN_FILES_H
