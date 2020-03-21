//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "updater/update_info.h"

#include "qt_base/qt_logging.h"

#include <QtXml>

namespace updater {

namespace {

QString parseElement(QXmlStreamReader& xml)
{
    if (xml.tokenType() != QXmlStreamReader::StartElement)
        return QString();

    if (xml.readNext() != QXmlStreamReader::Characters)
        return QString();

    return xml.text().toString();
}

} // namespace

// static
UpdateInfo UpdateInfo::fromXml(const QByteArray& buffer)
{
    UpdateInfo update_info;

    QXmlStreamReader xml(buffer);
    while (!xml.atEnd() && !xml.hasError())
    {
        QXmlStreamReader::TokenType token = xml.readNext();

        if (token == QXmlStreamReader::StartDocument)
            continue;

        if (token == QXmlStreamReader::StartElement)
        {
            if (xml.name() == QLatin1String("update"))
                continue;

            while (xml.tokenType() != QXmlStreamReader::EndElement)
            {
                if (xml.tokenType() == QXmlStreamReader::StartElement)
                {
                    if (xml.name() == QLatin1String("version"))
                    {
                        update_info.version_ = base::Version(parseElement(xml).toStdString());
                    }
                    else if (xml.name() == QLatin1String("description"))
                    {
                        update_info.description_ = parseElement(xml);
                    }
                    else if (xml.name() == QLatin1String("url"))
                    {
                        update_info.url_ = parseElement(xml);
                    }
                    else
                    {
                        // Unknown field. Ignore.
                    }
                }

                xml.readNext();
            }
        }
    }

    if (xml.hasError())
    {
        LOG(LS_WARNING) << "Error parsing XML: " << xml.errorString();
    }
    else
    {
        update_info.valid_ = true;
    }

    return update_info;
}

bool UpdateInfo::hasUpdate() const
{
    return !version_.isValid() && !url_.isEmpty();
}

} // namespace updater
