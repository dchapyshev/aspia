//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/environment.h"
#include "build/version.h"
#include "base/logging.h"

#include <curl/curl.h>

namespace common {

UpdateCheckerImpl::UpdateCheckerImpl(QObject* parent)
    : QObject(parent)
{
    LOG(LS_INFO) << "Ctor";
}

UpdateCheckerImpl::~UpdateCheckerImpl()
{
    LOG(LS_INFO) << "Dtor";
}

void UpdateCheckerImpl::setUpdateServer(const QString& update_server)
{
    update_server_ = update_server;
}

void UpdateCheckerImpl::setPackageName(const QString& package_name)
{
    package_name_ = package_name;
}

static size_t writeDataFunc(void* ptr, size_t size, size_t nmemb, QByteArray* data)
{
    data->append(reinterpret_cast<const char*>(ptr), static_cast<int>(size * nmemb));
    return size * nmemb;
}

void UpdateCheckerImpl::start()
{
    QByteArray response;

    CURL* curl = curl_easy_init();
    if (!curl)
    {
        LOG(LS_WARNING) << "curl_easy_init failed";
    }
    else
    {
        base::Version current_version(ASPIA_VERSION_MAJOR, ASPIA_VERSION_MINOR, ASPIA_VERSION_PATCH);

        std::string url(update_server_.toStdString());
        url += "/update.php?";
        url += "package=" + package_name_.toStdString();
        url += '&';
        url += "version=" + current_version.toString(3);

        LOG(LS_INFO) << "Start checking for updates. Url: " << url;

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
        curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 15);

        long verify_peer = 1L;
        if (base::Environment::has("ASPIA_NO_VERIFY_TLS_PEER"))
            verify_peer = 0L;

        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, verify_peer);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeDataFunc);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        CURLcode ret = curl_easy_perform(curl);
        if (ret != CURLE_OK)
        {
            LOG(LS_WARNING) << "curl_easy_perform failed: " << ret;
            response.clear();
        }
        else
        {
            LOG(LS_INFO) << "Checking is finished: " << response.toStdString();
        }

        curl_easy_cleanup(curl);
    }

    emit finished(response);
}

} // namespace common
