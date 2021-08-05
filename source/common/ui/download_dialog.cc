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

#include "common/ui/download_dialog.h"

#include <QAbstractButton>
#include <QFile>
#include <QMessageBox>
#include <QNetworkReply>

namespace common {

DownloadDialog::DownloadDialog(const QUrl& url, QFile& file, QWidget* parent)
    : QDialog(parent),
      network_manager_(this),
      file_(file)
{
    ui.setupUi(this);

    connect(ui.button_box, &QDialogButtonBox::clicked, [this](QAbstractButton* /* button */)
    {
        reject();
        close();
    });

    // Only "http"->"http", "http"->"https" or "https"->"https" redirects are allowed.
    network_manager_.setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);

    connect(&network_manager_, &QNetworkAccessManager::finished, [this](QNetworkReply* reply)
    {
        reject();

        if (reply->error())
        {
            QMessageBox::warning(this,
                                 tr("Warning"),
                                 tr("An error occurred while downloading the update: %1")
                                    .arg(reply->errorString()),
                                 QMessageBox::Ok);
        }
        else
        {
            file_.write(reply->readAll());
            accept();
        }

        reply->deleteLater();
        close();
    });

    QNetworkReply* reply = network_manager_.get(QNetworkRequest(url));

    connect(reply, &QNetworkReply::downloadProgress,
            [this](qint64 bytes_received, qint64 bytes_total)
    {
        if (bytes_total != 0)
        {
            int percentage = static_cast<int>((bytes_received * 100) / bytes_total);
            ui.progress_bar->setValue(percentage);
        }
    });
}

} // namespace common
