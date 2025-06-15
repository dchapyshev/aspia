//
// Aspia Project
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

#include "base/xml_settings.h"

#include <QDataStream>
#include <QRect>
#include <QXmlStreamReader>

namespace base {

namespace {

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
QString variantToType(const QVariant& value)
{
    QString result;

    switch (value.type())
    {
        case QVariant::Invalid:
            result = kInvalidType;
            break;

        case QVariant::Bool:
        case QVariant::Int:
        case QVariant::UInt:
        case QVariant::LongLong:
        case QVariant::ULongLong:
        case QVariant::Double:
        case QVariant::String:
        case QVariant::KeySequence:
            // Default type.
            break;

        case QVariant::ByteArray:
            result = kByteArrayType;
            break;

        case QVariant::Rect:
            result = kRectType;
            break;

        case QVariant::Size:
            result = kSizeType;
            break;

        case QVariant::Point:
            result = kPointType;
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

    switch (value.type())
    {
        case QVariant::Invalid:
            break;

        case QVariant::Bool:
        case QVariant::Int:
        case QVariant::UInt:
        case QVariant::LongLong:
        case QVariant::ULongLong:
        case QVariant::Double:
        case QVariant::String:
        case QVariant::KeySequence:
            result = value.toString();
            break;

        case QVariant::ByteArray:
            result = value.toByteArray().toHex();
            break;

        case QVariant::Rect:
        {
            QRect rect = value.toRect();
            result = QString::asprintf("%d %d %d %d",
                rect.left(), rect.top(), rect.width(), rect.height());
        }
        break;

        case QVariant::Size:
        {
            QSize size = value.toSize();
            result = QString::asprintf("%d %d", size.width(), size.height());
        }
        break;

        case QVariant::Point:
        {
            QPoint point = value.toPoint();
            result = QString::asprintf("%d %d", point.x(), point.y());
        }
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
QVariant stringToVariant(const QString& value, const QString& type)
{
    if (type == kByteArrayType)
    {
        return QVariant(QByteArray::fromHex(value.toUtf8()));
    }
    else if (type == kVariantType)
    {
        QByteArray buffer = QByteArray::fromHex(value.toUtf8());

        QDataStream stream(&buffer, QIODevice::ReadOnly);
        stream.setVersion(QDataStream::Qt_5_15);

        QVariant result;
        stream >> result;
        return result;
    }
    else if (type == kRectType)
    {
        QStringList list = value.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (list.size() == 4)
            return QVariant(QRect(list[0].toInt(), list[1].toInt(), list[2].toInt(), list[3].toInt()));
    }
    else if (type == kSizeType)
    {
        QStringList list = value.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (list.size() == 2)
            return QVariant(QSize(list[0].toInt(), list[1].toInt()));
    }
    else if (type == kPointType)
    {
        QStringList list = value.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (list.size() == 2)
            return QVariant(QPoint(list[0].toInt(), list[1].toInt()));
    }
    else if (type == kInvalidType)
    {
        return QVariant();
    }

    return QVariant(value);
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

                QString name = xml.attributes().value(kNameAttribute).toString();
                if (name.isEmpty())
                    return false;

                segments.append(name);
                type = xml.attributes().value(kTypeAttribute).toString();
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
    xml.writeStartElement(kSettingsElement);

    QStringList oldSegments;

    for (auto it = map.cbegin(), it_end = map.cend(); it != it_end; ++it)
    {
        QStringList segments = it.key().split(QLatin1Char('/'), Qt::SkipEmptyParts);
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

        QString type = variantToType(it.value());
        if (!type.isEmpty())
            xml.writeAttribute(kTypeAttribute, type);

        xml.writeCharacters(variantToString(it.value()));

        oldSegments.swap(segments);
    }

    for (int i = 0; i < oldSegments.size(); ++i)
        xml.writeEndElement();

    xml.writeEndElement();
    xml.writeEndDocument();

    return !xml.hasError();
}

} // namespace base
