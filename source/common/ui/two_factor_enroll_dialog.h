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

#ifndef COMMON_UI_TWO_FACTOR_ENROLL_DIALOG_H
#define COMMON_UI_TWO_FACTOR_ENROLL_DIALOG_H

#include <QDialog>

#include <memory>

namespace Ui {
class TwoFactorEnrollDialog;
} // namespace Ui

class TwoFactorEnrollDialog final : public QDialog
{
    Q_OBJECT

public:
    // |secret| is the raw shared secret (will be Base32-encoded for display).
    // |otpauth_uri| is the ready-made URI the authenticator app expects.
    TwoFactorEnrollDialog(const QByteArray& secret, const QString& otpauth_uri, QWidget* parent = nullptr);
    ~TwoFactorEnrollDialog() final;

    QString code() const;

private:
    std::unique_ptr<Ui::TwoFactorEnrollDialog> ui;
    Q_DISABLE_COPY_MOVE(TwoFactorEnrollDialog)
};

#endif // COMMON_UI_TWO_FACTOR_ENROLL_DIALOG_H
