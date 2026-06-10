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

#include "common/desktop/password_edit.h"

#include <QResizeEvent>
#include <QStyle>
#include <QToolButton>

#include "base/crypto/secure_string.h"
#include "base/gui_application.h"

//--------------------------------------------------------------------------------------------------
PasswordEdit::PasswordEdit(QWidget* parent)
    : QLineEdit(parent)
{
    setShowPassword(false);
}

//--------------------------------------------------------------------------------------------------
PasswordEdit::~PasswordEdit()
{
    clear();
}

//--------------------------------------------------------------------------------------------------
void PasswordEdit::setShowPassword(bool enable)
{
    if (enable)
    {
        setEchoMode(QLineEdit::Normal);
        setInputMethodHints(Qt::ImhNone);
    }
    else
    {
        setEchoMode(QLineEdit::Password);
        setInputMethodHints(Qt::ImhHiddenText | Qt::ImhSensitiveData |
                            Qt::ImhNoAutoUppercase | Qt::ImhNoPredictiveText);
    }
}

//--------------------------------------------------------------------------------------------------
bool PasswordEdit::isShowPassword() const
{
    return echoMode() == QLineEdit::Normal;
}

//--------------------------------------------------------------------------------------------------
void PasswordEdit::setShowPasswordButtonVisible(bool visible)
{
    if (visible == !show_password_button_.isNull())
        return;

    if (visible)
    {
        show_password_button_ = new QToolButton(this);
        show_password_button_->setIcon(GuiApplication::svgIcon(":/img/show-password.svg"));
        show_password_button_->setCheckable(true);
        show_password_button_->setAutoRaise(true);
        show_password_button_->setCursor(Qt::ArrowCursor);
        show_password_button_->setFocusPolicy(Qt::NoFocus);
        show_password_button_->setToolButtonStyle(Qt::ToolButtonIconOnly);

        connect(show_password_button_, &QToolButton::toggled, this, &PasswordEdit::setShowPassword);

        // Reserve space on the right edge so the text does not run under the button.
        int frame_width = style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
        QSize button_size = show_password_button_->sizeHint();
        setTextMargins(0, 0, button_size.width() + frame_width, 0);

        show_password_button_->show();
        // Trigger initial positioning.
        QResizeEvent dummy(size(), size());
        resizeEvent(&dummy);
    }
    else
    {
        delete show_password_button_;
        setTextMargins(0, 0, 0, 0);
    }
}

//--------------------------------------------------------------------------------------------------
void PasswordEdit::setPassword(const SecureString& password)
{
    QLineEdit::setText(password.toString());
}

//--------------------------------------------------------------------------------------------------
SecureString PasswordEdit::password() const
{
    return SecureString(QLineEdit::text());
}

//--------------------------------------------------------------------------------------------------
void PasswordEdit::clear()
{
    qsizetype length = QLineEdit::text().length();
    if (length > 0)
    {
        // Overwrite with zeros via setText. Qt's internal QString gets replaced wholesale - the
        // previous buffer is released to the allocator without being wiped, so this is a partial
        // mitigation rather than a guaranteed zeroize. It does ensure that the buffer Qt currently
        // owns no longer contains the password before clear() detaches it.
        QLineEdit::setText(QString(length, QChar(0)));
    }
    QLineEdit::clear();
}

//--------------------------------------------------------------------------------------------------
void PasswordEdit::resizeEvent(QResizeEvent* event)
{
    QLineEdit::resizeEvent(event);

    if (show_password_button_)
    {
        int frame_width = style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
        QSize button_size = show_password_button_->sizeHint();
        show_password_button_->move(rect().right() - frame_width - button_size.width(),
                                    (rect().bottom() - button_size.height() + 1) / 2);
    }
}
