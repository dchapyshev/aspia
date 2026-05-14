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

#include "client/json_backup.h"

#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>

#include <memory>
#include <optional>

#include "base/logging.h"
#include "base/crypto/data_cryptor.h"
#include "base/crypto/password_hash.h"
#include "base/crypto/random.h"
#include "base/crypto/secure_memory.h"
#include "base/crypto/secure_string.h"
#include "client/database.h"
#include "client/ui/export_password_dialog.h"
#include "client/ui/unlock_dialog.h"
#include "common/ui/msg_box.h"
#include "proto/router.h"

namespace {

constexpr int kFormatVersion = 1;
constexpr int kSaltSize = 32;
constexpr int kVerifierPayloadSize = 32;
constexpr int kMaxNameLength = 64;
constexpr int kMaxCommentLength = 2048;

//--------------------------------------------------------------------------------------------------
struct ImportCounters
{
    int routers = 0;
    int routers_skipped = 0;
    int groups = 0;
    int groups_skipped = 0;
    int computers = 0;
    int computers_skipped = 0;
};

//--------------------------------------------------------------------------------------------------
QString encryptToHex(const DataCryptor& cryptor, const QString& value)
{
    if (value.isEmpty())
        return QString();

    SecureByteArray plain(value.toUtf8());
    std::optional<QByteArray> encrypted = cryptor.encrypt(plain.toByteArray());

    if (!encrypted.has_value())
    {
        LOG(ERROR) << "Encryption failed";
        return QString();
    }

    return QString::fromLatin1(encrypted->toHex());
}

//--------------------------------------------------------------------------------------------------
// Returns std::nullopt only if the field is present but cannot be decrypted (corrupted data or
// wrong key). An absent or empty hex string yields an empty QString.
std::optional<QString> decryptFromHex(const DataCryptor& cryptor, const QJsonValue& value)
{
    if (value.isUndefined() || value.isNull())
        return QString();

    if (!value.isString())
        return std::nullopt;

    QByteArray hex = value.toString().toLatin1();
    if (hex.isEmpty())
        return QString();

    QByteArray encrypted = QByteArray::fromHex(hex);
    if (encrypted.isEmpty())
        return std::nullopt;

    std::optional<QByteArray> decrypted = cryptor.decrypt(encrypted);
    if (!decrypted.has_value())
        return std::nullopt;

    QString result = QString::fromUtf8(*decrypted);
    memZero(&*decrypted);
    return result;
}

//--------------------------------------------------------------------------------------------------
QString sanitizedName(const QString& name)
{
    return name.left(kMaxNameLength);
}

//--------------------------------------------------------------------------------------------------
QString sanitizedComment(const QString& comment)
{
    return comment.left(kMaxCommentLength);
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
    object.insert("password", encryptToHex(cryptor, router.password.toString()));
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
    object.insert("password", encryptToHex(cryptor, computer.password.toString()));
    return object;
}

//--------------------------------------------------------------------------------------------------
qint64 importRouter(const QJsonObject& router_object, const DataCryptor& cryptor, ImportCounters* counters)
{
    int session_type = router_object.value("session_type").toInt(proto::router::SESSION_TYPE_CLIENT);

    std::optional<QString> display_name = decryptFromHex(cryptor, router_object.value("display_name"));
    std::optional<QString> address = decryptFromHex(cryptor, router_object.value("address"));
    std::optional<QString> username = decryptFromHex(cryptor, router_object.value("username"));
    std::optional<QString> password = decryptFromHex(cryptor, router_object.value("password"));

    if (!display_name.has_value() || !address.has_value() ||
        !username.has_value() || !password.has_value())
    {
        ++counters->routers_skipped;
        return 0;
    }

    if (address->isEmpty() || username->isEmpty() || password->isEmpty())
    {
        ++counters->routers_skipped;
        return 0;
    }

    Database& db = Database::instance();
    const QList<RouterConfig> existing = db.routerList();
    for (const RouterConfig& router : std::as_const(existing))
    {
        if (router.address == *address && router.username == *username)
            return router.router_id;
    }

    QString display_name_value = sanitizedName(*display_name);

    RouterConfig config;
    config.display_name = display_name_value.isEmpty() ? *address : display_name_value;
    config.address = *address;
    config.username = *username;
    config.password = SecureString(std::move(*password));
    config.session_type = static_cast<proto::router::SessionType>(session_type);

    if (!db.addRouter(config))
    {
        LOG(ERROR) << "Unable to add router during import";
        ++counters->routers_skipped;
        return 0;
    }

    ++counters->routers;
    return config.router_id;
}

//--------------------------------------------------------------------------------------------------
bool readGroupIds(const QJsonObject& object, qint64* id, qint64* parent_id)
{
    if (!object.contains("id"))
        return false;

    *id = object.value("id").toInteger();
    *parent_id = object.value("parent_id").toInteger(0);
    return true;
}

//--------------------------------------------------------------------------------------------------
void importGroups(const QJsonArray& groups_array,
                  const DataCryptor& cryptor,
                  QHash<qint64, qint64>* group_id_map,
                  ImportCounters* counters)
{
    Database& db = Database::instance();

    QHash<qint64, QList<QJsonObject>> children;
    for (const QJsonValue& value : std::as_const(groups_array))
    {
        if (!value.isObject())
            continue;

        QJsonObject group_object = value.toObject();
        qint64 id = 0;
        qint64 parent_id = 0;
        if (!readGroupIds(group_object, &id, &parent_id))
            continue;

        children[parent_id].append(group_object);
    }

    group_id_map->insert(0, 0);

    // BFS from root so a child group is never imported before its parent.
    QList<qint64> queue;
    queue.append(0);

    while (!queue.isEmpty())
    {
        qint64 current_old_parent = queue.takeFirst();
        qint64 current_new_parent = group_id_map->value(current_old_parent, 0);

        const QList<QJsonObject>& list = children.value(current_old_parent);
        for (const QJsonObject& group_object : std::as_const(list))
        {
            qint64 old_id = 0;
            qint64 old_parent_id = 0;
            if (!readGroupIds(group_object, &old_id, &old_parent_id))
                continue;

            std::optional<QString> name_decrypted = decryptFromHex(cryptor, group_object.value("name"));
            std::optional<QString> comment = decryptFromHex(cryptor, group_object.value("comment"));
            if (!name_decrypted.has_value() || !comment.has_value())
            {
                ++counters->groups_skipped;
                continue;
            }

            QString name = sanitizedName(*name_decrypted);
            if (name.isEmpty())
            {
                ++counters->groups_skipped;
                continue;
            }

            GroupConfig group_config;
            group_config.parent_id = current_new_parent;
            group_config.name = name;
            group_config.comment = sanitizedComment(*comment);

            if (!db.addGroup(group_config))
            {
                LOG(ERROR) << "Unable to add group during import";
                ++counters->groups_skipped;
                continue;
            }

            group_id_map->insert(old_id, group_config.id);
            ++counters->groups;
            queue.append(old_id);
        }
    }
}

//--------------------------------------------------------------------------------------------------
void importComputers(const QJsonArray& computers_array,
                     const QHash<qint64, qint64>& group_id_map,
                     const QHash<qint64, qint64>& router_id_map,
                     const DataCryptor& cryptor,
                     ImportCounters* counters)
{
    Database& db = Database::instance();

    for (const QJsonValue& value : std::as_const(computers_array))
    {
        if (!value.isObject())
            continue;

        QJsonObject object = value.toObject();

        std::optional<QString> name_decrypted = decryptFromHex(cryptor, object.value("name"));
        std::optional<QString> address = decryptFromHex(cryptor, object.value("address"));
        std::optional<QString> comment = decryptFromHex(cryptor, object.value("comment"));
        std::optional<QString> username = decryptFromHex(cryptor, object.value("username"));
        std::optional<QString> password = decryptFromHex(cryptor, object.value("password"));

        if (!name_decrypted.has_value() || !address.has_value() || !comment.has_value() ||
            !username.has_value() || !password.has_value())
        {
            ++counters->computers_skipped;
            continue;
        }

        QString name = sanitizedName(*name_decrypted);
        if (name.isEmpty())
        {
            ++counters->computers_skipped;
            continue;
        }

        if (address->isEmpty())
        {
            ++counters->computers_skipped;
            continue;
        }

        qint64 old_group_id = object.value("group_id").toInteger(0);
        qint64 old_router_id = object.value("router_id").toInteger(0);

        ComputerConfig config;
        config.group_id = group_id_map.value(old_group_id, 0);
        config.router_id = router_id_map.value(old_router_id, 0);
        config.name = name;
        config.comment = sanitizedComment(*comment);
        config.address = *address;
        config.username = *username;
        config.password = SecureString(*password);

        if (!db.addComputer(config))
        {
            LOG(ERROR) << "Unable to add computer during import";
            ++counters->computers_skipped;
            continue;
        }

        ++counters->computers;
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
bool JsonBackup::exportToFile(QWidget* parent, const QString& file_path)
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

//--------------------------------------------------------------------------------------------------
// static
bool JsonBackup::importFromFile(QWidget* parent, const QString& file_path)
{
    QFile file(file_path);
    if (!file.open(QIODevice::ReadOnly))
    {
        MsgBox::warning(parent, tr("Unable to open file \"%1\": %2").arg(file_path, file.errorString()));
        return false;
    }

    QByteArray buffer = file.readAll();
    file.close();

    if (buffer.isEmpty())
    {
        MsgBox::warning(parent, tr("Selected file is empty."));
        return false;
    }

    QJsonParseError parse_error;
    QJsonDocument document = QJsonDocument::fromJson(buffer, &parse_error);
    if (document.isNull() || !document.isObject())
    {
        MsgBox::warning(parent, tr("The file is not a valid JSON document: %1").arg(parse_error.errorString()));
        return false;
    }

    QJsonObject root = document.object();

    int version = root.value("version").toInt(0);
    if (version != kFormatVersion)
    {
        MsgBox::warning(parent, tr("Unsupported file format version: %1").arg(version));
        return false;
    }

    QByteArray salt = QByteArray::fromHex(root.value("salt").toString().toLatin1());
    QByteArray verifier = QByteArray::fromHex(root.value("verifier").toString().toLatin1());

    if (salt.isEmpty() || verifier.isEmpty())
    {
        MsgBox::warning(parent, tr("The file is corrupted or not encrypted."));
        return false;
    }

    UnlockDialog dialog(parent, file_path, tr("ChaCha20 + Poly1305 (256-bit key)"));
    if (dialog.exec() != QDialog::Accepted)
        return false;

    QByteArray key = PasswordHash::hash(PasswordHash::ARGON2ID, dialog.password(), salt);
    auto cryptor = std::make_unique<DataCryptor>(key);
    memZero(&key);

    if (!cryptor->decrypt(verifier).has_value())
    {
        MsgBox::warning(parent, tr("Unable to decrypt the file with the specified password."));
        return false;
    }

    Database& db = Database::instance();
    if (!db.isValid())
    {
        MsgBox::warning(parent, tr("Address book database is not available."));
        return false;
    }

    ImportCounters counters;

    QHash<qint64, qint64> router_id_map;
    QJsonArray routers_array = root.value("routers").toArray();
    for (const QJsonValue& value : std::as_const(routers_array))
    {
        if (!value.isObject())
            continue;

        QJsonObject router_object = value.toObject();
        qint64 old_id = router_object.value("id").toInteger(0);
        qint64 new_id = importRouter(router_object, *cryptor, &counters);
        if (new_id != 0)
            router_id_map.insert(old_id, new_id);
    }

    QHash<qint64, qint64> group_id_map;
    QJsonArray groups_array = root.value("groups").toArray();
    importGroups(groups_array, *cryptor, &group_id_map, &counters);

    QJsonArray computers_array = root.value("computers").toArray();
    importComputers(computers_array, group_id_map, router_id_map, *cryptor, &counters);

    cryptor.reset();

    if (counters.routers == 0 && counters.groups == 0 && counters.computers == 0)
    {
        MsgBox::information(parent, tr("Nothing was imported."));
        return false;
    }

    MsgBox::information(parent,
        tr("Import completed successfully.\n"
           "Routers added: %1\n"
           "Routers skipped: %2\n"
           "Groups added: %3\n"
           "Groups skipped: %4\n"
           "Computers added: %5\n"
           "Computers skipped: %6")
            .arg(counters.routers)
            .arg(counters.routers_skipped)
            .arg(counters.groups)
            .arg(counters.groups_skipped)
            .arg(counters.computers)
            .arg(counters.computers_skipped));

    return true;
}
