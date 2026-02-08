//
// SmartCafe Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "common/update_info.h"

#include <QXmlStreamReader>

#include "base/logging.h"

namespace common {

namespace {

static const int kMaxDescriptionLength = 4096;
static const int kMinUrlLength = 10;
static const int kMaxUrlLength = 256;

//--------------------------------------------------------------------------------------------------
QString parseElement(QXmlStreamReader& xml)
{
    if (xml.tokenType() != QXmlStreamReader::StartElement)
        return QString();

    if (xml.readNext() != QXmlStreamReader::Characters)
        return QString();

    return xml.text().toString();
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
UpdateInfo UpdateInfo::fromXml(const QByteArray& buffer)
{
    if (buffer.isEmpty())
    {
        LOG(INFO) << "Empty XML buffer";
        return UpdateInfo();
    }

    LOG(INFO) << "XML:" << QString::fromUtf8(buffer);

    if (!buffer.startsWith('<'))
    {
        // No updates or an error occurred on the update server.
        return UpdateInfo();
    }

    UpdateInfo update_info;

    QXmlStreamReader xml(buffer);
    while (!xml.atEnd() && !xml.hasError())
    {
        QXmlStreamReader::TokenType token = xml.readNext();

        if (token == QXmlStreamReader::StartDocument)
            continue;

        if (token != QXmlStreamReader::StartElement)
            continue;

        if (xml.name() == QLatin1String("update"))
            continue;

        while (xml.tokenType() != QXmlStreamReader::EndElement)
        {
            if (xml.tokenType() == QXmlStreamReader::StartElement)
            {
                if (xml.name() == QLatin1String("version"))
                {
                    update_info.version_ = QVersionNumber::fromString(parseElement(xml));
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

    if (xml.hasError())
    {
        LOG(ERROR) << "Error parsing XML:" << xml.errorString();
    }
    else if (update_info.description_.size() > kMaxDescriptionLength)
    {
        LOG(ERROR) << "Too many characters in description";
    }
    else if (update_info.url_.size() < kMinUrlLength || update_info.url_.size() > kMaxUrlLength)
    {
        LOG(ERROR) << "Incorrect number of characters in URL";
    }
    else
    {
        update_info.valid_ = true;
    }

    return update_info;
}

} // namespace common
