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

#ifndef COMMON_UI_DOWNLOAD_DIALOG_H
#define COMMON_UI_DOWNLOAD_DIALOG_H

#include <QFile>
#include <QPointer>
#include <QThread>

#include "common/http_file_downloader.h"
#include "ui_download_dialog.h"

namespace common {

class DownloadDialog final : public QDialog
{
    Q_OBJECT

public:
    DownloadDialog(const QString& url, QFile& file, QWidget* parent = nullptr);
    ~DownloadDialog() final;

private slots:
    void onFileDownloaderError(int error_code);
    void onFileDownloaderCompleted();
    void onFileDownloaderProgress(int percentage);

private:
    Ui::DownloadDialog ui;

    std::unique_ptr<HttpFileDownloader> downloader_ = nullptr;
    QFile& file_;

    Q_DISABLE_COPY(DownloadDialog)
};

} // namespace common

#endif // COMMON_UI_DOWNLOAD_DIALOG_H
