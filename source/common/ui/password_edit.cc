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

#include "common/ui/password_edit.h"

#include "base/crypto/secure_string.h"

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
