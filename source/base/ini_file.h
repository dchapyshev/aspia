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

#ifndef BASE_INI_FILE_H
#define BASE_INI_FILE_H

#include <QByteArray>
#include <QFileDevice>
#include <QMap>
#include <QString>

class QIODevice;

// Reader and writer for ini files: sections with key/value pairs and comments starting with ';' or
// '#'. Keys that come before the first section header belong to the unnamed section, and the names
// of both are case sensitive. Comments and the original order of the keys are not preserved when
// writing.
//
// Each kind of value goes through its own pair of accessors, which say how the value is stored
// in the file.
class IniFile
{
public:
    // Reads |file_path|. A file that does not exist yet leaves the settings empty and is not an
    // error; one that cannot be read as a whole leaves them empty and sets hasErrors(). Lines that
    // are neither a comment, a section header nor a key/value pair are such an error, as are
    // repeated keys and lines of about 64 kB and longer. A file of more than a megabyte is not a
    // configuration file at all and is not read.
    //
    // |permissions| are applied by sync() when it creates the file; an existing file keeps the
    // permissions it already has. Empty permissions leave those of a new file to the system.
    explicit IniFile(const QString& file_path,
                     QFileDevice::Permissions permissions = QFileDevice::Permissions());

    // Complains into the log when the changes were not written back by sync().
    ~IniFile();

    const QString& filePath() const { return file_path_; }

    bool isEmpty() const;
    bool hasErrors() const { return has_errors_; }

    // Replaces the file as a whole, leaving the previous one untouched if writing fails. A file
    // that was not read correctly (hasErrors()) is never written, so its owner can repair it.
    bool sync();

    // Reading a missing key gives |default_value|; so does a value that does not fit the
    // requested type, which is logged. The log carries the key and never the value: the class
    // cannot know which values are secrets. A key that is present with an empty value is not a
    // missing one: it is an empty text that fits no number and no boolean.
    //
    // The section, the key and a text value must read back from the file as they are: none of
    // them may carry a line break or surrounding whitespace, and a key must not be empty, contain
    // an assignment or start like a comment or a section header. A value that must not is not
    // stored, which is logged.

    // A number is stored as it is written. A caller that needs a narrower type checks the range
    // itself.
    qlonglong int64Value(const QByteArray& section, const QByteArray& key,
                         qlonglong default_value = 0) const;
    void setInt64Value(const QByteArray& section, const QByteArray& key, qlonglong value);

    qulonglong uint64Value(const QByteArray& section, const QByteArray& key,
                           qulonglong default_value = 0) const;
    void setUInt64Value(const QByteArray& section, const QByteArray& key, qulonglong value);

    qint32 int32Value(const QByteArray& section, const QByteArray& key,
                      qint32 default_value = 0) const;
    void setInt32Value(const QByteArray& section, const QByteArray& key, qint32 value);

    quint32 uint32Value(const QByteArray& section, const QByteArray& key,
                        quint32 default_value = 0) const;
    void setUInt32Value(const QByteArray& section, const QByteArray& key, quint32 value);

    qint16 int16Value(const QByteArray& section, const QByteArray& key,
                      qint16 default_value = 0) const;
    void setInt16Value(const QByteArray& section, const QByteArray& key, qint16 value);

    quint16 uint16Value(const QByteArray& section, const QByteArray& key,
                        quint16 default_value = 0) const;
    void setUInt16Value(const QByteArray& section, const QByteArray& key, quint16 value);

    // Text is stored encoded in utf-8. The QByteArray overload takes the bytes of a text that
    // already is utf-8 and stores them as they are; latin1 and other encodings convert into
    // QString first.
    QString stringValue(const QByteArray& section, const QByteArray& key,
                        const QString& default_value = QString()) const;
    void setStringValue(const QByteArray& section, const QByteArray& key, const QString& value);
    void setStringValue(const QByteArray& section, const QByteArray& key, const char* value);
    void setStringValue(const QByteArray& section, const QByteArray& key, const QByteArray& value);

    // A boolean is stored as "1" or "0" and is also read as "true" and "false".
    bool booleanValue(const QByteArray& section, const QByteArray& key,
                      bool default_value = false) const;
    void setBooleanValue(const QByteArray& section, const QByteArray& key, bool value);

    // Binary data is stored encoded in hex.
    QByteArray binaryValue(const QByteArray& section, const QByteArray& key,
                           const QByteArray& default_value = QByteArray()) const;
    void setBinaryValue(const QByteArray& section, const QByteArray& key, const QByteArray& value);

    void removeValue(const QByteArray& section, const QByteArray& key);

private:
    using Section = QMap<QByteArray, QByteArray>;

    const QByteArray* rawValue(const QByteArray& section, const QByteArray& key) const;
    void setRawValue(const QByteArray& section, const QByteArray& key, const QByteArray& value);

    void parse(QIODevice* device);
    bool generate(QIODevice* device) const;

    const QString file_path_;
    const QFileDevice::Permissions permissions_;
    QMap<QByteArray, Section> sections_;
    bool has_errors_ = false;
    bool has_changes_ = false;

    Q_DISABLE_COPY_MOVE(IniFile)
};

#endif // BASE_INI_FILE_H
