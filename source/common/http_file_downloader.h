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

#ifndef COMMON_HTTP_FILE_DOWNLOADER_H
#define COMMON_HTTP_FILE_DOWNLOADER_H

#include <QByteArray>
#include <QThread>

namespace common {

class HttpFileDownloader final : public QThread
{
    Q_OBJECT

public:
    explicit HttpFileDownloader(QObject* parent = nullptr);
    ~HttpFileDownloader();

    void setUrl(const QString& url);
    const QByteArray& data() const;

signals:
    void sig_downloadError(int error_code);
    void sig_downloadCompleted();
    void sig_downloadProgress(int percentage);

protected:
    // QThread implementation.
    void run() final;

private:
    static size_t writeDataCallback(void* ptr, size_t size, size_t nmemb, HttpFileDownloader* self);
    static int progressCallback(
        HttpFileDownloader* self, double dltotal, double dlnow, double ultotal, double ulnow);

    std::atomic_bool interrupted_ { false };
    QString url_;
    QByteArray data_;

    Q_DISABLE_COPY(HttpFileDownloader)
};

} // namespace common

#endif // COMMON_HTTP_FILE_DOWNLOADER_H
