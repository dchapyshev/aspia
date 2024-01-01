//
// Aspia Project
// Copyright (C) 2016-2024 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "common/update_checker.h"

#include "base/environment.h"
#include "base/logging.h"
#include "base/version.h"
#include "base/memory/byte_array.h"
#include "base/net/curl_util.h"
#include "base/strings/unicode.h"
#include "build/build_config.h"

namespace common {

class UpdateChecker::Runner : public std::enable_shared_from_this<Runner>
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

    void onFinished(const base::ByteArray& response)
    {
        if (!owner_task_runner_->belongsToCurrentThread())
        {
            owner_task_runner_->postTask(std::bind(&Runner::onFinished, shared_from_this(), response));
            return;
        }

        if (delegate_)
        {
            delegate_->onUpdateCheckedFinished(response);
            delegate_ = nullptr;
        }
    }

private:
    std::shared_ptr<base::TaskRunner> owner_task_runner_;
    Delegate* delegate_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(Runner);
};

//--------------------------------------------------------------------------------------------------
UpdateChecker::UpdateChecker()
{
    LOG(LS_INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
UpdateChecker::~UpdateChecker()
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
void UpdateChecker::setUpdateServer(std::u16string_view update_server)
{
    update_server_ = update_server;
}

//--------------------------------------------------------------------------------------------------
void UpdateChecker::setPackageName(std::u16string_view package_name)
{
    package_name_ = package_name;
}

//--------------------------------------------------------------------------------------------------
void UpdateChecker::start(std::shared_ptr<base::TaskRunner> owner_task_runner, Delegate* delegate)
{
    LOG(LS_INFO) << "Starting update checker";
    runner_ = std::make_shared<Runner>(std::move(owner_task_runner), delegate);
    thread_.start(std::bind(&UpdateChecker::run, this));
}

//--------------------------------------------------------------------------------------------------
static size_t writeDataFunc(void* ptr, size_t size, size_t nmemb, base::ByteArray* buffer)
{
    size_t append_size = size * nmemb;
    base::append(buffer, ptr, append_size);
    return append_size;
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
void UpdateChecker::run()
{
    LOG(LS_INFO) << "run BEGIN";

    std::u16string os;

#if defined(OS_WIN)
    os = u"windows";
#elif defined(OS_LINUX)
    os = u"linux";
#elif defined(OS_MAC)
    os = u"macosx";
#else
#error Unknown OS
#endif

    std::u16string arch;

#if defined(ARCH_CPU_X86_64)
    arch = u"x86_64";
#elif defined(ARCH_CPU_X86)
    arch = u"x86";
#elif defined(ARCH_CPU_ARMEL)
    arch = u"arm";
#elif defined(ARCH_CPU_ARM64)
    arch = u"arm64";
#else
#error Unknown architecture
#endif

    const base::Version& version = base::Version::kCurrentShortVersion;

    std::u16string unicode_url(update_server_);
    unicode_url += u"/update.php?";
    unicode_url += u"package=" + package_name_;
    unicode_url += u'&';
    unicode_url += u"version=" + version.toString(3);

    if (!os.empty())
    {
        unicode_url += u'&';
        unicode_url += u"os=" + os;
    }

    if (!arch.empty())
    {
        unicode_url += u'&';
        unicode_url += u"arch=" + arch;
    }

    std::string url = base::local8BitFromUtf16(unicode_url);

    LOG(LS_INFO) << "Start checking for updates. Url: " << url;

    base::ScopedCURL curl;
    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_NOPROGRESS, 1);
    curl_easy_setopt(curl.get(), CURLOPT_MAXREDIRS, 15);
    curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(curl.get(), CURLOPT_VERBOSE, 1);
    curl_easy_setopt(curl.get(), CURLOPT_DEBUGFUNCTION, debugFunc);

    long verify_peer = 1;
    if (base::Environment::has("ASPIA_NO_VERIFY_TLS_PEER"))
    {
        LOG(LS_INFO) << "ASPIA_NO_VERIFY_TLS_PEER defined";
        verify_peer = 0;
    }

    base::ByteArray response;

    curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYPEER, verify_peer);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, writeDataFunc);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response);

    base::ScopedCURLM multi_curl;
    curl_multi_add_handle(multi_curl.get(), curl.get());

    int still_running = 1;
    do
    {
        CURLMcode error_code = curl_multi_perform(multi_curl.get(), &still_running);
        if (!error_code)
        {
            // Wait for activity, timeout or "nothing".
            error_code = curl_multi_poll(multi_curl.get(), nullptr, 0, 1000, nullptr);
        }

        if (error_code)
        {
            LOG(LS_ERROR) << "curl_multi_poll failed: " << curl_multi_strerror(error_code)
                          << " (" << error_code << ")";
            response.clear();
            break;
        }

        if (thread_.isStopping())
        {
            LOG(LS_INFO) << "Update check canceled";
            response.clear();
            break;
        }
    }
    while (still_running);

    curl_multi_remove_handle(multi_curl.get(), curl.get());

    if (!thread_.isStopping())
    {
        LOG(LS_INFO) << "Checking is finished: " << base::toStdString(response);
        if (runner_)
            runner_->onFinished(response);
    }

    LOG(LS_INFO) << "run END";
}

} // namespace common
