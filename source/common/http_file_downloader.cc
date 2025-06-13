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

#include "common/http_file_downloader.h"

#include "base/logging.h"
#include "base/net/curl_util.h"

namespace common {

//--------------------------------------------------------------------------------------------------
HttpFileDownloader::HttpFileDownloader(QObject* parent)
    : QThread(parent)
{
    LOG(LS_INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
HttpFileDownloader::~HttpFileDownloader()
{
    LOG(LS_INFO) << "Dtor";
    interrupted_.store(true, std::memory_order_relaxed);
    wait();
}

//--------------------------------------------------------------------------------------------------
void HttpFileDownloader::setUrl(const QString& url)
{
    url_ = url;
}

//--------------------------------------------------------------------------------------------------
const QByteArray& HttpFileDownloader::data() const
{
    return data_;
}

//--------------------------------------------------------------------------------------------------
static int debugFunc(
    CURL* /* handle */, curl_infotype type, char* data, size_t size, void* /* clientp */)
{
    switch (type)
    {
        case CURLINFO_TEXT:
        {
            std::string_view message(data, size);

            if (message.ends_with("\n"))
                message = message.substr(0, message.size() - 1);
            if (message.ends_with("\r"))
                message = message.substr(0, message.size() - 1);

            LOG(LS_INFO) << message;
        }
        break;

        default:
            break;
    }

    return 0;
}

//--------------------------------------------------------------------------------------------------
void HttpFileDownloader::run()
{
    LOG(LS_INFO) << "Starting http file downloader:" << url_;
    interrupted_.store(false, std::memory_order_relaxed);

    base::ScopedCURL curl;

    QByteArray url = url_.toUtf8();

    curl_easy_setopt(curl.get(), CURLOPT_URL, url.data());
    curl_easy_setopt(curl.get(), CURLOPT_NOPROGRESS, 0);
    curl_easy_setopt(curl.get(), CURLOPT_MAXREDIRS, 15);
    curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1);

    long verify_peer = 1;
    if (qEnvironmentVariableIsSet("ASPIA_NO_VERIFY_TLS_PEER"))
    {
        LOG(LS_INFO) << "ASPIA_NO_VERIFY_TLS_PEER defined";
        verify_peer = 0;
    }

    curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYPEER, verify_peer);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, writeDataCallback);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, this);
    curl_easy_setopt(curl.get(), CURLOPT_XFERINFOFUNCTION, progressCallback);
    curl_easy_setopt(curl.get(), CURLOPT_XFERINFODATA, this);
    curl_easy_setopt(curl.get(), CURLOPT_VERBOSE, 1);
    curl_easy_setopt(curl.get(), CURLOPT_DEBUGFUNCTION, debugFunc);

    base::ScopedCURLM multi_curl;
    curl_multi_add_handle(multi_curl.get(), curl.get());

    CURLMcode error_code = CURLM_OK;
    int still_running = 1;

    do
    {
        error_code = curl_multi_perform(multi_curl.get(), &still_running);
        if (!error_code)
        {
            // Wait for activity, timeout or "nothing".
            error_code = curl_multi_poll(multi_curl.get(), nullptr, 0, 1000, nullptr);
        }

        if (error_code)
        {
            LOG(LS_ERROR) << "curl_multi_poll failed:" << curl_multi_strerror(error_code)
                          << " (" << error_code << ")";
            break;
        }

        if (interrupted_.load(std::memory_order_relaxed))
        {
            LOG(LS_INFO) << "Downloading canceled";
            break;
        }
    }
    while (still_running);

    curl_multi_remove_handle(multi_curl.get(), curl.get());

    if (!interrupted_.load(std::memory_order_relaxed))
    {
        if (error_code != CURLM_OK)
        {
            emit sig_downloadError(error_code);
        }
        else
        {
            LOG(LS_INFO) << "Download is finished:" << data_.size() << "bytes";
            emit sig_downloadCompleted();
        }
    }

    LOG(LS_INFO) << "run END";
}

//--------------------------------------------------------------------------------------------------
// static
size_t HttpFileDownloader::writeDataCallback(
    void* ptr, size_t size, size_t nmemb, HttpFileDownloader* self)
{
    size_t result = 0;

    if (self)
    {
        if (self->interrupted_.load(std::memory_order_relaxed))
        {
            LOG(LS_INFO) << "Interrupted by user";
            return 0;
        }

        result = size * nmemb;
        self->data_.append(reinterpret_cast<char*>(ptr), static_cast<QByteArray::size_type>(result));
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
// static
int HttpFileDownloader::progressCallback(
    HttpFileDownloader* self, double dltotal, double dlnow, double /* ultotal */, double /* ulnow */)
{
    if (self && !self->interrupted_.load(std::memory_order_relaxed))
    {
        int percentage = 0;
        if (dltotal > 0)
            percentage = static_cast<int>((dlnow * 100) / dltotal);

        emit self->sig_downloadProgress(percentage);
    }

    return 0;
}

} // namespace common
