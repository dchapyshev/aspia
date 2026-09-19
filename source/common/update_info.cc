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

#include "common/update_info.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include "base/logging.h"

namespace {

const int kSchemaFormat = 1;
const int kMaxDescriptionLength = 4096;
const int kMinUrlLength = 10;
const int kMaxUrlLength = 256;
const int kSha256Length = 64;

volatile auto g_updateInfoType = qRegisterMetaType<UpdateInfo>();

//--------------------------------------------------------------------------------------------------
bool isLowerHex(const QString& text)
{
    for (QChar character : text)
    {
        if (!((character >= '0' && character <= '9') || (character >= 'a' && character <= 'f')))
            return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
QJsonObject rootObject(const QByteArray& buffer)
{
    QJsonParseError error;
    QJsonDocument document = QJsonDocument::fromJson(buffer, &error);
    if (!document.isObject())
    {
        LOG(ERROR) << "Unable to parse JSON:" << error.errorString();
        return QJsonObject();
    }

    QJsonObject root = document.object();
    int format = root.value("format").toInt();
    if (format != kSchemaFormat)
    {
        LOG(ERROR) << "Unsupported schema format:" << format;
        return QJsonObject();
    }

    return root;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
QVersionNumber UpdateInfo::targetVersion(const QByteArray& buffer, const QVersionNumber& current)
{
    QJsonObject root = rootObject(buffer);
    if (root.isEmpty())
        return QVersionNumber();

    const QJsonArray rules = root.value("updates").toArray();
    for (const QJsonValue& item : rules)
    {
        QJsonObject rule = item.toObject();
        QVersionNumber source = QVersionNumber::fromString(rule.value("source").toString());

        if (source.normalized() != current.normalized())
            continue;

        QString target = rule.value("target").toString();

        // A rule may name a label instead of a version. The label is the only place where the
        // current version is written down, so a release moves it and leaves the rules alone.
        if (target.startsWith('@'))
        {
            QString label = target.mid(1);

            target = root.value("targets").toObject().value(label).toString();
            if (target.isEmpty())
            {
                LOG(ERROR) << "Unknown label:" << label;
                return QVersionNumber();
            }
        }

        QVersionNumber version = QVersionNumber::fromString(target);
        if (version.isNull())
            LOG(ERROR) << "Invalid target version:" << target;

        return version;
    }

    LOG(INFO) << "No rule for version" << current.toString();
    return QVersionNumber();
}

//--------------------------------------------------------------------------------------------------
// static
UpdateInfo UpdateInfo::fromManifest(const QByteArray& buffer, const QString& package,
                                    const QString& os, const QString& arch, const QString& format)
{
    QJsonObject root = rootObject(buffer);
    if (root.isEmpty())
        return UpdateInfo();

    const QJsonArray files = root.value("packages").toObject()
                                 .value(package).toObject()
                                 .value(os).toObject()
                                 .value(arch).toArray();
    if (files.isEmpty())
    {
        // A missing entry is how the manifest says the release has no build for this platform.
        LOG(INFO) << "Release has no files for" << package << os << arch;
        return UpdateInfo();
    }

    QJsonObject file = files.first().toObject();
    for (const QJsonValue& item : files)
    {
        if (!format.isEmpty() && item.toObject().value("format").toString() == format)
        {
            file = item.toObject();
            break;
        }
    }

    UpdateInfo update_info;
    update_info.version_ = QVersionNumber::fromString(root.value("version").toString());
    update_info.description_ = root.value("description").toString();
    update_info.url_ = file.value("url").toString();
    update_info.sha256_ = file.value("sha256").toString().toLower();
    update_info.format_ = file.value("format").toString();

    if (update_info.version_.isNull())
    {
        LOG(ERROR) << "Manifest without a version";
    }
    else if (update_info.description_.size() > kMaxDescriptionLength)
    {
        LOG(ERROR) << "Too many characters in description";
    }
    else if (update_info.url_.size() < kMinUrlLength || update_info.url_.size() > kMaxUrlLength)
    {
        LOG(ERROR) << "Incorrect number of characters in URL";
    }
    else if (update_info.sha256_.size() != kSha256Length || !isLowerHex(update_info.sha256_))
    {
        LOG(ERROR) << "Invalid sha256 of file" << update_info.url_;
    }
    else if (update_info.format_.isEmpty())
    {
        LOG(ERROR) << "File without a format:" << update_info.url_;
    }
    else
    {
        update_info.valid_ = true;
    }

    return update_info;
}
