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

#ifndef COMMON_UI_DIALOG_BUTTON_BOX_H
#define COMMON_UI_DIALOG_BUTTON_BOX_H

#include <QDialogButtonBox>

namespace common {

class DialogButtonBox : public QDialogButtonBox
{
    Q_OBJECT

public:
    explicit DialogButtonBox(QWidget* parent = nullptr);
    DialogButtonBox(Qt::Orientation orientation, QWidget* parent = nullptr);
    DialogButtonBox(StandardButtons buttons, QWidget* parent = nullptr);
    DialogButtonBox(StandardButtons buttons, Qt::Orientation orientation, QWidget* parent = nullptr);
    ~DialogButtonBox() override;

protected:
    // QWidget implementation.
    void showEvent(QShowEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    void translateButtons();

    Q_DISABLE_COPY_MOVE(DialogButtonBox)
};

} // namespace common

#endif // COMMON_UI_DIALOG_BUTTON_BOX_H
