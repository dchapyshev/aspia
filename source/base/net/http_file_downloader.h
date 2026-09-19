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

#ifndef BASE_NET_HTTP_FILE_DOWNLOADER_H
#define BASE_NET_HTTP_FILE_DOWNLOADER_H

#include <QFile>
#include <QThread>

#include <curl/curl.h>

class HttpFileDownloader final : public QThread
{
    Q_OBJECT

public:
    // Downloads |url| into |file_path|, replacing whatever is in that file.
    HttpFileDownloader(const QString& url, const QString& file_path, QObject* parent = nullptr);
    ~HttpFileDownloader();

signals:
    void sig_downloadError(const QString& error);
    void sig_downloadCompleted();
    void sig_downloadProgress(int percentage);

protected:
    // QThread implementation.
    void run() final;

private:
    static size_t writeDataCallback(void* ptr, size_t size, size_t nmemb, HttpFileDownloader* self);
    static int progressCallback(HttpFileDownloader* self, curl_off_t dltotal, curl_off_t dlnow,
                                curl_off_t ultotal, curl_off_t ulnow);

    std::atomic_bool interrupted_ { false };
    const QString url_;
    const QString file_path_;
    QFile file_;

    Q_DISABLE_COPY_MOVE(HttpFileDownloader)
};

#endif // BASE_NET_HTTP_FILE_DOWNLOADER_H
