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

#ifndef CLIENT_UI_ROUTER_DIALOG_H
#define CLIENT_UI_ROUTER_DIALOG_H

#include "ui_router_dialog.h"
#include "client/config.h"

#include <QDialog>

class QAbstractButton;

namespace client {

class RouterDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit RouterDialog(QWidget* parent = nullptr);
    RouterDialog(const RouterConfig& config, QWidget* parent = nullptr);
    ~RouterDialog() final;

    RouterConfig routerConfig() const;

private slots:
    void onButtonBoxClicked(QAbstractButton* button);
    void onShowPasswordButtonToggled(bool checked);

private:
    void showError(const QString& message);

    Ui::RouterDialog ui;
};

} // namespace client

#endif // CLIENT_UI_ROUTER_DIALOG_H
