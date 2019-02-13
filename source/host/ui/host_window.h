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

#ifndef HOST__UI__HOST_WINDOW_H
#define HOST__UI__HOST_WINDOW_H

#include <QMainWindow>

#include "base/macros_magic.h"
#include "ui_host_window.h"

namespace host {

class HostWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit HostWindow(QWidget* parent = nullptr);
    ~HostWindow();

private slots:
    void onHelp();
    void onAbout();

private:
    Ui::HostWindow ui;

    DISALLOW_COPY_AND_ASSIGN(HostWindow);
};

} // namespace host

#endif // HOST__UI__HOST_WINDOW_H
