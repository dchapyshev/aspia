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

#include "base/xml_settings.h"

#include <QDataStream>
#include <QRect>
#include <QXmlStreamReader>

namespace base {

namespace {

const QString kEmptyString;
const QString kExtension = QStringLiteral("conf");
const QString kSettingsElement = QStringLiteral("settings");
const QString kGroupElement = QStringLiteral("group");
const QString kValueElement = QStringLiteral("value");
const QString kNameAttribute = QStringLiteral("name");
const QString kTypeAttribute = QStringLiteral("type");
const QString kInvalidType = QStringLiteral("invalid");
const QString kByteArrayType = QStringLiteral("bytes");
const QString kRectType = QStringLiteral("rect");
const QString kSizeType = QStringLiteral("size");
const QString kPointType = QStringLiteral("point");
const QString kVariantType = QStringLiteral("variant");

//--------------------------------------------------------------------------------------------------
QStringView variantToType(const QVariant& value)
{
    QStringView result;

    switch (value.typeId())
    {
        case QMetaType::Bool:
        case QMetaType::Int:
        case QMetaType::UInt:
        case QMetaType::LongLong:
        case QMetaType::ULongLong:
        case QMetaType::Double:
        case QMetaType::QString:
        case QMetaType::QKeySequence:
            result = kEmptyString;
            break;

        case QMetaType::QByteArray:
            result = kByteArrayType;
            break;

        case QMetaType::QRect:
            result = kRectType;
            break;

        case QMetaType::QSize:
            result = kSizeType;
            break;

        case QMetaType::QPoint:
            result = kPointType;
            break;

        case QMetaType::UnknownType:
            result = kInvalidType;
            break;

        default:
            result = kVariantType;
            break;
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
QString variantToString(const QVariant& value)
{
    QString result;

    switch (value.typeId())
    {
        case QMetaType::Bool:
        case QMetaType::Int:
        case QMetaType::UInt:
        case QMetaType::LongLong:
        case QMetaType::ULongLong:
        case QMetaType::Double:
        case QMetaType::QString:
        case QMetaType::QKeySequence:
            result = value.toString();
            break;

        case QMetaType::QByteArray:
            result = value.toByteArray().toHex();
            break;

        case QMetaType::QRect:
        {
            QRect rect = value.toRect();
            result = QString::asprintf("%d %d %d %d",
                rect.left(), rect.top(), rect.width(), rect.height());
        }
        break;

        case QMetaType::QSize:
        {
            QSize size = value.toSize();
            result = QString::asprintf("%d %d", size.width(), size.height());
        }
        break;

        case QMetaType::QPoint:
        {
            QPoint point = value.toPoint();
            result = QString::asprintf("%d %d", point.x(), point.y());
        }
        break;

        case QMetaType::UnknownType:
            break;

        default:
        {
            QByteArray buffer;

            {
                QDataStream stream(&buffer, QIODevice::WriteOnly);
                stream.setVersion(QDataStream::Qt_5_15);
                stream << value;
            }

            result = buffer.toHex();
        }
        break;
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
QVariant stringToVariant(QStringView value, const QString& type)
{
    if (type == kByteArrayType)
    {
        return QVariant(QByteArray::fromHex(value.toLatin1()));
    }
    else if (type == kVariantType)
    {
        QByteArray buffer = QByteArray::fromHex(value.toLatin1());

        QDataStream stream(&buffer, QIODevice::ReadOnly);
        stream.setVersion(QDataStream::Qt_5_15);

        QVariant result;
        stream >> result;
        return result;
    }
    else if (type == kRectType)
    {
        QList<QStringView> list = value.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (list.size() == 4)
            return QVariant(QRect(list[0].toInt(), list[1].toInt(), list[2].toInt(), list[3].toInt()));
    }
    else if (type == kSizeType)
    {
        QList<QStringView> list = value.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (list.size() == 2)
            return QVariant(QSize(list[0].toInt(), list[1].toInt()));
    }
    else if (type == kPointType)
    {
        QList<QStringView> list = value.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (list.size() == 2)
            return QVariant(QPoint(list[0].toInt(), list[1].toInt()));
    }
    else if (type == kInvalidType)
    {
        return QVariant();
    }

    return QVariant(value.toString());
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
QSettings::Format XmlSettings::format()
{
    static QSettings::Format xml_format =
        QSettings::registerFormat(kExtension,
                                  XmlSettings::readFunc,
                                  XmlSettings::writeFunc,
                                  Qt::CaseSensitive);

    return xml_format;
}

//--------------------------------------------------------------------------------------------------
// static
bool XmlSettings::readFunc(QIODevice& device, QSettings::SettingsMap& map)
{
    QXmlStreamReader xml(&device);
    QStringList segments;
    QString type;

    bool settings_found = false;

    while (!xml.atEnd() && !xml.hasError())
    {
        switch (xml.readNext())
        {
            case QXmlStreamReader::StartElement:
            {
                if (xml.name() == kSettingsElement)
                {
                    settings_found = true;
                    continue;
                }

                if (!settings_found)
                    return false;

                QXmlStreamAttributes attributes = xml.attributes();

                QString name = attributes.value(kNameAttribute).toString();
                if (name.isEmpty())
                    return false;

                segments.emplace_back(name);
                type = attributes.value(kTypeAttribute).toString();
            }
            break;

            case QXmlStreamReader::EndElement:
            {
                if (!segments.isEmpty())
                    segments.removeLast();
            }
            break;

            case QXmlStreamReader::Characters:
            {
                if (xml.isWhitespace())
                    continue;

                QString key;

                for (int i = 0; i < segments.size(); ++i)
                {
                    if (i != 0)
                        key += QLatin1Char('/');
                    key += segments.at(i);
                }

                if (key.isEmpty())
                    return false;

                map[key] = stringToVariant(xml.text().toString(), type);
            }
            break;

            default:
                break;
        }
    }

    return !xml.hasError();
}

//--------------------------------------------------------------------------------------------------
// static
bool XmlSettings::writeFunc(QIODevice& device, const QSettings::SettingsMap& map)
{
    QXmlStreamWriter xml(&device);

    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeStartElement(kSettingsElement); // <settings>

    QList<QStringView> oldSegments;

    for (auto it = map.cbegin(), it_end = map.cend(); it != it_end; ++it)
    {
        QList<QStringView> segments = QStringView(it.key()).split(QLatin1Char('/'), Qt::SkipEmptyParts);
        const QVariant& value = it.value();
        int count = 0;

        while (count < oldSegments.size() && segments.at(count) == oldSegments.at(count))
            ++count;

        for (int i = oldSegments.size() - 1; i >= count; --i)
            xml.writeEndElement();

        for (int i = count; i < segments.size(); ++i)
        {
            if (i == segments.size() - 1)
                xml.writeStartElement(kValueElement);
            else
                xml.writeStartElement(kGroupElement);

            xml.writeAttribute(kNameAttribute, segments.at(i));
        }

        QStringView type = variantToType(value);
        if (!type.isEmpty())
            xml.writeAttribute(kTypeAttribute, type);

        xml.writeCharacters(variantToString(value));

        oldSegments = std::move(segments);
    }

    for (int i = 0; i < oldSegments.size(); ++i)
        xml.writeEndElement();

    xml.writeEndElement(); // </settings>
    xml.writeEndDocument();

    return !xml.hasError();
}

} // namespace base
