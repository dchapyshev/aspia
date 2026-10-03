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

#include "host/win/portable_package.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <qt_windows.h>

#include "base/logging.h"
#include "host/database.h"

namespace {

const wchar_t kResourceName[] = L"ASPIA_QS_SETTINGS";
const WORD kResourceLanguage = MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL);
const wchar_t kIconResourceName[] = L"IDI_ICON1";
const char kQuickSupportIcon[] = ":/img/aspia-qs.ico";

const char kRouterAddress[] = "router_address";
const char kRouterPublicKey[] = "router_public_key";

const qsizetype kRouterPublicKeySize = 32;

#pragma pack(push, 2)

struct IconDir
{
    WORD reserved;
    WORD type;
    WORD count;
};

struct IconDirEntry
{
    BYTE width;
    BYTE height;
    BYTE color_count;
    BYTE reserved;
    WORD planes;
    WORD bit_count;
    DWORD bytes_in_res;
    DWORD image_offset;
};

struct GroupIconDirEntry
{
    BYTE width;
    BYTE height;
    BYTE color_count;
    BYTE reserved;
    WORD planes;
    WORD bit_count;
    DWORD bytes_in_res;
    WORD id;
};

#pragma pack(pop)

//--------------------------------------------------------------------------------------------------
bool isValidSettings(const PortablePackage::Settings& settings)
{
    return settings.router_address.isValid() &&
           settings.router_public_key.size() == kRouterPublicKeySize;
}

//--------------------------------------------------------------------------------------------------
bool replaceIcon(HANDLE update)
{
    QFile file(kQuickSupportIcon);
    if (!file.open(QIODevice::ReadOnly))
    {
        LOG(ERROR) << "Unable to open icon:" << file.errorString();
        return false;
    }

    const QByteArray icon = file.readAll();

    IconDir dir;
    if (icon.size() < static_cast<qsizetype>(sizeof(dir)))
    {
        LOG(ERROR) << "Invalid icon file";
        return false;
    }

    memcpy(&dir, icon.constData(), sizeof(dir));

    if (dir.type != 1 || dir.count == 0 ||
        icon.size() < static_cast<qsizetype>(sizeof(dir) + dir.count * sizeof(IconDirEntry)))
    {
        LOG(ERROR) << "Invalid icon directory";
        return false;
    }

    QByteArray group(reinterpret_cast<const char*>(&dir), sizeof(dir));

    for (WORD i = 0; i < dir.count; ++i)
    {
        IconDirEntry entry;
        memcpy(&entry, icon.constData() + sizeof(dir) + i * sizeof(entry), sizeof(entry));

        if (static_cast<qint64>(entry.image_offset) + entry.bytes_in_res > icon.size())
        {
            LOG(ERROR) << "Invalid icon image" << i;
            return false;
        }

        const WORD id = i + 1;

        if (!UpdateResourceW(update, RT_ICON, MAKEINTRESOURCEW(id), kResourceLanguage,
                             const_cast<char*>(icon.constData() + entry.image_offset), entry.bytes_in_res))
        {
            PLOG(ERROR) << "UpdateResourceW failed";
            return false;
        }

        const GroupIconDirEntry group_entry = { entry.width, entry.height, entry.color_count,
            entry.reserved, entry.planes, entry.bit_count, entry.bytes_in_res, id };
        group.append(reinterpret_cast<const char*>(&group_entry), sizeof(group_entry));
    }

    if (!UpdateResourceW(update, RT_GROUP_ICON, kIconResourceName, kResourceLanguage, group.data(),
                         static_cast<DWORD>(group.size())))
    {
        PLOG(ERROR) << "UpdateResourceW failed";
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool updateResources(const QString& file_path, const QByteArray& data)
{
    HANDLE update = BeginUpdateResourceW(qUtf16Printable(file_path), FALSE);
    if (!update)
    {
        PLOG(ERROR) << "BeginUpdateResourceW failed";
        return false;
    }

    if (!UpdateResourceW(update, RT_RCDATA, kResourceName, kResourceLanguage,
                         const_cast<char*>(data.constData()), static_cast<DWORD>(data.size())))
    {
        PLOG(ERROR) << "UpdateResourceW failed";
        EndUpdateResourceW(update, TRUE);
        return false;
    }

    if (!replaceIcon(update))
    {
        EndUpdateResourceW(update, TRUE);
        return false;
    }

    if (!EndUpdateResourceW(update, FALSE))
    {
        PLOG(ERROR) << "EndUpdateResourceW failed";
        return false;
    }

    return true;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
PortablePackage::Result PortablePackage::exportWithSettings(const QString& file_path)
{
    Database& db = Database::instance();

    Settings settings;
    settings.router_address = db.routerAddress();
    settings.router_public_key = db.routerPublicKey();

    if (!db.isRouterEnabled() || !isValidSettings(settings))
    {
        LOG(ERROR) << "Router is not configured";
        return Result::NO_ROUTER;
    }

    QJsonObject object;
    object[kRouterAddress] = settings.router_address.toString();
    object[kRouterPublicKey] = QString::fromLatin1(settings.router_public_key.toHex());

    const QString executable_path = QCoreApplication::applicationFilePath();

    if (QFile::exists(file_path) && !QFile::remove(file_path))
    {
        LOG(ERROR) << "Unable to remove existing file:" << file_path;
        return Result::FAILED;
    }

    if (!QFile::copy(executable_path, file_path))
    {
        LOG(ERROR) << "Unable to copy" << executable_path << "to:" << file_path;
        return Result::FAILED;
    }

    if (!updateResources(file_path, QJsonDocument(object).toJson(QJsonDocument::Compact)))
    {
        QFile::remove(file_path);
        return Result::FAILED;
    }

    return Result::SUCCESS;
}

//--------------------------------------------------------------------------------------------------
// static
std::optional<PortablePackage::Settings> PortablePackage::builtInSettings()
{
    HRSRC resource = FindResourceExW(nullptr, RT_RCDATA, kResourceName, kResourceLanguage);
    if (!resource)
        return std::nullopt;

    HGLOBAL handle = LoadResource(nullptr, resource);
    const DWORD size = SizeofResource(nullptr, resource);
    const void* data = handle ? LockResource(handle) : nullptr;
    if (!data || !size)
    {
        LOG(ERROR) << "Unable to load built-in settings";
        return std::nullopt;
    }

    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(
        QByteArray::fromRawData(static_cast<const char*>(data), static_cast<qsizetype>(size)), &error);
    if (!document.isObject())
    {
        LOG(ERROR) << "Unable to parse built-in settings:" << error.errorString();
        return std::nullopt;
    }

    const QJsonObject object = document.object();

    Settings settings;
    settings.router_address =
        Address::fromString(object[kRouterAddress].toString(), kDefaultRouterHostTcpPort);
    settings.router_public_key = QByteArray::fromHex(object[kRouterPublicKey].toString().toLatin1());

    if (!isValidSettings(settings))
    {
        LOG(ERROR) << "Invalid built-in settings";
        return std::nullopt;
    }

    return settings;
}
