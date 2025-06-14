//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef COMMON_UI_ABOUT_DIALOG_H
#define COMMON_UI_ABOUT_DIALOG_H

#include <QDialog>

namespace Ui {
class AboutDialog;
} // namespace Ui

namespace common {

class AboutDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit AboutDialog(const QString& application_name, QWidget* parent = nullptr);
    ~AboutDialog() final;

private slots:
    void onServiceContextMenu(const QPoint& pos);

private:
    std::unique_ptr<Ui::AboutDialog> ui;

    Q_DISABLE_COPY(AboutDialog)
};

} // namespace common

#endif // COMMON_UI_ABOUT_DIALOG_H
