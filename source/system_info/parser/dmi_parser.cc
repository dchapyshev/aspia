//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "system_info/parser/dmi_parser.h"

namespace aspia {

DmiParser::DmiParser(QObject* parent, const QString& uuid)
    : Parser(parent, uuid)
{
    // Nothing
}

// static
Parser* DmiParser::create(QObject* parent, const QString& uuid)
{
    return new DmiParser(parent, uuid);
}

QJsonObject DmiParser::toJson()
{
    // TODO
    return QJsonObject();
}

QDomElement DmiParser::toXml()
{
    // TODO
    return QDomElement();
}

QDomElement DmiParser::toHtml()
{
    // TODO
    return QDomElement();
}

QString DmiParser::toText()
{
    // TODO
    return QString();
}

void DmiParser::updateData() const
{
    // TODO
}

void DmiParser::readReply(const QString& uuid, const QByteArray& data)
{
    // TODO
}

} // namespace aspia
