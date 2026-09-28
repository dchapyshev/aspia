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

#ifndef CLIENT_DESKTOP_QUICK_CONNECT_DIALOG_H
#define CLIENT_DESKTOP_QUICK_CONNECT_DIALOG_H

#include <QDialog>

#include <memory>

namespace Ui {
class QuickConnectDialog;
} // namespace Ui

namespace proto::peer {
enum SessionType : int;
} // namespace proto::peer

class HostConfig;
class QAbstractButton;

class QuickConnectDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit QuickConnectDialog(QWidget* parent = nullptr);
    ~QuickConnectDialog() final;

    HostConfig host() const;
    proto::peer::SessionType sessionType() const;

private slots:
    void onRouterChanged(int index);
    void onButtonBoxClicked(QAbstractButton* button);
    void onLoadData();

private:
    void updateAddressLabel();

    std::unique_ptr<Ui::QuickConnectDialog> ui;
    qint64 router_id_ = 0;
    qint32 session_type_ = 0;

    Q_DISABLE_COPY_MOVE(QuickConnectDialog)
};

#endif // CLIENT_DESKTOP_QUICK_CONNECT_DIALOG_H
