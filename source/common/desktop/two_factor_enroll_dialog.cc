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

#include "common/desktop/two_factor_enroll_dialog.h"

#include <QDialogButtonBox>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

#include <qrcodegen.hpp>

#include "ui_two_factor_enroll_dialog.h"

namespace {

//--------------------------------------------------------------------------------------------------
// Renders |qr| into a square QPixmap sized to fit |max_size| pixels, with a four-module quiet
// zone around the matrix (the QR spec calls for at least four modules of empty border).
QPixmap renderQrPixmap(const qrcodegen::QrCode& qr, int max_size)
{
    const int size = qr.getSize();
    const int border = 4;
    const int modules = size + border * 2;
    const int module_px = std::max(1, max_size / modules);
    const int image_px = module_px * modules;

    QImage image(image_px, image_px, QImage::Format_RGB32);
    image.fill(Qt::white);

    QPainter painter(&image);
    painter.setBrush(Qt::black);
    painter.setPen(Qt::NoPen);

    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            if (qr.getModule(x, y))
            {
                painter.drawRect((x + border) * module_px,
                                 (y + border) * module_px,
                                 module_px, module_px);
            }
        }
    }
    painter.end();
    return QPixmap::fromImage(image);
}

} // namespace

//--------------------------------------------------------------------------------------------------
TwoFactorEnrollDialog::TwoFactorEnrollDialog(const QString& otpauth_uri, QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::TwoFactorEnrollDialog>())
{
    ui->setupUi(this);

    // Pull the Base32 secret out of the URI's query string and insert a space every four
    // characters so the user can transcribe it by hand when QR scanning is unavailable. The
    // authenticator app strips whitespace on input.
    const QString base32 = QUrlQuery(QUrl(otpauth_uri)).queryItemValue("secret");
    QString grouped;
    grouped.reserve(base32.size() + base32.size() / 4);
    for (qsizetype i = 0; i < base32.size(); ++i)
    {
        if (i > 0 && (i % 4) == 0)
            grouped += QLatin1Char(' ');
        grouped += base32[i];
    }

    ui->edit_secret->setText(grouped);

    // Medium error-correction matches what GA/MS Authenticator emit by default; it absorbs
    // ~15% damage which is more than enough for an on-screen, never-printed image.
    const qrcodegen::QrCode qr =
        qrcodegen::QrCode::encodeText(otpauth_uri.toUtf8().constData(), qrcodegen::QrCode::Ecc::MEDIUM);
    ui->label_qr->setPixmap(renderQrPixmap(qr, 220));

    ui->edit_code->setValidator(
        new QRegularExpressionValidator(QRegularExpression("\\d*"), ui->edit_code));
    ui->edit_code->setFocus();

    connect(ui->buttonbox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->buttonbox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QTimer::singleShot(0, this, [this]() { setFixedSize(sizeHint()); });
}

//--------------------------------------------------------------------------------------------------
TwoFactorEnrollDialog::~TwoFactorEnrollDialog() = default;

//--------------------------------------------------------------------------------------------------
QString TwoFactorEnrollDialog::code() const
{
    return ui->edit_code->text().trimmed();
}
