//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "network/http_downloader.h"

#include <QFile>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace aspia {

HttpDownloader::HttpDownloader(QObject* parent)
    : QObject(parent),
      manager_(this)
{
    // Only "http"->"http", "http"->"https" or "https"->"https" redirects are allowed.
    manager_.setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);

    connect(&manager_, &QNetworkAccessManager::finished, [this](QNetworkReply* reply)
    {
        if (reply->error())
        {
            emit errorOccurred(reply->errorString());
        }
        else
        {
            QFile file(file_path_);

            if (!file.open(QIODevice::WriteOnly))
            {
                emit errorOccurred(file.errorString());
            }
            else
            {
                file.write(reply->readAll());
                file.close();

                emit downloadFinished();
            }
        }

        reply->deleteLater();
    });
}

HttpDownloader::~HttpDownloader() = default;

void HttpDownloader::start(const QUrl& url, const QString& file_path)
{
    file_path_ = file_path;
    manager_.get(QNetworkRequest(url));
}

} // namespace aspia
