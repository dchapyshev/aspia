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

#ifndef CLIENT_UI_HOSTS_ROUTER_USER_DIALOG_H
#define CLIENT_UI_HOSTS_ROUTER_USER_DIALOG_H

#include <QDialog>

#include <memory>

#include "base/peer/user.h"

class QAbstractButton;

namespace Ui {
class RouterUserDialog;
} // namespace Ui

namespace proto::router {
enum SessionType : int;
} // namespace proto::router

class RouterUserDialog final : public QDialog
{
    Q_OBJECT

public:
    RouterUserDialog(const User& user, const QStringList& users, QWidget* parent);
    ~RouterUserDialog() final;

    const User& user() const;

protected:
    // QDialog implementation.
    bool eventFilter(QObject* object, QEvent* event) final;

private:
    void onButtonBoxClicked(QAbstractButton* button);
    void setAccountChanged(bool changed);
    static QString sessionTypeToString(proto::router::SessionType session_type);

    std::unique_ptr<Ui::RouterUserDialog> ui;
    User user_;
    QStringList users_;
    bool account_changed_ = true;

    Q_DISABLE_COPY_MOVE(RouterUserDialog)
};

#endif // CLIENT_UI_HOSTS_ROUTER_USER_DIALOG_H
