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

#include "updater/update_checker.h"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>

#include "base/qt_logging.h"
#include "build/version.h"

namespace aspia {

UpdateChecker::UpdateChecker(QObject* parent)
    : QObject(parent),
      network_manager_(this)
{
    // Only "http"->"http", "http"->"https" or "https"->"https" redirects are allowed.
    network_manager_.setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);

    connect(&network_manager_, &QNetworkAccessManager::finished, [this](QNetworkReply* reply)
    {
        if (reply->error())
        {
            LOG(LS_WARNING) << "Error checking for updates: " << reply->errorString();
            emit finished(UpdateInfo());
        }
        else
        {
            emit finished(UpdateInfo::fromXml(reply->readAll()));
        }

        reply->deleteLater();
    });
}

UpdateChecker::~UpdateChecker() = default;

void UpdateChecker::checkForUpdates(const QString& update_server, const QString& package_name)
{
    QVersionNumber current_version(ASPIA_VERSION_MAJOR,
                                   ASPIA_VERSION_MINOR,
                                   ASPIA_VERSION_PATCH);
    QUrl url(update_server);

    url.setPath("/update.php");
    url.setQuery(QUrlQuery(
        QString("package=%1&version=%2")
        .arg(package_name)
        .arg(current_version.toString())));

    network_manager_.get(QNetworkRequest(url));
}

} // namespace aspia
