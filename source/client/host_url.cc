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

#include "client/host_url.h"

#include <QUrl>
#include <QUrlQuery>

#include "proto/peer.h"

namespace {

const char kScheme[] = "aspia";
const char kSchemePrefix[] = "aspia://";

// The authority part is a fixed keyword and the record references are passed in the query.
// Numbers must not be placed in the authority part: QUrl normalizes authority hosts as IP
// addresses in all inet_aton forms, so a numeric value would silently turn into an IPv4
// address.
const char kConnectAuthority[] = "connect";

const char kDataParam[] = "d";

const char kEntryParam[] = "entry";
const char kRouterParam[] = "router";
const char kHostParam[] = "host";
const char kSessionParam[] = "session";

//--------------------------------------------------------------------------------------------------
bool isValidSessionType(int value)
{
    switch (value)
    {
        case proto::peer::SESSION_TYPE_DESKTOP:
        case proto::peer::SESSION_TYPE_FILE_TRANSFER:
        case proto::peer::SESSION_TYPE_SYSTEM_INFO:
        case proto::peer::SESSION_TYPE_CHAT:
        case proto::peer::SESSION_TYPE_TERMINAL:
            return true;

        default:
            return false;
    }
}

//--------------------------------------------------------------------------------------------------
qint64 parseId(const QString& str)
{
    bool ok = false;
    qint64 id = str.toLongLong(&ok);
    return (ok && id > 0) ? id : -1;
}

//--------------------------------------------------------------------------------------------------
QString makeUrl(const QUrlQuery& query)
{
    QByteArray payload = query.toString(QUrl::FullyEncoded).toUtf8().toBase64(
        QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    return kSchemePrefix + QString(kConnectAuthority) + '?' + kDataParam + '=' + payload;
}

} // namespace

//--------------------------------------------------------------------------------------------------
HostUrl::HostUrl()
    : host_id_(kInvalidHostId),
      session_type_(proto::peer::SESSION_TYPE_DESKTOP)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
// static
bool HostUrl::isHostUrl(const QString& str)
{
    return str.startsWith(kSchemePrefix, Qt::CaseInsensitive);
}

//--------------------------------------------------------------------------------------------------
// static
HostUrl HostUrl::fromString(const QString& str)
{
    HostUrl result;

    QUrl url(str);
    if (!url.isValid() || url.scheme() != kScheme || url.host() != kConnectAuthority)
        return result;

    QString token = QUrlQuery(url).queryItemValue(kDataParam);
    if (token.isEmpty())
        return result;

    // Lenient base64 decoding is enough: any corruption is caught by the payload validation
    // below.
    QUrlQuery query(QString::fromUtf8(
        QByteArray::fromBase64(token.toLatin1(), QByteArray::Base64UrlEncoding)));

    QString entry = query.queryItemValue(kEntryParam);
    QString router = query.queryItemValue(kRouterParam);
    QString host = query.queryItemValue(kHostParam);

    // The link must reference either a local address book entry or a host record from a
    // router address book.
    if (entry.isEmpty() == (router.isEmpty() && host.isEmpty()))
        return result;

    qint64 entry_id = -1;
    qint64 router_id = -1;
    HostId host_id = kInvalidHostId;

    if (!entry.isEmpty())
    {
        entry_id = parseId(entry);
        if (entry_id <= 0)
            return result;
    }
    else
    {
        router_id = parseId(router);
        if (router_id <= 0 || !isHostId(host))
            return result;

        host_id = stringToHostId(host);
        if (host_id == kInvalidHostId)
            return result;
    }

    proto::peer::SessionType session_type = proto::peer::SESSION_TYPE_DESKTOP;
    QString session = query.queryItemValue(kSessionParam);
    if (!session.isEmpty())
    {
        bool ok = false;
        int value = session.toInt(&ok);
        if (!ok || !isValidSessionType(value))
            return result;

        session_type = static_cast<proto::peer::SessionType>(value);
    }

    result.valid_ = true;
    result.entry_id_ = entry_id;
    result.router_id_ = router_id;
    result.host_id_ = host_id;
    result.session_type_ = session_type;
    return result;
}

//--------------------------------------------------------------------------------------------------
// static
QString HostUrl::stringForEntry(qint64 entry_id, proto::peer::SessionType session_type)
{
    if (entry_id <= 0)
        return QString();

    QUrlQuery query;
    query.addQueryItem(kEntryParam, QString::number(entry_id));
    query.addQueryItem(kSessionParam, QString::number(session_type));

    return makeUrl(query);
}

//--------------------------------------------------------------------------------------------------
// static
QString HostUrl::stringForRouterHost(qint64 router_id, HostId host_id, proto::peer::SessionType session_type)
{
    if (router_id <= 0 || host_id == kInvalidHostId)
        return QString();

    QUrlQuery query;
    query.addQueryItem(kRouterParam, QString::number(router_id));
    query.addQueryItem(kHostParam, hostIdToString(host_id));
    query.addQueryItem(kSessionParam, QString::number(session_type));

    return makeUrl(query);
}
