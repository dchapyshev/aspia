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
#include <QSet>
#include <QUuid>

#include <algorithm>
#include <memory>
#include <optional>

#include "base/logging.h"
#include "base/crypto/data_cryptor.h"
#include "base/crypto/password_hash.h"
#include "base/crypto/random.h"
#include "base/crypto/secure_memory.h"
#include "base/crypto/secure_string.h"
#include "client/database.h"
#include "proto/router.h"

namespace {

constexpr int kFormatVersion = 1;
constexpr int kSaltSize = 32;
constexpr int kVerifierPayloadSize = 32;
constexpr int kMaxNameLength = 64;
constexpr int kMaxCommentLength = 2048;

using ImportCounters = JsonBackup::ImportCounts;

//--------------------------------------------------------------------------------------------------
// A field that fails to encrypt must not be written as an empty one: the file would be short of
// what the user was told it holds, and nothing in it would say so.
std::optional<QString> encryptToHex(const DataCryptor& cryptor, const QString& value)
{
    if (value.isEmpty())
        return QString();

    SecureByteArray plain(value.toUtf8());
    std::optional<QByteArray> encrypted = cryptor.encrypt(plain.toByteArray());

    if (!encrypted.has_value())
        return std::nullopt;

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
std::optional<QJsonObject> buildRouter(const RouterConfig& router, const DataCryptor& cryptor)
{
    std::optional<QString> display_name = encryptToHex(cryptor, router.displayName());
    std::optional<QString> address = encryptToHex(cryptor, router.address());
    std::optional<QString> username = encryptToHex(cryptor, router.username());
    std::optional<QString> password = encryptToHex(cryptor, router.password().toString());

    if (!display_name || !address || !username || !password)
    {
        LOG(ERROR) << "Unable to encrypt router:" << router.routerId();
        return std::nullopt;
    }

    QJsonObject object;
    object.insert("id", static_cast<qint64>(router.routerId()));
    object.insert("display_name", *display_name);
    object.insert("address", *address);
    object.insert("session_type", static_cast<int>(router.sessionType()));
    object.insert("username", *username);
    object.insert("password", *password);
    return object;
}

//--------------------------------------------------------------------------------------------------
std::optional<QJsonObject> buildGroup(const GroupConfig& group, const DataCryptor& cryptor)
{
    std::optional<QString> name = encryptToHex(cryptor, group.name());
    std::optional<QString> comment = encryptToHex(cryptor, group.comment());

    if (!name || !comment)
    {
        LOG(ERROR) << "Unable to encrypt group:" << group.id();
        return std::nullopt;
    }

    QJsonObject object;
    object.insert("id", static_cast<qint64>(group.id()));
    object.insert("parent_id", static_cast<qint64>(group.parentId()));
    object.insert("name", *name);
    object.insert("comment", *comment);
    return object;
}

//--------------------------------------------------------------------------------------------------
std::optional<QJsonObject> buildHost(const HostConfig& host, const DataCryptor& cryptor)
{
    std::optional<QString> name = encryptToHex(cryptor, host.name());
    std::optional<QString> comment = encryptToHex(cryptor, host.comment());
    std::optional<QString> address = encryptToHex(cryptor, host.address());
    std::optional<QString> username = encryptToHex(cryptor, host.username());
    std::optional<QString> password = encryptToHex(cryptor, host.password().toString());

    if (!name || !comment || !address || !username || !password)
    {
        LOG(ERROR) << "Unable to encrypt host:" << host.id();
        return std::nullopt;
    }

    QJsonObject object;
    object.insert("id", static_cast<qint64>(host.id()));
    object.insert("group_id", static_cast<qint64>(host.groupId()));
    object.insert("router_id", static_cast<qint64>(host.routerId()));
    object.insert("guid", host.guid());
    object.insert("name", *name);
    object.insert("comment", *comment);
    object.insert("address", *address);
    object.insert("username", *username);
    object.insert("password", *password);
    return object;
}

//--------------------------------------------------------------------------------------------------
qint64 importRouter(Database& db, const QJsonObject& router_object, const DataCryptor& cryptor,
                    ImportCounters* counters)
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

    const QList<RouterConfig> existing = db.routerList();
    for (const RouterConfig& router : std::as_const(existing))
    {
        if (router.address() == *address && router.username() == *username)
            return router.routerId();
    }

    QString display_name_value = sanitizedName(*display_name);

    RouterConfig config;
    config.setDisplayName(display_name_value.isEmpty() ? *address : display_name_value);
    config.setAddress(*address);
    config.setUsername(*username);
    config.setPassword(SecureString(std::move(*password)));
    config.setSessionType(static_cast<proto::router::SessionType>(session_type));

    if (!db.addRouter(config))
    {
        LOG(ERROR) << "Unable to add router during import";
        ++counters->routers_skipped;
        return 0;
    }

    ++counters->routers;
    return config.routerId();
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
void importGroups(Database& db,
                  const QJsonArray& groups_array,
                  const DataCryptor& cryptor,
                  QHash<qint64, qint64>* group_id_map,
                  ImportCounters* counters)
{
    QHash<qint64, QList<QJsonObject>> children;
    QList<QJsonObject> all_groups;
    QSet<qint64> present_ids;

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
        all_groups.append(group_object);
        present_ids.insert(id);
    }

    group_id_map->insert(0, 0);

    // BFS from root so a child group is never imported before its parent. |visited_ids| guards
    // against cycles and duplicate ids in the untrusted backup.
    QSet<qint64> visited_ids;
    visited_ids.insert(0);

    QList<qint64> queue;
    queue.append(0);

    // A parent the file does not carry names no group, so what hangs off it starts at the root -
    // where a host whose group is missing is put as well. Leaving those out would take their own
    // children with them, and the tally would say nothing about any of it.
    QList<qint64> missing_parents;
    for (auto it = children.constBegin(); it != children.constEnd(); ++it)
    {
        if (it.key() != 0 && !present_ids.contains(it.key()))
            missing_parents.append(it.key());
    }

    // The hash hands them over in whatever order it likes, and the order decides which of the
    // groups ends up first at the root.
    std::sort(missing_parents.begin(), missing_parents.end());

    for (qint64 parent_id : std::as_const(missing_parents))
    {
        group_id_map->insert(parent_id, 0);
        queue.append(parent_id);
    }

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

            if (visited_ids.contains(old_id))
                continue;
            visited_ids.insert(old_id);

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
            group_config.setParentId(current_new_parent);
            group_config.setName(name);
            group_config.setComment(sanitizedComment(*comment));

            if (!db.addGroup(group_config))
            {
                LOG(ERROR) << "Unable to add group during import";
                ++counters->groups_skipped;
                continue;
            }

            group_id_map->insert(old_id, group_config.id());
            ++counters->groups;
            queue.append(old_id);
        }
    }

    // What the walk never reached names itself as its own parent, straight away or around a ring of
    // groups. That cannot be a tree, so it is not imported - and the tally says so instead of the
    // rows quietly not being there.
    for (const QJsonObject& group_object : std::as_const(all_groups))
    {
        qint64 old_id = 0;
        qint64 old_parent_id = 0;

        if (readGroupIds(group_object, &old_id, &old_parent_id) && !visited_ids.contains(old_id))
            ++counters->groups_skipped;
    }
}

//--------------------------------------------------------------------------------------------------
void importHosts(Database& db,
                 const QJsonArray& hosts_array,
                 const QHash<qint64, qint64>& group_id_map,
                 const QHash<qint64, qint64>& router_id_map,
                 const DataCryptor& cryptor,
                 ImportCounters* counters)
{
    for (const QJsonValue& value : std::as_const(hosts_array))
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
            ++counters->hosts_skipped;
            continue;
        }

        QString name = sanitizedName(*name_decrypted);
        if (name.isEmpty())
        {
            ++counters->hosts_skipped;
            continue;
        }

        if (address->isEmpty())
        {
            ++counters->hosts_skipped;
            continue;
        }

        qint64 old_group_id = object.value("group_id").toInteger(0);
        qint64 old_router_id = object.value("router_id").toInteger(0);

        HostConfig config;
        config.setGroupId(group_id_map.value(old_group_id, 0));
        config.setRouterId(router_id_map.value(old_router_id, 0));
        config.setName(name);
        config.setComment(sanitizedComment(*comment));
        config.setAddress(*address);
        config.setUsername(*username);
        config.setPassword(SecureString(*password));

        QUuid guid = QUuid::fromString(object.value("guid").toString());
        if (!guid.isNull())
        {
            QString guid_string = guid.toString(QUuid::WithoutBraces);
            if (!db.findHostByGuid(guid_string).has_value())
                config.setGuid(guid_string);
        }

        if (!db.addHost(config))
        {
            LOG(ERROR) << "Unable to add host during import";
            ++counters->hosts_skipped;
            continue;
        }

        ++counters->hosts;
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
JsonBackup::Result JsonBackup::exportToFile(Database& db, const QString& file_path,
                                            const SecureString& password, ExportCounts* counts)
{
    if (!db.isValid())
        return Result::DATABASE_UNAVAILABLE;

    QByteArray salt = Random::byteArray(kSaltSize);
    CHECK(!salt.isEmpty());

    SecureByteArray key(PasswordHash::hash(PasswordHash::ARGON2ID, password, salt));
    DataCryptor cryptor(CipherType::AES256_GCM, key);

    QByteArray verifier_payload = Random::byteArray(kVerifierPayloadSize);
    CHECK(!verifier_payload.isEmpty());

    std::optional<QByteArray> verifier = cryptor.encrypt(verifier_payload);
    if (!verifier.has_value())
    {
        LOG(ERROR) << "Failed to generate verifier";
        return Result::INTERNAL_ERROR;
    }

    QJsonObject root;
    root.insert("version", kFormatVersion);
    root.insert("salt", QString::fromLatin1(salt.toHex()));
    root.insert("verifier", QString::fromLatin1(verifier->toHex()));

    // A record that cannot be written whole fails the export. Half of one in the file would be read
    // back as a record whose fields the user had emptied.
    QJsonArray routers_array;
    const QList<RouterConfig> routers = db.routerList();
    for (const RouterConfig& router : std::as_const(routers))
    {
        std::optional<QJsonObject> object = buildRouter(router, cryptor);
        if (!object.has_value())
            return Result::INTERNAL_ERROR;

        routers_array.append(*object);
    }
    root.insert("routers", routers_array);

    QJsonArray groups_array;
    const QList<GroupConfig> groups = db.allGroups();
    for (const GroupConfig& group : std::as_const(groups))
    {
        std::optional<QJsonObject> object = buildGroup(group, cryptor);
        if (!object.has_value())
            return Result::INTERNAL_ERROR;

        groups_array.append(*object);
    }
    root.insert("groups", groups_array);

    QJsonArray hosts_array;
    const QList<HostConfig> hosts = db.allHosts();
    for (const HostConfig& host : std::as_const(hosts))
    {
        std::optional<QJsonObject> object = buildHost(host, cryptor);
        if (!object.has_value())
            return Result::INTERNAL_ERROR;

        hosts_array.append(*object);
    }
    root.insert("hosts", hosts_array);

    QJsonDocument document(root);
    QByteArray payload = document.toJson(QJsonDocument::Indented);

    QSaveFile file(file_path);
    if (!file.open(QIODevice::WriteOnly))
    {
        LOG(ERROR) << "Unable to open file" << file_path << ":" << file.errorString();
        return Result::FILE_ERROR;
    }

    if (file.write(payload) != payload.size() || !file.commit())
    {
        LOG(ERROR) << "Unable to write file" << file_path << ":" << file.errorString();
        return Result::FILE_ERROR;
    }

    if (counts)
    {
        counts->routers = static_cast<int>(routers.size());
        counts->groups = static_cast<int>(groups.size());
        counts->hosts = static_cast<int>(hosts.size());
    }

    return Result::SUCCESS;
}

//--------------------------------------------------------------------------------------------------
// static
JsonBackup::Result JsonBackup::importFromFile(Database& db, const QString& file_path,
                                              const SecureString& password, ImportCounts* counts)
{
    QFile file(file_path);
    if (!file.open(QIODevice::ReadOnly))
    {
        LOG(ERROR) << "Unable to open file" << file_path << ":" << file.errorString();
        return Result::FILE_ERROR;
    }

    QByteArray buffer = file.readAll();
    file.close();

    if (buffer.isEmpty())
        return Result::INVALID_FORMAT;

    QJsonParseError parse_error;
    QJsonDocument document = QJsonDocument::fromJson(buffer, &parse_error);
    if (document.isNull() || !document.isObject())
    {
        LOG(ERROR) << "Invalid JSON document:" << parse_error.errorString();
        return Result::INVALID_FORMAT;
    }

    QJsonObject root = document.object();

    if (root.value("version").toInt(0) != kFormatVersion)
        return Result::UNSUPPORTED_VERSION;

    QByteArray salt = QByteArray::fromHex(root.value("salt").toString().toLatin1());
    QByteArray verifier = QByteArray::fromHex(root.value("verifier").toString().toLatin1());

    if (salt.size() != kSaltSize || verifier.isEmpty())
        return Result::INVALID_FORMAT;

    SecureByteArray key(PasswordHash::hash(PasswordHash::ARGON2ID, password, salt));
    auto cryptor = std::make_unique<DataCryptor>(CipherType::AES256_GCM, key);

    if (!cryptor->decrypt(verifier).has_value())
        return Result::WRONG_PASSWORD;

    if (!db.isValid())
        return Result::DATABASE_UNAVAILABLE;

    ImportCounts local_counters;
    ImportCounters& counters = counts ? *counts : local_counters;

    QHash<qint64, qint64> router_id_map;
    QJsonArray routers_array = root.value("routers").toArray();
    for (const QJsonValue& value : std::as_const(routers_array))
    {
        if (!value.isObject())
            continue;

        QJsonObject router_object = value.toObject();
        qint64 old_id = router_object.value("id").toInteger(0);
        qint64 new_id = importRouter(db, router_object, *cryptor, &counters);
        if (new_id != 0)
            router_id_map.insert(old_id, new_id);
    }

    QHash<qint64, qint64> group_id_map;
    QJsonArray groups_array = root.value("groups").toArray();
    importGroups(db, groups_array, *cryptor, &group_id_map, &counters);

    QJsonArray hosts_array = root.value("hosts").toArray();
    importHosts(db, hosts_array, group_id_map, router_id_map, *cryptor, &counters);

    cryptor.reset();

    if (counters.total() == 0)
        return Result::NOTHING_IMPORTED;

    return Result::SUCCESS;
}
