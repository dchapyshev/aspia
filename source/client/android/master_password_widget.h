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

#ifndef CLIENT_ANDROID_MASTER_PASSWORD_WIDGET_H
#define CLIENT_ANDROID_MASTER_PASSWORD_WIDGET_H

#include <QWidget>

class Label;
class LineEdit;

class MasterPasswordWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit MasterPasswordWidget(QWidget* parent = nullptr);
    ~MasterPasswordWidget() final;

    // Resets the form and moves the focus to the current password field.
    void prepare();

    // Clears the entered passwords and the error.
    void clear();

signals:
    // Emitted after the master password has been changed.
    void sig_finished();

private slots:
    void onChangeClicked();

private:
    void showError(const QString& message);

    LineEdit* edit_current_ = nullptr;
    LineEdit* edit_password_ = nullptr;
    LineEdit* edit_confirm_ = nullptr;
    Label* label_error_ = nullptr;

    Q_DISABLE_COPY_MOVE(MasterPasswordWidget)
};

#endif // CLIENT_ANDROID_MASTER_PASSWORD_WIDGET_H
