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

#include "client/json_exporter.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <optional>

#include "base/logging.h"
#include "base/crypto/data_cryptor.h"
#include "base/crypto/password_hash.h"
#include "base/crypto/random.h"
#include "base/crypto/secure_memory.h"
#include "client/database.h"
#include "client/ui/export_password_dialog.h"
#include "common/ui/msg_box.h"

namespace {

constexpr int kFormatVersion = 1;
constexpr int kSaltSize = 32;
constexpr int kVerifierPayloadSize = 32;

//--------------------------------------------------------------------------------------------------
QString encryptToHex(const DataCryptor& cryptor, const QString& value)
{
    if (value.isEmpty())
        return QString();

    QByteArray plain = value.toUtf8();
    std::optional<QByteArray> encrypted = cryptor.encrypt(plain);
    memZero(&plain);

    if (!encrypted.has_value())
    {
        LOG(ERROR) << "Encryption failed";
        return QString();
    }

    return QString::fromLatin1(encrypted->toHex());
}

//--------------------------------------------------------------------------------------------------
QJsonObject buildRouter(const RouterConfig& router, const DataCryptor& cryptor)
{
    QJsonObject object;
    object.insert("id", static_cast<qint64>(router.router_id));
    object.insert("display_name", encryptToHex(cryptor, router.display_name));
    object.insert("address", encryptToHex(cryptor, router.address));
    object.insert("session_type", static_cast<int>(router.session_type));
    object.insert("username", encryptToHex(cryptor, router.username));
    object.insert("password", encryptToHex(cryptor, router.password));
    return object;
}

//--------------------------------------------------------------------------------------------------
QJsonObject buildGroup(const GroupConfig& group, const DataCryptor& cryptor)
{
    QJsonObject object;
    object.insert("id", static_cast<qint64>(group.id));
    object.insert("parent_id", static_cast<qint64>(group.parent_id));
    object.insert("name", encryptToHex(cryptor, group.name));
    object.insert("comment", encryptToHex(cryptor, group.comment));
    return object;
}

//--------------------------------------------------------------------------------------------------
QJsonObject buildComputer(const ComputerConfig& computer, const DataCryptor& cryptor)
{
    QJsonObject object;
    object.insert("id", static_cast<qint64>(computer.id));
    object.insert("group_id", static_cast<qint64>(computer.group_id));
    object.insert("router_id", static_cast<qint64>(computer.router_id));
    object.insert("name", encryptToHex(cryptor, computer.name));
    object.insert("comment", encryptToHex(cryptor, computer.comment));
    object.insert("address", encryptToHex(cryptor, computer.address));
    object.insert("username", encryptToHex(cryptor, computer.username));
    object.insert("password", encryptToHex(cryptor, computer.password));
    return object;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
bool JsonExporter::exportToFile(QWidget* parent, const QString& file_path)
{
    Database& db = Database::instance();
    if (!db.isValid())
    {
        MsgBox::warning(parent, tr("Address book database is not available."));
        return false;
    }

    ExportPasswordDialog dialog(parent);
    if (dialog.exec() != QDialog::Accepted)
        return false;

    QByteArray salt = Random::byteArray(kSaltSize);
    QByteArray key = PasswordHash::hash(PasswordHash::ARGON2ID, dialog.password(), salt);
    DataCryptor cryptor(key);
    memZero(&key);

    std::optional<QByteArray> verifier = cryptor.encrypt(Random::byteArray(kVerifierPayloadSize));
    if (!verifier.has_value())
    {
        MsgBox::warning(parent, tr("Failed to generate verifier."));
        return false;
    }

    QJsonObject root;
    root.insert("version", kFormatVersion);
    root.insert("salt", QString::fromLatin1(salt.toHex()));
    root.insert("verifier", QString::fromLatin1(verifier->toHex()));

    QJsonArray routers_array;
    const QList<RouterConfig> routers = db.routerList();
    for (const RouterConfig& router : std::as_const(routers))
        routers_array.append(buildRouter(router, cryptor));
    root.insert("routers", routers_array);

    QJsonArray groups_array;
    const QList<GroupConfig> groups = db.allGroups();
    for (const GroupConfig& group : std::as_const(groups))
        groups_array.append(buildGroup(group, cryptor));
    root.insert("groups", groups_array);

    QJsonArray computers_array;
    const QList<ComputerConfig> computers = db.allComputers();
    for (const ComputerConfig& computer : std::as_const(computers))
        computers_array.append(buildComputer(computer, cryptor));
    root.insert("computers", computers_array);

    QJsonDocument document(root);
    QByteArray payload = document.toJson(QJsonDocument::Indented);

    QSaveFile file(file_path);
    if (!file.open(QIODevice::WriteOnly))
    {
        MsgBox::warning(parent,
            tr("Unable to open file \"%1\": %2").arg(file_path, file.errorString()));
        return false;
    }

    if (file.write(payload) != payload.size() || !file.commit())
    {
        MsgBox::warning(parent, tr("Unable to write file \"%1\": %2").arg(file_path, file.errorString()));
        return false;
    }

    MsgBox::information(parent,
        tr("Export completed successfully.\n"
           "Routers exported: %1\n"
           "Groups exported: %2\n"
           "Computers exported: %3")
            .arg(routers.size())
            .arg(groups.size())
            .arg(computers.size()));

    return true;
}
