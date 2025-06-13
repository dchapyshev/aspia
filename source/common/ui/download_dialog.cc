//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/logging.h"

#include <QAbstractButton>
#include <QFile>
#include <QPushButton>
#include <QMessageBox>

namespace common {

//--------------------------------------------------------------------------------------------------
DownloadDialog::DownloadDialog(const QString& url, QFile& file, QWidget* parent)
    : QDialog(parent),
      downloader_(std::make_unique<HttpFileDownloader>()),
      file_(file)
{
    LOG(LS_INFO) << "Ctor";
    ui.setupUi(this);

    QPushButton* cancel_button = ui.button_box->button(QDialogButtonBox::StandardButton::Cancel);
    if (cancel_button)
        cancel_button->setText(tr("Cancel"));

    connect(ui.button_box, &QDialogButtonBox::clicked, this, [this](QAbstractButton* /* button */)
    {
        LOG(LS_INFO) << "[ACTION] Cancel downloading";
        reject();
        close();
    });

    connect(downloader_.get(), &HttpFileDownloader::sig_downloadError,
            this, &DownloadDialog::onFileDownloaderError);
    connect(downloader_.get(), &HttpFileDownloader::sig_downloadCompleted,
            this, &DownloadDialog::onFileDownloaderCompleted);
    connect(downloader_.get(), &HttpFileDownloader::sig_downloadProgress,
            this, &DownloadDialog::onFileDownloaderProgress);

    downloader_->setUrl(url);
    downloader_->start();
}

//--------------------------------------------------------------------------------------------------
DownloadDialog::~DownloadDialog()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void DownloadDialog::onFileDownloaderError(int error_code)
{
    LOG(LS_ERROR) << "Error while downloading update:" << error_code;
    QMessageBox::warning(this,
                         tr("Warning"),
                         tr("An error occurred while downloading the update: %1").arg(error_code),
                         QMessageBox::Ok);
    reject();
    close();
}

//--------------------------------------------------------------------------------------------------
void DownloadDialog::onFileDownloaderCompleted()
{
    LOG(LS_INFO) << "File downloaded";
    const QByteArray& buffer = downloader_->data();

    file_.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    file_.flush();

    accept();
    close();
}

//--------------------------------------------------------------------------------------------------
void DownloadDialog::onFileDownloaderProgress(int percentage)
{
    ui.progress_bar->setValue(percentage);
}

} // namespace common
