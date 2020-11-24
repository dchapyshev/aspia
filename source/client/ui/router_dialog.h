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

#ifndef CLIENT__UI__ROUTER_DIALOG_H
#define CLIENT__UI__ROUTER_DIALOG_H

#include "client/router_config.h"

#include <optional>

#include <QDialog>

class QAbstractButton;

namespace Ui {
class RouterDialog;
} // namespace Ui

namespace client {

class RouterDialog : public QDialog
{
    Q_OBJECT

public:
    RouterDialog(const std::optional<RouterConfig>& router, QWidget* parent);
    ~RouterDialog();

    const std::optional<RouterConfig>& router() const { return router_; }

private slots:
    void showPasswordButtonToggled(bool checked);
    void onButtonBoxClicked(QAbstractButton* button);

private:
    void showError(const QString& message);

    std::unique_ptr<Ui::RouterDialog> ui;
    std::optional<RouterConfig> router_;
};

} // namespace client

#endif // CLIENT__UI__ROUTER_DIALOG_H
