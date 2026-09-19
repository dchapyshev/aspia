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

namespace {

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

} // namespace
#endif // defined(Q_OS_ANDROID)

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

    curl_easy_setopt(curl_, CURLOPT_CAINFO_BLOB, &blob);
#endif // defined(Q_OS_ANDROID)
}

//--------------------------------------------------------------------------------------------------
ScopedCURL::~ScopedCURL()
{
    curl_easy_cleanup(curl_);
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
