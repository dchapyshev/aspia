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

#include "base/net/curl_util.h"

#include "base/logging.h"

#if defined(Q_OS_ANDROID)
#include <QJniEnvironment>
#include <QJniObject>
#elif defined(Q_OS_LINUX)
#include <QFile>
#endif

namespace {

#if defined(Q_OS_ANDROID)

//--------------------------------------------------------------------------------------------------
// Wraps one certificate in the armor curl expects to find around it.
QByteArray toPem(const QByteArray& certificate)
{
    const qsizetype kLineLength = 64;

    QByteArray base64 = certificate.toBase64();
    QByteArray pem = "-----BEGIN CERTIFICATE-----\n";

    for (qsizetype i = 0; i < base64.size(); i += kLineLength)
        pem += base64.mid(i, kLineLength) + "\n";

    return pem + "-----END CERTIFICATE-----\n";
}

//--------------------------------------------------------------------------------------------------
QByteArray systemCaBundle()
{
    QJniObject certificates = QJniObject::callStaticObjectMethod(
        "org/qtproject/qt/android/QtNative", "getSSLCertificates", "()[[B");

    QJniEnvironment env;

    if (env.checkAndClearExceptions() || !certificates.isValid())
        return QByteArray();

    jobjectArray array = certificates.object<jobjectArray>();
    jsize count = env->GetArrayLength(array);

    QByteArray result;

    for (jsize i = 0; i < count; ++i)
    {
        jbyteArray item = static_cast<jbyteArray>(env->GetObjectArrayElement(array, i));
        jsize size = env->GetArrayLength(item);
        jbyte* data = env->GetByteArrayElements(item, nullptr);

        result += toPem(QByteArray(reinterpret_cast<const char*>(data), size));

        env->ReleaseByteArrayElements(item, data, JNI_ABORT);
        env->DeleteLocalRef(item);
    }

    return result;
}

#elif defined(Q_OS_LINUX)

//--------------------------------------------------------------------------------------------------
QByteArray systemCaBundlePath()
{
    static const char* const kPaths[] =
    {
        "/etc/ssl/certs/ca-certificates.crt", // Debian, Ubuntu, Arch, Gentoo.
        "/etc/pki/tls/certs/ca-bundle.crt",   // Red Hat family.
        "/etc/ssl/ca-bundle.pem",             // openSUSE.
        "/etc/pki/tls/cacert.pem",
        "/etc/ssl/cert.pem"
    };

    for (const char* path : kPaths)
    {
        if (QFile::exists(QString::fromLatin1(path)))
            return QByteArray(path);
    }

    return QByteArray();
}

#endif // defined(Q_OS_ANDROID)

//--------------------------------------------------------------------------------------------------
const char* proxyScheme(ProxySettings::Type type)
{
    switch (type)
    {
        case ProxySettings::Type::HTTP: return "http";
        case ProxySettings::Type::HTTPS: return "https";
        case ProxySettings::Type::SOCKS4: return "socks4";
        case ProxySettings::Type::SOCKS4A: return "socks4a";
        case ProxySettings::Type::SOCKS5: return "socks5";
        case ProxySettings::Type::SOCKS5H: return "socks5h";
    }

    return "http";
}

} // namespace

//--------------------------------------------------------------------------------------------------
ScopedCURL::ScopedCURL()
    : curl_(curl_easy_init())
{
    CHECK(curl_);

#if defined(Q_OS_ANDROID)
    // Android keeps the certificates it trusts behind an interface of Java that curl cannot reach,
    // and the backend of Qt that reads them needs a shared OpenSSL this build does not carry, so
    // the trust manager of the system is asked directly. Building the bundle once is enough.
    static const QByteArray bundle = systemCaBundle();

    if (bundle.isEmpty())
    {
        LOG(ERROR) << "System has no known certificate authorities";
        return;
    }

    curl_blob blob;
    blob.data = const_cast<char*>(bundle.constData());
    blob.len = static_cast<size_t>(bundle.size());
    blob.flags = CURL_BLOB_COPY;

    // The roots for the host and for an HTTPS proxy are separate options in curl.
    curl_easy_setopt(curl_, CURLOPT_CAINFO_BLOB, &blob);
    curl_easy_setopt(curl_, CURLOPT_PROXY_CAINFO_BLOB, &blob);
#elif defined(Q_OS_LINUX)
    static const QByteArray bundle_path = systemCaBundlePath();

    if (bundle_path.isEmpty())
    {
        LOG(ERROR) << "System has no known certificate authorities";
        return;
    }

    curl_easy_setopt(curl_, CURLOPT_CAINFO, bundle_path.constData());
    curl_easy_setopt(curl_, CURLOPT_PROXY_CAINFO, bundle_path.constData());
#elif defined(Q_OS_WINDOWS)
    // The OpenSSL backend of curl knows no roots on Windows by itself, this makes it read the ROOT and CA
    // stores of the system. The Schannel backend reads them anyway and ignores the option.
    curl_easy_setopt(curl_, CURLOPT_SSL_OPTIONS, static_cast<long>(CURLSSLOPT_NATIVE_CA));
    curl_easy_setopt(curl_, CURLOPT_PROXY_SSL_OPTIONS, static_cast<long>(CURLSSLOPT_NATIVE_CA));
#endif // defined(Q_OS_ANDROID)
}

//--------------------------------------------------------------------------------------------------
ScopedCURL::~ScopedCURL()
{
    curl_easy_cleanup(curl_);
}

//--------------------------------------------------------------------------------------------------
// static
QUrl ScopedCURL::proxyUrl(const ProxySettings& settings)
{
    // curl takes the type of the proxy from the scheme of its address.
    QUrl url;
    url.setScheme(QString::fromLatin1(proxyScheme(settings.type)));
    url.setHost(settings.address);
    url.setPort(settings.port);

    if (!url.isValid() || url.host().isEmpty())
        return QUrl();

    return url;
}

//--------------------------------------------------------------------------------------------------
bool ScopedCURL::setProxy(const ProxySettings& settings)
{
    if (!settings.enabled)
        return true;

    const QUrl url = proxyUrl(settings);
    if (!url.isValid())
    {
        LOG(ERROR) << "Invalid address of the proxy:" << settings.address << ":" << settings.port;
        return false;
    }

    curl_easy_setopt(curl_, CURLOPT_PROXY, url.toEncoded().constData());

    if (settings.type == ProxySettings::Type::HTTPS && qEnvironmentVariableIsSet("ASPIA_NO_VERIFY_TLS_PEER"))
    {
        LOG(INFO) << "ASPIA_NO_VERIFY_TLS_PEER defined, the certificate of the proxy is not verified";
        curl_easy_setopt(curl_, CURLOPT_PROXY_SSL_VERIFYPEER, 0L);
    }

    if (!settings.user.isEmpty())
    {
        curl_easy_setopt(curl_, CURLOPT_PROXYAUTH, static_cast<long>(CURLAUTH_ANY));
        curl_easy_setopt(curl_, CURLOPT_PROXYUSERNAME, settings.user.toUtf8().constData());
        curl_easy_setopt(curl_, CURLOPT_PROXYPASSWORD, settings.password.toUtf8().constData());
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
ScopedCURLM::ScopedCURLM()
    : curlm_(curl_multi_init())
{
    CHECK(curlm_);
}

//--------------------------------------------------------------------------------------------------
ScopedCURLM::~ScopedCURLM()
{
    curl_multi_cleanup(curlm_);
}

//--------------------------------------------------------------------------------------------------
ScopedCurlSlist::~ScopedCurlSlist()
{
    curl_slist_free_all(list_);
}

//--------------------------------------------------------------------------------------------------
bool ScopedCurlSlist::append(const char* string)
{
    curl_slist* list = curl_slist_append(list_, string);
    if (!list)
        return false;

    list_ = list;
    return true;
}
