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

#include "base/ini_file.h"

#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QSaveFile>

#include <limits>
#include <utility>

#include "base/logging.h"

namespace {

// A Windows editor may start the file with a byte order mark.
const char kByteOrderMark[] = "\xEF\xBB\xBF";

// A real configuration file is a few kilobytes; anything much bigger is something else put in its
// place and must not be walked through at all.
constexpr qint64 kMaxFileSize = 1024 * 1024;

// No sane line of a configuration file comes close, and a runaway one must not be read as a whole
// into memory.
constexpr qsizetype kMaxLineLength = 64 * 1024;

constexpr char kSectionStart = '[';
constexpr char kSectionEnd = ']';
constexpr char kAssign = '=';
constexpr char kNewLine = '\n';
constexpr char kCarriageReturn = '\r';
constexpr char kComment = ';';
constexpr char kAlternateComment = '#';

//--------------------------------------------------------------------------------------------------
// The parser reads a name or a value back as it is only when it starts no line of its own and
// carries no surrounding whitespace, which the parser trims away.
bool readsBackUnchanged(const QByteArray& text)
{
    return !text.contains(kNewLine) && !text.contains(kCarriageReturn) && text.trimmed() == text;
}

//--------------------------------------------------------------------------------------------------
bool isValidKey(const QByteArray& key)
{
    // A key with an assignment would split at it, and one that starts like a comment or a section
    // header would not read back as a key at all.
    return !key.isEmpty() && !key.contains(kAssign) && !key.startsWith(kSectionStart) &&
           !key.startsWith(kComment) && !key.startsWith(kAlternateComment) &&
           readsBackUnchanged(key);
}

//--------------------------------------------------------------------------------------------------
bool isHex(const QByteArray& value)
{
    if (value.size() % 2 != 0)
        return false;

    for (const char character : value)
    {
        if ((character < '0' || character > '9') && (character < 'a' || character > 'f') &&
            (character < 'A' || character > 'F'))
        {
            return false;
        }
    }

    return true;
}

} // namespace

//--------------------------------------------------------------------------------------------------
IniFile::IniFile(const QString& file_path, QFileDevice::Permissions permissions)
    : file_path_(file_path),
      permissions_(permissions)
{
    QFile file(file_path_);
    if (!file.exists())
        return;

    if (!file.open(QFile::ReadOnly | QFile::Text))
    {
        LOG(ERROR) << "Unable to open file" << file_path_ << ":" << file.errorString();
        has_errors_ = true;
        return;
    }

    if (file.size() > kMaxFileSize)
    {
        LOG(ERROR) << "File is too big:" << file_path_ << "(" << file.size() << "bytes)";
        has_errors_ = true;
        return;
    }

    parse(&file);

    if (has_errors_)
    {
        LOG(ERROR) << "File contains invalid lines:" << file_path_;

        // A file that was read only in part must not be used.
        sections_.clear();
    }
}

//--------------------------------------------------------------------------------------------------
IniFile::~IniFile()
{
    // A warning would be filtered out by the default logging level.
    if (has_changes_)
        LOG(ERROR) << "Unsaved changes are lost for file" << file_path_;
}

//--------------------------------------------------------------------------------------------------
bool IniFile::isEmpty() const
{
    for (const Section& values : std::as_const(sections_))
    {
        if (!values.isEmpty())
            return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool IniFile::sync()
{
    // The settings of a file that did not read as a whole are empty, so writing them back would
    // replace the file the owner could repair with an empty one.
    if (has_errors_)
    {
        LOG(ERROR) << "File was not read correctly and is not written:" << file_path_;
        return false;
    }

    const bool file_created = !QFileInfo::exists(file_path_);

    QSaveFile file(file_path_);
    if (!file.open(QFile::WriteOnly | QFile::Text))
    {
        LOG(ERROR) << "Unable to open file" << file_path_ << ":" << file.errorString();
        return false;
    }

    if (file_created && permissions_ != QFileDevice::Permissions())
        file.setPermissions(permissions_);

    if (!generate(&file) || !file.commit())
    {
        LOG(ERROR) << "Unable to write file" << file_path_ << ":" << file.errorString();
        return false;
    }

    has_changes_ = false;
    return true;
}

//--------------------------------------------------------------------------------------------------
qlonglong IniFile::int64Value(const QByteArray& section, const QByteArray& key,
                              qlonglong default_value) const
{
    const QByteArray* result = rawValue(section, key);
    if (!result)
        return default_value;

    bool ok = false;
    const qlonglong number = result->toLongLong(&ok);
    if (!ok)
    {
        LOG(ERROR) << "Invalid value for" << key << "in section" << section << "of file" << file_path_;
        return default_value;
    }

    return number;
}

//--------------------------------------------------------------------------------------------------
void IniFile::setInt64Value(const QByteArray& section, const QByteArray& key, qlonglong value)
{
    setRawValue(section, key, QByteArray::number(value));
}

//--------------------------------------------------------------------------------------------------
qulonglong IniFile::uint64Value(const QByteArray& section, const QByteArray& key,
                                qulonglong default_value) const
{
    const QByteArray* result = rawValue(section, key);
    if (!result)
        return default_value;

    bool ok = false;
    const qulonglong number = result->toULongLong(&ok);
    if (!ok)
    {
        LOG(ERROR) << "Invalid value for" << key << "in section" << section << "of file" << file_path_;
        return default_value;
    }

    return number;
}

//--------------------------------------------------------------------------------------------------
void IniFile::setUInt64Value(const QByteArray& section, const QByteArray& key, qulonglong value)
{
    setRawValue(section, key, QByteArray::number(value));
}

//--------------------------------------------------------------------------------------------------
qint32 IniFile::int32Value(const QByteArray& section, const QByteArray& key,
                           qint32 default_value) const
{
    const QByteArray* result = rawValue(section, key);
    if (!result)
        return default_value;

    bool ok = false;
    const qlonglong number = result->toLongLong(&ok);
    if (!ok || number < std::numeric_limits<qint32>::min() || number > std::numeric_limits<qint32>::max())
    {
        LOG(ERROR) << "Invalid value for" << key << "in section" << section << "of file" << file_path_;
        return default_value;
    }

    return static_cast<qint32>(number);
}

//--------------------------------------------------------------------------------------------------
void IniFile::setInt32Value(const QByteArray& section, const QByteArray& key, qint32 value)
{
    setRawValue(section, key, QByteArray::number(value));
}

//--------------------------------------------------------------------------------------------------
quint32 IniFile::uint32Value(const QByteArray& section, const QByteArray& key,
                             quint32 default_value) const
{
    const QByteArray* result = rawValue(section, key);
    if (!result)
        return default_value;

    bool ok = false;
    const qulonglong number = result->toULongLong(&ok);
    if (!ok || number > std::numeric_limits<quint32>::max())
    {
        LOG(ERROR) << "Invalid value for" << key << "in section" << section << "of file" << file_path_;
        return default_value;
    }

    return static_cast<quint32>(number);
}

//--------------------------------------------------------------------------------------------------
void IniFile::setUInt32Value(const QByteArray& section, const QByteArray& key, quint32 value)
{
    setRawValue(section, key, QByteArray::number(value));
}

//--------------------------------------------------------------------------------------------------
qint16 IniFile::int16Value(const QByteArray& section, const QByteArray& key,
                           qint16 default_value) const
{
    const QByteArray* result = rawValue(section, key);
    if (!result)
        return default_value;

    bool ok = false;
    const qlonglong number = result->toLongLong(&ok);
    if (!ok || number < std::numeric_limits<qint16>::min() || number > std::numeric_limits<qint16>::max())
    {
        LOG(ERROR) << "Invalid value for" << key << "in section" << section << "of file" << file_path_;
        return default_value;
    }

    return static_cast<qint16>(number);
}

//--------------------------------------------------------------------------------------------------
void IniFile::setInt16Value(const QByteArray& section, const QByteArray& key, qint16 value)
{
    setRawValue(section, key, QByteArray::number(value));
}

//--------------------------------------------------------------------------------------------------
quint16 IniFile::uint16Value(const QByteArray& section, const QByteArray& key,
                             quint16 default_value) const
{
    const QByteArray* result = rawValue(section, key);
    if (!result)
        return default_value;

    bool ok = false;
    const qulonglong number = result->toULongLong(&ok);
    if (!ok || number > std::numeric_limits<quint16>::max())
    {
        LOG(ERROR) << "Invalid value for" << key << "in section" << section << "of file" << file_path_;
        return default_value;
    }

    return static_cast<quint16>(number);
}

//--------------------------------------------------------------------------------------------------
void IniFile::setUInt16Value(const QByteArray& section, const QByteArray& key, quint16 value)
{
    setRawValue(section, key, QByteArray::number(value));
}

//--------------------------------------------------------------------------------------------------
QString IniFile::stringValue(const QByteArray& section, const QByteArray& key,
                             const QString& default_value) const
{
    const QByteArray* result = rawValue(section, key);
    if (!result)
        return default_value;

    return QString::fromUtf8(*result);
}

//--------------------------------------------------------------------------------------------------
void IniFile::setStringValue(const QByteArray& section, const QByteArray& key, const QString& value)
{
    setRawValue(section, key, value.toUtf8());
}

//--------------------------------------------------------------------------------------------------
void IniFile::setStringValue(const QByteArray& section, const QByteArray& key, const char* value)
{
    setRawValue(section, key, value);
}

//--------------------------------------------------------------------------------------------------
void IniFile::setStringValue(const QByteArray& section, const QByteArray& key,
                             const QByteArray& value)
{
    setRawValue(section, key, value);
}

//--------------------------------------------------------------------------------------------------
bool IniFile::booleanValue(const QByteArray& section, const QByteArray& key,
                           bool default_value) const
{
    const QByteArray* result = rawValue(section, key);
    if (!result)
        return default_value;

    if (*result == "true" || *result == "1")
        return true;

    if (*result == "false" || *result == "0")
        return false;

    LOG(ERROR) << "Invalid value for" << key << "in section" << section << "of file" << file_path_;
    return default_value;
}

//--------------------------------------------------------------------------------------------------
void IniFile::setBooleanValue(const QByteArray& section, const QByteArray& key, bool value)
{
    setRawValue(section, key, value ? "1" : "0");
}

//--------------------------------------------------------------------------------------------------
QByteArray IniFile::binaryValue(const QByteArray& section, const QByteArray& key,
                                const QByteArray& default_value) const
{
    const QByteArray* result = rawValue(section, key);
    if (!result)
        return default_value;

    // QByteArray::fromHex silently skips invalid characters, so a damaged value would decode into
    // a truncated one instead of an empty one.
    if (!isHex(*result))
    {
        LOG(ERROR) << "Invalid value for" << key << "in section" << section << "of file" << file_path_;
        return default_value;
    }

    return QByteArray::fromHex(*result);
}

//--------------------------------------------------------------------------------------------------
void IniFile::setBinaryValue(const QByteArray& section, const QByteArray& key,
                             const QByteArray& value)
{
    setRawValue(section, key, value.toHex());
}

//--------------------------------------------------------------------------------------------------
void IniFile::removeValue(const QByteArray& section, const QByteArray& key)
{
    auto values = sections_.find(section);
    if (values != sections_.end() && values->remove(key) > 0)
        has_changes_ = true;
}

//--------------------------------------------------------------------------------------------------
const QByteArray* IniFile::rawValue(const QByteArray& section, const QByteArray& key) const
{
    auto values = sections_.constFind(section);
    if (values == sections_.constEnd())
        return nullptr;

    auto value = values->constFind(key);
    if (value == values->constEnd())
        return nullptr;

    return &value.value();
}

//--------------------------------------------------------------------------------------------------
void IniFile::setRawValue(const QByteArray& section, const QByteArray& key, const QByteArray& value)
{
    // What would not survive the way through the file must not get into it.
    if (!readsBackUnchanged(section) || !isValidKey(key) || !readsBackUnchanged(value))
    {
        LOG(ERROR) << "Invalid section" << section << ", key" << key << "or value for file"
                   << file_path_;
        return;
    }

    Section& values = sections_[section];

    // A value that is already there is not a change.
    const auto stored = values.find(key);
    if (stored != values.end() && *stored == value)
        return;

    values.insert(key, value);
    has_changes_ = true;
}

//--------------------------------------------------------------------------------------------------
void IniFile::parse(QIODevice* device)
{
    // A file of empty lines overflows a counter of 32 bits within its possible size.
    qint64 line_number = 0;

    // The content of the line stays out of the log: a damaged line of a secret still carries
    // most of it. The owner of the file finds the line by its number.
    auto invalid_line = [this, &line_number]()
    {
        LOG(ERROR) << "Invalid line" << line_number << "in ini file:" << file_path_;
        has_errors_ = true;
    };

    QByteArray section;
    Section* values = nullptr;
    bool skip_section = false;

    while (!device->atEnd())
    {
        QByteArray line = device->readLine(kMaxLineLength);
        ++line_number;

        // The line did not fit into the limit as a whole, so everything after it would be read
        // out of its context.
        if (!line.endsWith(kNewLine) && !device->atEnd())
        {
            LOG(ERROR) << "Too long line" << line_number << "in ini file:" << file_path_;
            has_errors_ = true;
            return;
        }

        // The mark comes before anything else in the file, even the whitespace of the trimming.
        if (line_number == 1 && line.startsWith(kByteOrderMark))
            line.remove(0, sizeof(kByteOrderMark) - 1);

        line = line.trimmed();

        if (line.isEmpty() || line.startsWith(kComment) || line.startsWith(kAlternateComment))
            continue;

        if (line.startsWith(kSectionStart))
        {
            const QByteArray name = line.endsWith(kSectionEnd) ?
                line.mid(1, line.size() - 2).trimmed() : QByteArray();
            if (name.isEmpty())
            {
                invalid_line();

                // The previous section still ends here, so the keys that follow do not fall
                // into it.
                values = nullptr;
                skip_section = true;
                continue;
            }

            section = name;
            values = nullptr;
            skip_section = false;
            continue;
        }

        // The keys of a section whose header did not parse have no section to belong to.
        if (skip_section)
            continue;

        // Everything after the first assignment is the value, so a value may contain one itself.
        const qsizetype position = line.indexOf(kAssign);
        const QByteArray name = position > 0 ? line.first(position).trimmed() : QByteArray();

        if (name.isEmpty())
        {
            invalid_line();
            continue;
        }

        // The section only changes on a header line, so it is looked up once for all its keys.
        if (!values)
            values = &sections_[section];

        if (values->contains(name))
        {
            invalid_line();
            continue;
        }

        values->insert(name, line.sliced(position + 1).trimmed());
    }
}

//--------------------------------------------------------------------------------------------------
bool IniFile::generate(QIODevice* device) const
{
    for (auto section = sections_.cbegin(); section != sections_.cend(); ++section)
    {
        if (section->isEmpty())
            continue;

        QByteArray buffer;

        if (!section.key().isEmpty())
            buffer += kSectionStart + section.key() + kSectionEnd + kNewLine;

        for (auto value = section->cbegin(); value != section->cend(); ++value)
            buffer += value.key() + kAssign + value.value() + kNewLine;

        buffer += kNewLine;

        if (device->write(buffer) != buffer.size())
            return false;
    }

    return true;
}

