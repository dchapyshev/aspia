//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "router/keygen/main_window.h"

#include "base/crypto/key_pair.h"

#include <QClipboard>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>

namespace router {

MainWindow::MainWindow()
{
    ui.setupUi(this);
    setWindowFlag(Qt::WindowMaximizeButtonHint, false);

    connect(ui.button_copy_private_key, &QPushButton::released, this, &MainWindow::onCopyPrivateKey);
    connect(ui.button_save_private_key, &QPushButton::released, this, &MainWindow::onSavePrivateKey);
    connect(ui.button_copy_public_key, &QPushButton::released, this, &MainWindow::onCopyPublicKey);
    connect(ui.button_save_public_key, &QPushButton::released, this, &MainWindow::onSavePublicKey);
    connect(ui.button_generate, &QPushButton::released, this, &MainWindow::onGenerate);
}

MainWindow::~MainWindow() = default;

void MainWindow::onCopyPrivateKey()
{
    copyToClipboard(ui.edit_private_key->toPlainText());
}

void MainWindow::onSavePrivateKey()
{
    saveToFile(ui.edit_private_key->toPlainText());
}

void MainWindow::onCopyPublicKey()
{
    copyToClipboard(ui.edit_public_key->toPlainText());
}

void MainWindow::onSavePublicKey()
{
    saveToFile(ui.edit_public_key->toPlainText());
}

void MainWindow::onGenerate()
{
    base::KeyPair key_pair = base::KeyPair::create(base::KeyPair::Type::X25519);
    if (!key_pair.isValid())
    {
        QMessageBox::warning(this,
                             tr("Warning"),
                             tr("Error generating keys."),
                             QMessageBox::Ok);
        return;
    }

    ui.edit_private_key->setPlainText(
        QString::fromStdString(base::toHex(key_pair.privateKey())));
    ui.edit_public_key->setPlainText(
        QString::fromStdString(base::toHex(key_pair.publicKey())));
}

void MainWindow::copyToClipboard(const QString& text)
{
    if (text.isEmpty())
        return;

    QClipboard* clipboard = QApplication::clipboard();
    if (!clipboard)
        return;

    clipboard->setText(text);
}

void MainWindow::saveToFile(const QString& text)
{
    if (text.isEmpty())
        return;

    QString file_path =
        QFileDialog::getSaveFileName(this,
                                     tr("Text File"),
                                     QDir::homePath(),
                                     tr("Text File (*.txt)"));
    if (file_path.isEmpty())
        return;

    QFile file(file_path);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::warning(this,
                             tr("Warning"),
                             tr("Error saving file: ").arg(file.errorString()),
                             QMessageBox::Ok);
        return;
    }

    if (file.write(text.toUtf8()) == -1)
    {
        QMessageBox::warning(this,
                             tr("Warning"),
                             tr("Error saving file: ").arg(file.errorString()),
                             QMessageBox::Ok);
    }
}

} // namespace router
