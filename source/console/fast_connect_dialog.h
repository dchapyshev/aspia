//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CONSOLE__FAST_CONNECT_DIALOG_H
#define CONSOLE__FAST_CONNECT_DIALOG_H

#include "base/macros_magic.h"
#include "client/client_config.h"
#include "proto/desktop.pb.h"
#include "ui_fast_connect_dialog.h"

#include <QDialog>

class QAbstractButton;

namespace console {

class FastConnectDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FastConnectDialog(QWidget* parent = nullptr);
    ~FastConnectDialog();

private slots:
    void sessionTypeChanged(int item_index);
    void sessionConfigButtonPressed();
    void onButtonBoxClicked(QAbstractButton* button);

private:
    void reloadRouters();

    Ui::FastConnectDialog ui;

    client::RouterConfigList routers_;
    client::Config config_;
    proto::DesktopConfig desktop_config_;

    DISALLOW_COPY_AND_ASSIGN(FastConnectDialog);
};

} // namespace console

#endif // CONSOLE__FAST_CONNECT_DIALOG_H
