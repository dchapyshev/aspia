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

#ifndef CLIENT_HOST_URL_H
#define CLIENT_HOST_URL_H

#include <QString>

#include "base/peer/host_id.h"

namespace proto::peer {
enum SessionType : int;
} // namespace proto::peer

// Link to an address book record that can be opened from outside the application. The link
// carries only record references, never addresses or credentials, so it is useless outside
// the machines where the referenced records exist:
//   aspia://connect?d=ZT00MiZzPTE
// The payload is a base64url-encoded query string ("e=42&s=1" or
// "r=<router GUID>&h=123456789&s=1"), where |e| is a local address book entry ID, |r| is
// the persistent router GUID (portable between machines, resolved against the GUIDs cached
// in the local router records), |h| is the record key in that router's address book and |s|
// is a proto::peer::SessionType value.
class HostUrl final
{
public:
    HostUrl();

    // Quick check that the string looks like an aspia:// link (used to distinguish links
    // from other command line arguments without full parsing).
    static bool isHostUrl(const QString& str);

    // Parses a link. On any error the returned object is not valid.
    static HostUrl fromString(const QString& str);

    // Builds a link to a local address book entry.
    static QString stringForEntry(qint64 entry_id, proto::peer::SessionType session_type);

    // Builds a link to a host from a router address book.
    static QString stringForRouterHost(const QString& router_guid, HostId host_id, proto::peer::SessionType session_type);

    bool isValid() const { return valid_; }
    bool isRouterHost() const { return !router_guid_.isEmpty(); }

    // Local address book entry ID. -1 for router links.
    qint64 entryId() const { return entry_id_; }

    // Router GUID and host record key. Empty / kInvalidHostId for local links.
    const QString& routerGuid() const { return router_guid_; }
    HostId hostId() const { return host_id_; }

    proto::peer::SessionType sessionType() const { return session_type_; }

private:
    bool valid_ = false;
    qint64 entry_id_ = -1;
    QString router_guid_;
    HostId host_id_;
    proto::peer::SessionType session_type_;
};

#endif // CLIENT_HOST_URL_H
