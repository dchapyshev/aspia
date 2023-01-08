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

#ifndef COMMON_UI_DOWNLOAD_IMPL_H
#define COMMON_UI_DOWNLOAD_IMPL_H

#include "base/macros_magic.h"

#include <QThread>

namespace common {

class DownloadImpl : public QThread
{
    Q_OBJECT

public:
    explicit DownloadImpl(const QString& url, QObject* parent = nullptr);
    ~DownloadImpl() override;

signals:
    void errorOccurred(const QString& error);
    void downloadCompleted(const QByteArray& data);
    void progress(int percentage);

protected:
    void run() override;

private:
    static size_t writeDataCallback(void* ptr, size_t size, size_t nmemb, DownloadImpl* self);
    static int progressCallback(
        DownloadImpl* self, double dltotal, double dlnow, double ultotal, double ulnow);

    std::string url_;
    QByteArray data_;

    DISALLOW_COPY_AND_ASSIGN(DownloadImpl);
};

} // namespace common

#endif // COMMON_UI_DOWNLOAD_IMPL_H
