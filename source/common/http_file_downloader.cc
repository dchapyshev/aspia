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

#include "common/http_file_downloader.h"

#include "base/environment.h"
#include "base/logging.h"
#include "base/net/curl_util.h"

namespace common {

class HttpFileDownloader::Runner : public std::enable_shared_from_this<Runner>
{
public:
    Runner(std::shared_ptr<base::TaskRunner> owner_task_runner, Delegate* delegate)
        : owner_task_runner_(std::move(owner_task_runner)),
          delegate_(delegate)
    {
        DCHECK(owner_task_runner_);
        DCHECK(delegate_);
    }

    ~Runner()
    {
        dettach();
    }

    void dettach()
    {
        delegate_ = nullptr;
    }

    void onError(int error_code)
    {
        if (!owner_task_runner_->belongsToCurrentThread())
        {
            owner_task_runner_->postTask(std::bind(&Runner::onError, shared_from_this(), error_code));
            return;
        }

        if (delegate_)
        {
            delegate_->onFileDownloaderError(error_code);
            delegate_ = nullptr;
        }
    }

    void onCompleted()
    {
        if (!owner_task_runner_->belongsToCurrentThread())
        {
            owner_task_runner_->postTask(std::bind(&Runner::onCompleted, shared_from_this()));
            return;
        }

        if (delegate_)
        {
            delegate_->onFileDownloaderCompleted();
            delegate_ = nullptr;
        }
    }

    void onProgress(int percentage)
    {
        if (!owner_task_runner_->belongsToCurrentThread())
        {
            owner_task_runner_->postTask(std::bind(&Runner::onProgress, shared_from_this(), percentage));
            return;
        }

        if (delegate_)
            delegate_->onFileDownloaderProgress(percentage);
    }

private:
    std::shared_ptr<base::TaskRunner> owner_task_runner_;
    Delegate* delegate_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(Runner);
};

//--------------------------------------------------------------------------------------------------
HttpFileDownloader::HttpFileDownloader()
{
    LOG(LS_INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
HttpFileDownloader::~HttpFileDownloader()
{
    LOG(LS_INFO) << "Dtor";

    if (runner_)
    {
        runner_->dettach();
        runner_.reset();
    }
    thread_.stop();
}

//--------------------------------------------------------------------------------------------------
void HttpFileDownloader::start(std::string_view url,
                               std::shared_ptr<base::TaskRunner> owner_task_runner,
                               Delegate* delegate)
{
    url_ = url;
    runner_ = std::make_shared<Runner>(std::move(owner_task_runner), delegate);
    thread_.start(std::bind(&HttpFileDownloader::run, this));
}

//--------------------------------------------------------------------------------------------------
void HttpFileDownloader::run()
{
    base::ScopedCURL curl;

    curl_easy_setopt(curl.get(), CURLOPT_URL, url_.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_NOPROGRESS, 0);
    curl_easy_setopt(curl.get(), CURLOPT_MAXREDIRS, 15);
    curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1);

    long verify_peer = 1;
    if (base::Environment::has("ASPIA_NO_VERIFY_TLS_PEER"))
        verify_peer = 0;

    curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYPEER, verify_peer);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, writeDataCallback);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, this);
    curl_easy_setopt(curl.get(), CURLOPT_XFERINFOFUNCTION, progressCallback);
    curl_easy_setopt(curl.get(), CURLOPT_XFERINFODATA, this);

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
            LOG(LS_ERROR) << "curl_multi_poll failed: " << error_code;
            break;
        }

        if (thread_.isStopping())
        {
            LOG(LS_INFO) << "Downloading canceled";
            break;
        }
    }
    while (still_running);

    curl_multi_remove_handle(multi_curl.get(), curl.get());

    if (!thread_.isStopping())
    {
        if (error_code != CURLM_OK)
        {
            if (runner_)
                runner_->onError(error_code);
        }
        else
        {
            LOG(LS_INFO) << "Download is finished: " << data_.size() << " bytes";
            if (runner_)
                runner_->onCompleted();
        }
    }
}

//--------------------------------------------------------------------------------------------------
// static
size_t HttpFileDownloader::writeDataCallback(
    void* ptr, size_t size, size_t nmemb, HttpFileDownloader* self)
{
    size_t result = 0;

    if (self)
    {
        if (self->thread_.isStopping())
        {
            LOG(LS_INFO) << "Interrupted by user";
            return 0;
        }

        result = size * nmemb;
        base::append(&self->data_, ptr, result);
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
// static
int HttpFileDownloader::progressCallback(
    HttpFileDownloader* self, double dltotal, double dlnow, double /* ultotal */, double /* ulnow */)
{
    if (self && !self->thread_.isStopping())
    {
        int percentage = 0;
        if (dltotal > 0)
            percentage = static_cast<int>((dlnow * 100) / dltotal);

        if (self->runner_)
            self->runner_->onProgress(percentage);
    }

    return 0;
}

} // namespace common
