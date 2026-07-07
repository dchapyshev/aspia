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

namespace {

const QString kEmptyString;
const QString kExtension = "conf";
const QString kSettingsElement = "settings";
const QString kGroupElement = "group";
const QString kValueElement = "value";
const QString kNameAttribute = "name";
const QString kTypeAttribute = "type";
const QString kInvalidType = "invalid";
const QString kByteArrayType = "bytes";
const QString kRectType = "rect";
const QString kSizeType = "size";
const QString kPointType = "point";
const QString kVariantType = "variant";

//--------------------------------------------------------------------------------------------------
// Returns true when the string can be stored as XML character data as-is: it consists of characters
// permitted by XML 1.0 and contains no carriage returns (the reader would silently normalize those
// to line feeds). Everything else goes through the binary variant encoding.
bool isXmlSafeString(const QString& value)
{
    for (int i = 0; i < value.size(); ++i)
    {
        const char16_t c = value.at(i).unicode();

        if (c < 0x20)
        {
            if (c != 0x09 && c != 0x0A)
                return false;
        }
        else if (c == 0xFFFE || c == 0xFFFF)
        {
            return false;
        }
        else if (QChar::isHighSurrogate(c))
        {
            if (i + 1 >= value.size() || !value.at(i + 1).isLowSurrogate())
                return false;
            ++i;
        }
        else if (QChar::isLowSurrogate(c))
        {
            return false;
        }
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
QString variantToHex(const QVariant& value)
{
    QByteArray buffer;

    {
        QDataStream stream(&buffer, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_6_10);
        stream << value;
    }

    return buffer.toHex();
}

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
        case QMetaType::QKeySequence:
            result = kEmptyString;
            break;

        case QMetaType::QString:
            result = isXmlSafeString(value.toString()) ? kEmptyString : kVariantType;
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
        case QMetaType::QKeySequence:
            result = value.toString();
            break;

        case QMetaType::QString:
        {
            QString str = value.toString();
            // Strings with characters XML cannot carry are stored in the binary variant encoding;
            // writing them as character data would make the whole file unwritable or unreadable.
            result = isXmlSafeString(str) ? str : variantToHex(value);
        }
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
            result = variantToHex(value);
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
        stream.setVersion(QDataStream::Qt_6_10);

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
        QSettings::registerFormat(kExtension, XmlSettings::readFunc, XmlSettings::writeFunc,
            Qt::CaseSensitive);

    return xml_format;
}

//--------------------------------------------------------------------------------------------------
// static
bool XmlSettings::readFunc(QIODevice& device, QSettings::SettingsMap& map)
{
    // One entry per currently open element. The value is inserted when its element closes, so empty
    // values keep their keys, character data split into several chunks is accumulated, and every
    // element uses its own type attribute.
    struct Level
    {
        QString name;
        QString type;
        QString text;
        bool has_children = false;
    };

    QXmlStreamReader xml(&device);
    QList<Level> levels;

    bool settings_found = false;

    while (!xml.atEnd() && !xml.hasError())
    {
        switch (xml.readNext())
        {
            case QXmlStreamReader::StartElement:
            {
                // The root wrapper does not form a key segment.
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

                if (!levels.isEmpty())
                    levels.last().has_children = true;

                Level level;
                level.name = std::move(name);
                level.type = attributes.value(kTypeAttribute).toString();
                levels.append(std::move(level));
            }
            break;

            case QXmlStreamReader::EndElement:
            {
                // The wrapper did not push a level, so it must not pop one either.
                if (xml.name() == kSettingsElement)
                    continue;

                if (levels.isEmpty())
                    continue;

                // A closing leaf always produces its key, even with an empty value; a group only
                // when it carried its own character data next to the child elements.
                Level level = levels.takeLast();

                // The auto-formatting indent written before a child element ends up as trailing
                // whitespace in the parent's own character data; it is formatting, not the value.
                if (level.has_children)
                {
                    while (!level.text.isEmpty() && level.text.back().isSpace())
                        level.text.chop(1);
                }

                if (!level.has_children || !level.text.isEmpty())
                {
                    QString key;

                    for (const Level& parent : std::as_const(levels))
                    {
                        key += parent.name;
                        key += QLatin1Char('/');
                    }
                    key += level.name;

                    map[key] = stringToVariant(level.text, level.type);
                }
            }
            break;

            case QXmlStreamReader::Characters:
            {
                if (levels.isEmpty())
                {
                    // Only formatting whitespace may appear outside of the value elements.
                    if (!xml.isWhitespace())
                        return false;
                    continue;
                }

                // Formatting whitespace between child elements is not data; whitespace arriving
                // after real character data is part of a split text node and is kept.
                if (xml.isWhitespace() && levels.last().text.isEmpty())
                    continue;

                levels.last().text += xml.text();
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

        // An empty key has no element to carry the value.
        if (segments.isEmpty())
            continue;

        while (count < oldSegments.size() && count < segments.size() &&
               segments.at(count) == oldSegments.at(count))
        {
            ++count;
        }

        // A key that collapses into the previous one after normalization (for example a stray "//")
        // would leave no element to open; write it as a sibling leaf instead of corrupting the
        // currently open element.
        if (count == segments.size())
            --count;

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
