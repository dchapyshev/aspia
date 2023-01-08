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

#include "common/ui/download_impl.h"

#include "base/environment.h"
#include "base/logging.h"

#include <curl/curl.h>

namespace common {

DownloadImpl::DownloadImpl(const QString& url, QObject* parent)
    : QThread(parent),
      url_(url.toStdString())
{
    LOG(LS_INFO) << "Ctor";
}

DownloadImpl::~DownloadImpl()
{
    LOG(LS_INFO) << "Dtor";

    if (isRunning())
    {
        requestInterruption();
        wait();
    }
}

void DownloadImpl::run()
{
    CURL* curl = curl_easy_init();
    if (!curl)
    {
        LOG(LS_WARNING) << "curl_easy_init";
        emit errorOccurred(tr("Initialization error"));
    }
    else
    {
        curl_easy_setopt(curl, CURLOPT_URL, url_.c_str());
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);
        curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 100);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

        long verify_peer = 1;
        if (base::Environment::has("ASPIA_NO_VERIFY_TLS_PEER"))
            verify_peer = 0;

        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, verify_peer);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeDataCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progressCallback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, this);

        CURLcode ret = curl_easy_perform(curl);
        if (ret != CURLE_OK)
        {
            if (!isInterruptionRequested())
            {
                LOG(LS_WARNING) << "curl_easy_perform failed: " << ret;
                emit errorOccurred(QString::number(ret));
            }
        }
        else
        {
            if (!isInterruptionRequested())
            {
                LOG(LS_INFO) << "Download is finished: " << data_.size() << " bytes";
                emit downloadCompleted(data_);
            }
        }

        curl_easy_cleanup(curl);
    }
}

// static
size_t DownloadImpl::writeDataCallback(void* ptr, size_t size, size_t nmemb, DownloadImpl* self)
{
    size_t result = 0;

    if (self)
    {
        if (self->isInterruptionRequested())
        {
            LOG(LS_INFO) << "Interrupted by user";
            return 0;
        }

        result = size * nmemb;
        self->data_.append(reinterpret_cast<const char*>(ptr), static_cast<int>(result));
    }

    return result;
}

// static
int DownloadImpl::progressCallback(
    DownloadImpl* self, double dltotal, double dlnow, double /* ultotal */, double /* ulnow */)
{
    if (self && !self->isInterruptionRequested())
    {
        int percentage = 0;
        if (dltotal > 0)
            percentage = static_cast<int>((dlnow * 100) / dltotal);
        emit self->progress(percentage);
    }

    return 0;
}

} // namespace common
