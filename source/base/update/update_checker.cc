//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/update/update_checker.h"

#include <QStandardPaths>
#include <QVersionNumber>

#include <optional>
#include <utility>

#include "base/build_config.h"
#include "base/logging.h"
#include "base/version_constants.h"
#include "base/crypto/signature.h"
#include "base/net/curl_util.h"

namespace {

const long kMaxFileSize = 1024 * 1024;

// Seconds. A server that takes the connection and then says nothing holds this thread until the
// check is canceled, and nothing cancels a check nobody is waiting for. The files read here are
// small, so the whole request is given a deadline of its own.
const long kConnectTimeout = 30;
const long kRequestTimeout = 60;

//--------------------------------------------------------------------------------------------------
QString serverUrl(const QString& channel)
{
    QString name = kStableUpdateChannel;

    if (channel == kBetaUpdateChannel || channel == kAlphaUpdateChannel)
        name = channel;

    return kUpdateServer + "/" + name;
}

//--------------------------------------------------------------------------------------------------
size_t writeDataFunc(void* ptr, size_t size, size_t nmemb, QByteArray* buffer)
{
    size_t append_size = size * nmemb;
    buffer->append(reinterpret_cast<char*>(ptr), static_cast<qsizetype>(append_size));
    return append_size;
}

//--------------------------------------------------------------------------------------------------
int debugFunc(CURL* /* handle */, curl_infotype type, char* data, size_t size, void* /* clientp */)
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

            LOG(TRACE) << message;
        }
        break;

        default:
            break;
    }

    return 0;
}

} // namespace

//--------------------------------------------------------------------------------------------------
UpdateChecker::UpdateChecker(const QString& channel, const QString& package, QObject* parent)
    : QThread(parent),
      server_(serverUrl(channel)),
      package_(package)
{
    LOG(TRACE) << "Ctor";

    for (QByteArrayView public_key : kUpdatePublicKeys)
        public_keys_.append(public_key.toByteArray());
}

//--------------------------------------------------------------------------------------------------
UpdateChecker::~UpdateChecker()
{
    LOG(TRACE) << "Dtor";

    interrupted_.store(true, std::memory_order_relaxed);
    wait();
}

//--------------------------------------------------------------------------------------------------
void UpdateChecker::setServerForTesting(const QString& server)
{
    server_ = server;
}

//--------------------------------------------------------------------------------------------------
void UpdateChecker::setPublicKeysForTesting(const QList<QByteArray>& public_keys)
{
    public_keys_ = public_keys;
}

//--------------------------------------------------------------------------------------------------
void UpdateChecker::run()
{
    LOG(TRACE) << "run BEGIN";
    interrupted_.store(false, std::memory_order_relaxed);
    check();
    LOG(TRACE) << "run END";
}

//--------------------------------------------------------------------------------------------------
void UpdateChecker::check()
{
    QString os;
#if defined(Q_OS_WINDOWS)
    os = "windows";
#elif defined(Q_OS_ANDROID)
    os = "android";
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

    QString format;
#if defined(Q_OS_WINDOWS)
    format = "msi";
#elif defined(Q_OS_ANDROID)
    format = "apk";
#elif defined(Q_OS_LINUX)
    if (!QStandardPaths::findExecutable("apt-get").isEmpty())
        format = "deb";
    else if (!QStandardPaths::findExecutable("dnf").isEmpty())
        format = "rpm";
#elif defined(Q_OS_MACOS)
    format = "pkg";
#endif

    QByteArray rules = downloadSigned(server_ + "/latest.json");
    if (interrupted_.load(std::memory_order_relaxed))
        return;

    if (rules.isEmpty())
    {
        emit sig_checkFailed();
        return;
    }

    std::optional<QVersionNumber> target_version = UpdateInfo::targetVersion(rules, kCurrentVersion);
    if (!target_version)
    {
        emit sig_checkFailed();
        return;
    }

    if (target_version->isNull() || *target_version <= kCurrentVersion)
    {
        LOG(INFO) << "No updates for version" << kCurrentVersion.toString();
        emit sig_checkFinished(UpdateInfo());
        return;
    }

    QByteArray manifest = downloadSigned(server_ + "/" + target_version->toString() + ".json");
    if (interrupted_.load(std::memory_order_relaxed))
        return;

    if (manifest.isEmpty())
    {
        emit sig_checkFailed();
        return;
    }

    std::optional<UpdateInfo> update_info = UpdateInfo::fromManifest(manifest, package_, os, arch, format);
    if (update_info && update_info->isValid() && update_info->version() != *target_version)
    {
        LOG(ERROR) << "Manifest of version" << target_version->toString()
                   << "carries version" << update_info->version().toString();
        update_info.reset();
    }

    if (!update_info)
    {
        emit sig_checkFailed();
        return;
    }

    emit sig_checkFinished(*update_info);
}

//--------------------------------------------------------------------------------------------------
QByteArray UpdateChecker::downloadSigned(const QString& url)
{
    QByteArray data = download(url);
    if (data.isEmpty())
        return QByteArray();

    QByteArray signature = Signature::untagged(download(url + ".sig"));
    if (signature.isEmpty())
        return QByteArray();

    for (const QByteArray& public_key : std::as_const(public_keys_))
    {
        if (Signature::verify(public_key, data, signature))
            return data;
    }

    LOG(ERROR) << "File is not signed with a key we know:" << url;
    return QByteArray();
}

//--------------------------------------------------------------------------------------------------
QByteArray UpdateChecker::download(const QString& unicode_url)
{
    LOG(INFO) << "Reading" << unicode_url;

    QByteArray url = unicode_url.toUtf8();

    ScopedCURL curl;
    curl_easy_setopt(curl.get(), CURLOPT_URL, url.data());
    curl_easy_setopt(curl.get(), CURLOPT_NOPROGRESS, 1);
    curl_easy_setopt(curl.get(), CURLOPT_MAXFILESIZE, kMaxFileSize);
    curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, kConnectTimeout);
    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, kRequestTimeout);
    curl_easy_setopt(curl.get(), CURLOPT_MAXREDIRS, 15);
    curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(curl.get(), CURLOPT_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(curl.get(), CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(curl.get(), CURLOPT_VERBOSE, 1);
    curl_easy_setopt(curl.get(), CURLOPT_DEBUGFUNCTION, debugFunc);

    long verify_peer = 1;
    if (qEnvironmentVariableIsSet("ASPIA_NO_VERIFY_TLS_PEER"))
    {
        LOG(INFO) << "ASPIA_NO_VERIFY_TLS_PEER defined";
        verify_peer = 0;
    }

    QByteArray response;

    curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYPEER, verify_peer);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, writeDataFunc);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response);

    ScopedCURLM multi_curl;
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
            LOG(ERROR) << "curl_multi_poll failed:" << curl_multi_strerror(error_code)
                       << "(" << error_code << ")";
            response.clear();
            break;
        }

        if (interrupted_.load(std::memory_order_relaxed))
        {
            LOG(INFO) << "Update check canceled";
            response.clear();
            break;
        }
    }
    while (still_running);

    // How the transfer itself ended is a message in the queue of the multi handle, and there is
    // only one transfer in it. Removing the easy handle takes the message away, so it is read first.
    CURLcode result = CURLE_OK;
    int messages_left = 0;
    CURLMsg* message = curl_multi_info_read(multi_curl.get(), &messages_left);

    if (message && message->msg == CURLMSG_DONE)
        result = message->data.result;

    long response_code = 0;
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &response_code);

    curl_multi_remove_handle(multi_curl.get(), curl.get());

    if (result != CURLE_OK)
    {
        LOG(ERROR) << "Transfer failed:" << curl_easy_strerror(result);
        response.clear();
    }
    else if (response_code != 200)
    {
        LOG(ERROR) << "Unexpected response code:" << response_code;
        response.clear();
    }

    return response;
}
