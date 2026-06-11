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

#include "client/android/two_factor_dialog.h"

#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>

#include "common/android/button.h"
#include "common/android/label.h"
#include "common/android/line_edit.h"

namespace {

constexpr int kCodeLength = 6;
constexpr int kSecretGroupSize = 4;

//--------------------------------------------------------------------------------------------------
QString groupedSecret(const QString& otpauth_uri)
{
    const QString secret = QUrlQuery(QUrl(otpauth_uri)).queryItemValue("secret");

    // Insert a space every few characters so the key can be transcribed by hand. Authenticator
    // apps strip whitespace on input.
    QString grouped;
    grouped.reserve(secret.size() + secret.size() / kSecretGroupSize);
    for (qsizetype i = 0; i < secret.size(); ++i)
    {
        if (i > 0 && (i % kSecretGroupSize) == 0)
            grouped += QChar(' ');
        grouped += secret[i];
    }

    return grouped;
}

} // namespace

//--------------------------------------------------------------------------------------------------
TwoFactorDialog::TwoFactorDialog(const QString& otpauth_uri, QWidget* parent)
    : Dialog(parent),
      code_(new LineEdit(this))
{
    const bool enroll = !otpauth_uri.isEmpty();

    setTitle(tr("Two-Factor Authentication"));
    setText(enroll ? tr("Add the setup key below to an authenticator app, then enter the 6-digit"
                        " code it shows to confirm.")
                   : tr("Enter the 6-digit code from your authenticator app."));

    QVBoxLayout* content = contentLayout();

    if (enroll)
    {
        LineEdit* secret = new LineEdit(this);
        secret->setLabel(tr("Setup Key"));
        secret->setText(groupedSecret(otpauth_uri));
        secret->setReadOnly(true);
        content->addWidget(secret);
    }

    code_->setLabel(tr("Code"));
    code_->setMaxLength(kCodeLength);
    code_->setInputMethodHints(Qt::ImhDigitsOnly | Qt::ImhPreferNumbers);
    code_->setValidator(new QRegularExpressionValidator(QRegularExpression("\\d*"), code_));
    content->addWidget(code_);

    addButton(tr("Cancel"), Button::Role::TEXT);
    Button* ok = addButton(tr("OK"), Button::Role::FILLED);

    connect(ok, &Button::clicked, this, &TwoFactorDialog::accept);
}

//--------------------------------------------------------------------------------------------------
TwoFactorDialog::~TwoFactorDialog() = default;

//--------------------------------------------------------------------------------------------------
QString TwoFactorDialog::code() const
{
    return code_->text().trimmed();
}
