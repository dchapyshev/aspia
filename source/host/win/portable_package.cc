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

const wchar_t kResourceName[] = L"ASPIA_PORTABLE_SETTINGS";
const WORD kResourceLanguage = MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL);

const char kRouterAddress[] = "router_address";
const char kRouterPublicKey[] = "router_public_key";

const qsizetype kRouterPublicKeySize = 32;

//--------------------------------------------------------------------------------------------------
bool isValidSettings(const PortablePackage::Settings& settings)
{
    return settings.router_address.isValid() &&
           settings.router_public_key.size() == kRouterPublicKeySize;
}

//--------------------------------------------------------------------------------------------------
bool addResource(const QString& file_path, const QByteArray& data)
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

    if (!addResource(file_path, QJsonDocument(object).toJson(QJsonDocument::Compact)))
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
