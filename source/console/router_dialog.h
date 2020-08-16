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

#ifndef CONSOLE__ROUTER_DIALOG_H
#define CONSOLE__ROUTER_DIALOG_H

#include "proto/address_book.pb.h"
#include "ui_router_dialog.h"

#include <optional>

#include <QDialog>

namespace console {

class RouterDialog : public QDialog
{
    Q_OBJECT

public:
    RouterDialog(const std::optional<proto::address_book::Router>& router, QWidget* parent);
    ~RouterDialog();

    const std::optional<proto::address_book::Router>& router() const { return router_; }

private slots:
    void showPasswordButtonToggled(bool checked);
    void onButtonBoxClicked(QAbstractButton* button);

private:
    void showError(const QString& message);

    Ui::RouterDialog ui;
    std::optional<proto::address_book::Router> router_;
};

} // namespace console

#endif // CONSOLE__ROUTER_DIALOG_H
