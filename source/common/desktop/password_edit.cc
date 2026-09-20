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

#include <QAction>
#include <QStyle>

#include "base/crypto/secure_string.h"
#include "base/gui_application.h"

namespace {

//--------------------------------------------------------------------------------------------------
// The crossed out eye says that the password is on screen and that the button puts it back.
QIcon stateIcon(const QWidget* widget, bool show_password)
{
    int size = widget->style()->pixelMetric(QStyle::PM_SmallIconSize);

    return GuiApplication::svgIcon(
        show_password ? ":/img/hide-password.svg" : ":/img/show-password.svg", QSize(size, size));
}

} // namespace

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

    if (show_password_action_)
    {
        show_password_action_->setIcon(stateIcon(this, enable));
        show_password_action_->setChecked(enable);
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
    if (visible == !show_password_action_.isNull())
        return;

    if (visible)
    {
        // An action of the line edit itself. A button placed inside the field is drawn by the
        // platform as a button, with a frame of its own, and on macOS that looks like a control
        // dropped into the text.
        show_password_action_ = addAction(stateIcon(this, isShowPassword()), TrailingPosition);
        show_password_action_->setCheckable(true);
        show_password_action_->setChecked(isShowPassword());

        connect(show_password_action_, &QAction::toggled, this, &PasswordEdit::setShowPassword);
    }
    else
    {
        delete show_password_action_;
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
