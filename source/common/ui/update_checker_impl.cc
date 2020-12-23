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

#include "common/ui/update_checker_impl.h"

#include "build/version.h"
#include "qt_base/qt_logging.h"

#include <QCoreApplication>
#include <QEvent>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>

namespace common {

UpdateCheckerImpl::UpdateCheckerImpl(QObject* parent)
    : QObject(parent)
{
    // Nothing
}

UpdateCheckerImpl::~UpdateCheckerImpl() = default;

void UpdateCheckerImpl::setUpdateServer(const QString& update_server)
{
    update_server_ = update_server;
}

void UpdateCheckerImpl::setPackageName(const QString& package_name)
{
    package_name_ = package_name;
}

void UpdateCheckerImpl::start()
{
    network_manager_ = new QNetworkAccessManager(this);

    // Only "http"->"http", "http"->"https" or "https"->"https" redirects are allowed.
    network_manager_->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);

    connect(network_manager_, &QNetworkAccessManager::finished, [this](QNetworkReply* reply)
    {
        if (reply->error())
        {
            LOG(LS_WARNING) << "Error checking for updates: " << reply->errorString();
            emit finished(QByteArray());
        }
        else
        {
            emit finished(reply->readAll());
        }

        reply->deleteLater();
    });

    base::Version current_version(ASPIA_VERSION_MAJOR, ASPIA_VERSION_MINOR, ASPIA_VERSION_PATCH);
    QUrl url(update_server_);

    url.setPath(QStringLiteral("/update.php"));
    url.setQuery(QUrlQuery(
        QString("package=%1&version=%2")
        .arg(package_name_)
        .arg(QString::fromStdString(current_version.toString()))));

    network_manager_->get(QNetworkRequest(url));
}

} // namespace common
