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

#ifndef CLIENT__UI__CLIENT_DIALOG_H
#define CLIENT__UI__CLIENT_DIALOG_H

#include "base/macros_magic.h"
#include "client/client_config.h"
#include "proto/desktop.pb.h"

#include <QDialog>

class QAbstractButton;

namespace Ui {
class ClientDialog;
} // namespace Ui

namespace client {

class ClientDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ClientDialog(QWidget* parent = nullptr);
    ~ClientDialog();

private slots:
    void sessionTypeChanged(int item_index);
    void sessionConfigButtonPressed();
    void onButtonBoxClicked(QAbstractButton* button);

private:
    std::unique_ptr<Ui::ClientDialog> ui;

    Config config_;
    proto::DesktopConfig desktop_config_;

    DISALLOW_COPY_AND_ASSIGN(ClientDialog);
};

} // namespace client

#endif // CLIENT__UI__CLIENT_DIALOG_H
