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

#include "base/net/proxy_settings.h"

#include <QDataStream>
#include <QUrl>

#if defined(Q_OS_WINDOWS)
#include <qt_windows.h>
#include <winhttp.h>
#endif // defined(Q_OS_WINDOWS)

#include "base/logging.h"

namespace {

#if defined(Q_OS_WINDOWS)
const quint16 kDefaultHttpPort = 80;
const quint16 kDefaultSocksPort = 1080;
#endif // defined(Q_OS_WINDOWS)

//--------------------------------------------------------------------------------------------------
#if defined(Q_OS_WINDOWS)
// An entry of the list is "host", "host:port" or either of them after a scheme such as "http://".
ProxySettings proxyFromEntry(const QString& entry, ProxySettings::Type type, quint16 default_port)
{
    QString authority = entry;

    const qsizetype scheme_end = authority.indexOf("://");
    if (scheme_end != -1)
        authority.remove(0, scheme_end + 3);

    const QUrl url("//" + authority, QUrl::StrictMode);
    if (!url.isValid() || url.host().isEmpty())
    {
        LOG(ERROR) << "Invalid proxy in the system settings:" << entry;
        return ProxySettings();
    }

    ProxySettings proxy;
    proxy.enabled = true;
    proxy.type = type;
    proxy.address = url.host();
    proxy.port = static_cast<quint16>(url.port(default_port));
    return proxy;
}
#endif // defined(Q_OS_WINDOWS)

} // namespace

//--------------------------------------------------------------------------------------------------
// static
ProxySettings ProxySettings::systemSettings()
{
#if defined(Q_OS_WINDOWS)
    using GetIEProxyConfigFunc = BOOL(WINAPI*)(WINHTTP_CURRENT_USER_IE_PROXY_CONFIG* config);

    // Resolved once, the library stays loaded for the rest of the process.
    static const GetIEProxyConfigFunc get_proxy_config = []() -> GetIEProxyConfigFunc
    {
        // Only from the system directory. The portable host is often started from a download folder,
        // where a library of the same name could lie.
        HMODULE module = LoadLibraryExW(L"winhttp.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!module)
        {
            PLOG(ERROR) << "LoadLibraryExW failed";
            return nullptr;
        }

        GetIEProxyConfigFunc function = reinterpret_cast<GetIEProxyConfigFunc>(
            GetProcAddress(module, "WinHttpGetIEProxyConfigForCurrentUser"));
        if (!function)
            PLOG(ERROR) << "GetProcAddress failed";

        return function;
    }();

    if (!get_proxy_config)
        return ProxySettings();

    WINHTTP_CURRENT_USER_IE_PROXY_CONFIG config;
    memset(&config, 0, sizeof(config));

    if (!get_proxy_config(&config))
    {
        PLOG(ERROR) << "WinHttpGetIEProxyConfigForCurrentUser failed";
        return ProxySettings();
    }

    QString list;
    if (config.lpszProxy)
    {
        list = QString::fromWCharArray(config.lpszProxy);
        GlobalFree(config.lpszProxy);
    }

    if (config.lpszProxyBypass)
        GlobalFree(config.lpszProxyBypass);
    if (config.lpszAutoConfigUrl)
        GlobalFree(config.lpszAutoConfigUrl);

    // The list holds one address for every protocol or an address per protocol ("https=host:port"),
    // separated by semicolons or spaces.
    const QStringList entries = list.simplified().replace(' ', ';').split(';', Qt::SkipEmptyParts);

    QString common;
    QString http;
    QString https;
    QString socks;

    for (const QString& entry : entries)
    {
        const qsizetype separator = entry.indexOf('=');
        if (separator == -1)
        {
            common = entry;
            continue;
        }

        const QString protocol = entry.left(separator).toLower();
        const QString address = entry.mid(separator + 1);

        if (protocol == "https")
            https = address;
        else if (protocol == "http")
            http = address;
        else if (protocol == "socks")
            socks = address;
    }

    // Our channels are tunnels, and the proxy Windows opens tunnels through is the one for https.
    // Windows speaks only the fourth version of SOCKS.
    if (!common.isEmpty())
        return proxyFromEntry(common, Type::HTTP, kDefaultHttpPort);
    if (!https.isEmpty())
        return proxyFromEntry(https, Type::HTTP, kDefaultHttpPort);
    if (!http.isEmpty())
        return proxyFromEntry(http, Type::HTTP, kDefaultHttpPort);
    if (!socks.isEmpty())
        return proxyFromEntry(socks, Type::SOCKS4, kDefaultSocksPort);
#endif // defined(Q_OS_WINDOWS)

    return ProxySettings();
}

//--------------------------------------------------------------------------------------------------
// static
QString ProxySettings::proxyArgument(const ProxySettings& proxy)
{
    QByteArray buffer;

    QDataStream stream(&buffer, QIODevice::WriteOnly);
    stream << proxy.enabled << static_cast<quint32>(proxy.type) << proxy.address << proxy.port << proxy.user
           << proxy.password.toString();

    return QString::fromLatin1(buffer.toBase64(QByteArray::Base64UrlEncoding));
}

//--------------------------------------------------------------------------------------------------
// static
std::optional<ProxySettings> ProxySettings::proxyFromArgument(const QString& value)
{
    QByteArray::FromBase64Result decoded = QByteArray::fromBase64Encoding(
        value.toLatin1(), QByteArray::Base64UrlEncoding | QByteArray::AbortOnBase64DecodingErrors);
    if (!decoded)
        return std::nullopt;

    ProxySettings proxy;
    quint32 type = 0;
    QString password;

    QDataStream stream(decoded.decoded);
    stream >> proxy.enabled >> type >> proxy.address >> proxy.port >> proxy.user >> password;

    if (stream.status() != QDataStream::Ok || !stream.atEnd() ||
        type > static_cast<quint32>(Type::SOCKS5H))
    {
        return std::nullopt;
    }

    proxy.type = static_cast<Type>(type);
    proxy.password = SecureString(std::move(password));
    return proxy;
}
