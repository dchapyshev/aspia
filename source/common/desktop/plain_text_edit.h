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

#ifndef COMMON_DESKTOP_PLAIN_TEXT_EDIT_H
#define COMMON_DESKTOP_PLAIN_TEXT_EDIT_H

#include <QPlainTextEdit>

// Plain text edit that can limit the length of its text.
class PlainTextEdit final : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit PlainTextEdit(QWidget* parent = nullptr);
    ~PlainTextEdit() final;

    void setMaxLength(int length);

protected:
    // QPlainTextEdit implementation.
    void keyPressEvent(QKeyEvent* event) final;
    void insertFromMimeData(const QMimeData* source) final;
    void inputMethodEvent(QInputMethodEvent* event) final;

private slots:
    void onTextChanged();

private:
    int freeLength() const;

    int max_length_ = -1;

    Q_DISABLE_COPY_MOVE(PlainTextEdit)
};

#endif // COMMON_DESKTOP_PLAIN_TEXT_EDIT_H
