//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "common/ui/download_dialog.h"

#include <QAbstractButton>
#include <QFile>
#include <QPushButton>
#include <QMessageBox>

namespace common {

DownloadDialog::DownloadDialog(const QString& url, QFile& file, QWidget* parent)
    : QDialog(parent),
      file_(file)
{
    ui.setupUi(this);

    impl_ = new DownloadImpl(url, this);

    connect(impl_, &DownloadImpl::finished, this, [this]()
    {
        reject();
        close();
    });

    connect(impl_, &DownloadImpl::errorOccurred, this, [this](const QString& error)
    {
        QMessageBox::warning(this,
                             tr("Warning"),
                             tr("An error occurred while downloading the update: %1")
                                .arg(error),
                             QMessageBox::Ok);
        reject();
        close();
    }, Qt::QueuedConnection);

    connect(impl_, &DownloadImpl::downloadCompleted, this, [this](const QByteArray& data)
    {
        file_.write(data);
        file_.flush();

        accept();
        close();
    }, Qt::QueuedConnection);

    connect(impl_, &DownloadImpl::progress, this, [this](int percentage)
    {
        ui.progress_bar->setValue(percentage);
    }, Qt::QueuedConnection);

    impl_->start();

    QPushButton* cancel_button = ui.button_box->button(QDialogButtonBox::StandardButton::Cancel);
    if (cancel_button)
        cancel_button->setText(tr("Cancel"));

    connect(ui.button_box, &QDialogButtonBox::clicked, this, [this](QAbstractButton* /* button */)
    {
        reject();
        close();
    });
}

} // namespace common
