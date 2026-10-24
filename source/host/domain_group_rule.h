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

#ifndef HOST_DOMAIN_GROUP_RULE_H
#define HOST_DOMAIN_GROUP_RULE_H

#include <QString>
#include <QVector>

// One row of the "domain_groups" table: the account |id| (a group, or a single user) gets |sessions|.
//
// Rules are matched by the identifier only, never by a name, so that a group or a user with the same
// name in another (trusted) domain or on the computer itself never matches:
//   - Windows: a SID ("S-1-5-21-...-1105");
//   - Linux: a GID ("1234") or a single user ("uid:1234").
// |group| is the name as it was resolved when the rule was saved ("CORP\Aspia Operators"); it is
// shown to the administrator only. Rules saved by older versions have only |group| and an empty
// |id|; they are resolved when used (see resolveDomainAccount()) until the settings are saved again.
struct DomainGroupRule
{
    enum class Kind
    {
        GROUP = 0, // |id| is a group: matches the groups of the account.
        USER  = 1  // |id| is a single account: matches only the account itself.
    };

    qint64 entry_id = 0;
    QString group;
    QString id;
    Kind kind = Kind::GROUP;
    quint32 sessions = 0; // Bitmask of proto::peer::SessionType.
};

using DomainGroupRuleList = QVector<DomainGroupRule>;

#endif // HOST_DOMAIN_GROUP_RULE_H
