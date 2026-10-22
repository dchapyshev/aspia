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

#include "base/peer/host_key.h"

#include "base/crypto/generic_hash.h"

//--------------------------------------------------------------------------------------------------
// static
QString HostKey::fingerprint(const QByteArray& public_key)
{
    if (public_key.size() != kPublicKeySize)
        return QString();

    const QString hex =
        QString::fromLatin1(GenericHash::hash(GenericHash::SHA256, public_key).toHex()).toUpper();

    QString result;
    result.reserve(hex.size() + hex.size() / 4);

    for (qsizetype i = 0; i < hex.size(); i += 4)
    {
        if (!result.isEmpty())
            result += QLatin1Char(' ');
        result += hex.mid(i, 4);
    }

    return result;
}
