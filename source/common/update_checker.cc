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

#include "common/update_checker.h"

#include "base/logging.h"
#include "base/net/curl_util.h"

#include <QVersionNumber>

namespace common {

//--------------------------------------------------------------------------------------------------
UpdateChecker::UpdateChecker(QObject* parent)
    : QThread(parent)
{
    LOG(LS_INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
UpdateChecker::~UpdateChecker()
{
    LOG(LS_INFO) << "Dtor";

    interrupted_.store(true, std::memory_order_relaxed);
    wait();
}

//--------------------------------------------------------------------------------------------------
void UpdateChecker::setUpdateServer(const QString& update_server)
{
    update_server_ = update_server;
}

//--------------------------------------------------------------------------------------------------
void UpdateChecker::setPackageName(const QString& package_name)
{
    package_name_ = package_name;
}

//--------------------------------------------------------------------------------------------------
static size_t writeDataFunc(void* ptr, size_t size, size_t nmemb, QByteArray* buffer)
{
    size_t append_size = size * nmemb;
    buffer->append(reinterpret_cast<char*>(ptr), static_cast<QByteArray::size_type>(append_size));
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
    interrupted_.store(false, std::memory_order_relaxed);

    QString os;
#if defined(Q_OS_WINDOWS)
    os = "windows";
#elif defined(Q_OS_LINUX)
    os = "linux";
#elif defined(Q_OS_MACOS)
    os = "macosx";
#else
#error Unknown OS
#endif

    QString arch;
#if defined(Q_PROCESSOR_X86_64)
    arch = "x86_64";
#elif defined(Q_PROCESSOR_X86_32)
    arch = "x86";
#elif defined(Q_PROCESSOR_ARM_32)
    arch = "arm";
#elif defined(Q_PROCESSOR_ARM_64)
    arch = "arm64";
#else
#error Unknown architecture
#endif

    QVersionNumber version({ASPIA_VERSION_MAJOR, ASPIA_VERSION_MINOR, ASPIA_VERSION_PATCH});
    QString unicode_url(update_server_);

    unicode_url += "/update.php?";
    unicode_url += "package=" + package_name_;
    unicode_url += '&';
    unicode_url += "version=" + version.toString();

    if (!os.isEmpty())
    {
        unicode_url += '&';
        unicode_url += "os=" + os;
    }

    if (!arch.isEmpty())
    {
        unicode_url += '&';
        unicode_url += "arch=" + arch;
    }

    QByteArray url = unicode_url.toLocal8Bit();

    LOG(LS_INFO) << "Start checking for updates. Url: " << unicode_url;

    base::ScopedCURL curl;
    curl_easy_setopt(curl.get(), CURLOPT_URL, url.data());
    curl_easy_setopt(curl.get(), CURLOPT_NOPROGRESS, 1);
    curl_easy_setopt(curl.get(), CURLOPT_MAXREDIRS, 15);
    curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(curl.get(), CURLOPT_VERBOSE, 1);
    curl_easy_setopt(curl.get(), CURLOPT_DEBUGFUNCTION, debugFunc);

    long verify_peer = 1;
    if (qEnvironmentVariableIsSet("ASPIA_NO_VERIFY_TLS_PEER"))
    {
        LOG(LS_INFO) << "ASPIA_NO_VERIFY_TLS_PEER defined";
        verify_peer = 0;
    }

    QByteArray response;

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

        if (interrupted_.load(std::memory_order_relaxed))
        {
            LOG(LS_INFO) << "Update check canceled";
            response.clear();
            break;
        }
    }
    while (still_running);

    curl_multi_remove_handle(multi_curl.get(), curl.get());

    if (!interrupted_.load(std::memory_order_relaxed))
    {
        LOG(LS_INFO) << "Checking is finished: " << response.toStdString();
        emit sig_checkedFinished(response);
    }

    LOG(LS_INFO) << "run END";
}

} // namespace common
