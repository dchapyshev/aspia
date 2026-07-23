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

#ifndef COMMON_ANDROID_DIALOG_H
#define COMMON_ANDROID_DIALOG_H

#include <QDialog>

#include "common/android/button.h"

class QHBoxLayout;
class QVBoxLayout;
class Label;

// Modal dialog adapted for touch screens: a scrim dims the parent window and a rounded card in
// the center holds the title, the message, custom content and the action buttons. A tap on the
// scrim dismisses the dialog. Like Menu and BottomSheet, the dialog is an overlay child widget
// covering the parent window rather than a separate native window: on Android creating a window
// while the surface of a just closed one is still being torn down aborts inside the platform
// plugin.
class Dialog : public QDialog
{
    Q_OBJECT

public:
    explicit Dialog(QWidget* parent = nullptr);
    ~Dialog() override;

    void setTitle(const QString& title);
    void setText(const QString& text);

    // Layout for custom content placed between the message and the action buttons.
    QVBoxLayout* contentLayout() const { return content_layout_; }

    // Adds an action button to the trailing edge of the button row.
    Button* addButton(const QString& text, Button::Role role = Button::Role::TEXT);

    // QDialog implementation.
    void done(int result) final;

protected:
    // QObject implementation.
    bool eventFilter(QObject* object, QEvent* event) final;

    // QDialog implementation.
    void mousePressEvent(QMouseEvent* event) final;
    void paintEvent(QPaintEvent* event) final;
    void resizeEvent(QResizeEvent* event) final;
    void showEvent(QShowEvent* event) final;

private:
    // Caps the card width to the screen so long content does not overflow a narrow display.
    void updateCardWidth();

    QWidget* card_;
    Label* title_label_;
    Label* text_label_;
    QVBoxLayout* content_layout_;
    QHBoxLayout* button_row_;

    Q_DISABLE_COPY_MOVE(Dialog)
};

#endif // COMMON_ANDROID_DIALOG_H
