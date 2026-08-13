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

#include "client/backup.h"

#include <QFile>
#include <QHash>
#include <QSaveFile>
#include <QSet>
#include <QUuid>

#include <algorithm>
#include <optional>

#include "base/logging.h"
#include "base/serialization.h"
#include "base/crypto/data_cryptor.h"
#include "base/crypto/password_hash.h"
#include "base/crypto/random.h"
#include "base/crypto/secure_byte_array.h"
#include "client/database.h"
#include "proto/router.h"
#include "proto/storage.h"

namespace {

constexpr int kFormatVersion = 1;
constexpr int kSaltSize = 32;
constexpr int kVerifierPayloadSize = 32;
constexpr int kMaxNameLength = 64;
constexpr int kMaxCommentLength = 2048;

using ImportCounters = Backup::ImportCounts;
using BackupContent = proto::storage::BackupFile::Content;
using BackupGroup = proto::storage::BackupFile::Group;
using BackupHost = proto::storage::BackupFile::Host;
using BackupRouter = proto::storage::BackupFile::Router;

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
void buildRouter(const RouterConfig& router, BackupRouter* out)
{
    out->set_id(router.routerId());
    out->set_display_name(router.displayName().toUtf8().toStdString());
    out->set_address(router.address().toUtf8().toStdString());
    out->set_session_type(static_cast<quint32>(router.sessionType()));
    out->set_username(router.username().toUtf8().toStdString());

    const SecureByteArray password = router.password().toUtf8();
    out->set_password(password.constData(), static_cast<size_t>(password.size()));
}

//--------------------------------------------------------------------------------------------------
void buildGroup(const GroupConfig& group, BackupGroup* out)
{
    out->set_id(group.id());
    out->set_parent_id(group.parentId());
    out->set_name(group.name().toUtf8().toStdString());
    out->set_comment(group.comment().toUtf8().toStdString());
}

//--------------------------------------------------------------------------------------------------
void buildHost(const HostConfig& host, BackupHost* out)
{
    out->set_id(host.id());
    out->set_group_id(host.groupId());
    out->set_router_id(host.routerId());
    out->set_guid(host.guid().toUtf8().toStdString());
    out->set_name(host.name().toUtf8().toStdString());
    out->set_comment(host.comment().toUtf8().toStdString());
    out->set_address(host.address().toUtf8().toStdString());
    out->set_username(host.username().toUtf8().toStdString());

    const SecureByteArray password = host.password().toUtf8();
    out->set_password(password.constData(), static_cast<size_t>(password.size()));
}

//--------------------------------------------------------------------------------------------------
SecureString toSecureString(const std::string& value)
{
    return SecureString::fromUtf8(
        SecureByteArray(value.data(), static_cast<qsizetype>(value.size())));
}

//--------------------------------------------------------------------------------------------------
qint64 importRouter(Database& db, const BackupRouter& router, ImportCounters* counters)
{
    const QString address = QString::fromStdString(router.address());
    const QString username = QString::fromStdString(router.username());

    if (address.isEmpty() || username.isEmpty() || router.password().empty())
    {
        ++counters->routers_skipped;
        return 0;
    }

    const QList<RouterConfig> existing = db.routerList();
    for (const RouterConfig& config : std::as_const(existing))
    {
        if (config.address() == address && config.username() == username)
            return config.routerId();
    }

    const QString display_name = sanitizedName(QString::fromStdString(router.display_name()));

    RouterConfig config;
    config.setDisplayName(display_name.isEmpty() ? address : display_name);
    config.setAddress(address);
    config.setUsername(username);
    config.setPassword(toSecureString(router.password()));
    config.setSessionType(static_cast<proto::router::SessionType>(router.session_type()));

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
void importGroups(Database& db,
                  const BackupContent& data,
                  QHash<qint64, qint64>* group_id_map,
                  ImportCounters* counters)
{
    QHash<qint64, QList<const BackupGroup*>> children;
    QSet<qint64> present_ids;

    // The root is where the tree starts and is not a record, so no group can name it as its own id.
    for (const BackupGroup& group : data.groups())
    {
        if (group.id() == 0)
            continue;

        children[group.parent_id()].append(&group);
        present_ids.insert(group.id());
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
        const qint64 current_old_parent = queue.takeFirst();
        const qint64 current_new_parent = group_id_map->value(current_old_parent, 0);

        const QList<const BackupGroup*>& list = children.value(current_old_parent);
        for (const BackupGroup* group : std::as_const(list))
        {
            if (visited_ids.contains(group->id()))
                continue;
            visited_ids.insert(group->id());

            const QString name = sanitizedName(QString::fromStdString(group->name()));
            if (name.isEmpty())
            {
                ++counters->groups_skipped;
                continue;
            }

            GroupConfig group_config;
            group_config.setParentId(current_new_parent);
            group_config.setName(name);
            group_config.setComment(
                sanitizedComment(QString::fromStdString(group->comment())));

            if (!db.addGroup(group_config))
            {
                LOG(ERROR) << "Unable to add group during import";
                ++counters->groups_skipped;
                continue;
            }

            group_id_map->insert(group->id(), group_config.id());
            ++counters->groups;
            queue.append(group->id());
        }
    }

    // What the walk never reached names itself as its own parent, straight away or around a ring of
    // groups. That cannot be a tree, so it is not imported - and the tally says so instead of the
    // rows quietly not being there.
    for (const BackupGroup& group : data.groups())
    {
        if (group.id() != 0 && !visited_ids.contains(group.id()))
            ++counters->groups_skipped;
    }
}

//--------------------------------------------------------------------------------------------------
void importHosts(Database& db,
                 const BackupContent& data,
                 const QHash<qint64, qint64>& group_id_map,
                 const QHash<qint64, qint64>& router_id_map,
                 ImportCounters* counters)
{
    for (const BackupHost& host : data.hosts())
    {
        const QString name = sanitizedName(QString::fromStdString(host.name()));
        const QString address = QString::fromStdString(host.address());

        if (name.isEmpty() || address.isEmpty())
        {
            ++counters->hosts_skipped;
            continue;
        }

        HostConfig config;
        config.setGroupId(group_id_map.value(host.group_id(), 0));
        config.setRouterId(router_id_map.value(host.router_id(), 0));
        config.setName(name);
        config.setComment(sanitizedComment(QString::fromStdString(host.comment())));
        config.setAddress(address);
        config.setUsername(QString::fromStdString(host.username()));
        config.setPassword(toSecureString(host.password()));

        const QUuid guid = QUuid::fromString(QString::fromStdString(host.guid()));
        if (!guid.isNull())
        {
            const QString guid_string = guid.toString(QUuid::WithoutBraces);
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
Backup::Result Backup::exportToFile(Database& db, const QString& file_path,
                                    const SecureString& password, ExportCounts* counts)
{
    if (!db.isValid())
        return Result::DATABASE_UNAVAILABLE;

    QByteArray salt = Random::byteArray(kSaltSize);
    CHECK(!salt.isEmpty());

    SecureByteArray key(PasswordHash::hash(PasswordHash::ARGON2ID, password, salt));
    DataCryptor cryptor(CipherType::AES256_GCM, key);

    std::optional<QByteArray> verifier = cryptor.encrypt(Random::byteArray(kVerifierPayloadSize));
    if (!verifier.has_value())
    {
        LOG(ERROR) << "Failed to generate verifier";
        return Result::INTERNAL_ERROR;
    }

    BackupContent data;

    const QList<RouterConfig> routers = db.routerList();
    for (const RouterConfig& router : std::as_const(routers))
        buildRouter(router, data.add_routers());

    const QList<GroupConfig> groups = db.allGroups();
    for (const GroupConfig& group : std::as_const(groups))
        buildGroup(group, data.add_groups());

    const QList<HostConfig> hosts = db.allHosts();
    for (const HostConfig& host : std::as_const(hosts))
        buildHost(host, data.add_hosts());

    // Serialized, the address book is in the clear, so it goes into a buffer that wipes itself.
    std::optional<QByteArray> sealed;
    {
        const SecureByteArray buffer(serialize(data));
        sealed = cryptor.encrypt(buffer.toByteArray());
    }

    if (!sealed.has_value())
    {
        LOG(ERROR) << "Unable to encrypt address book";
        return Result::INTERNAL_ERROR;
    }

    proto::storage::BackupFile file_message;
    file_message.set_version(kFormatVersion);
    file_message.set_salt(salt.toStdString());
    file_message.set_verifier(verifier->toStdString());
    file_message.set_data(sealed->toStdString());

    const QByteArray payload = serialize(file_message);
    if (payload.isEmpty())
    {
        LOG(ERROR) << "Unable to serialize backup";
        return Result::INTERNAL_ERROR;
    }

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
Backup::Result Backup::importFromFile(Database& db, const QString& file_path,
                                      const SecureString& password, ImportCounts* counts)
{
    QFile file(file_path);
    if (!file.open(QIODevice::ReadOnly))
    {
        LOG(ERROR) << "Unable to open file" << file_path << ":" << file.errorString();
        return Result::FILE_ERROR;
    }

    const QByteArray buffer = file.readAll();
    file.close();

    if (buffer.isEmpty())
        return Result::INVALID_FORMAT;

    proto::storage::BackupFile file_message;
    if (!parse(buffer, &file_message))
    {
        LOG(ERROR) << "Unable to parse backup file";
        return Result::INVALID_FORMAT;
    }

    if (file_message.version() != kFormatVersion)
        return Result::UNSUPPORTED_VERSION;

    const QByteArray salt = QByteArray::fromStdString(file_message.salt());
    const QByteArray verifier = QByteArray::fromStdString(file_message.verifier());

    if (salt.size() != kSaltSize || verifier.isEmpty() || file_message.data().empty())
        return Result::INVALID_FORMAT;

    SecureByteArray key(PasswordHash::hash(PasswordHash::ARGON2ID, password, salt));
    DataCryptor cryptor(CipherType::AES256_GCM, key);

    // Tells a wrong password from a damaged file.
    if (!cryptor.decrypt(verifier).has_value())
        return Result::WRONG_PASSWORD;

    if (!db.isValid())
        return Result::DATABASE_UNAVAILABLE;

    BackupContent data;
    {
        std::optional<QByteArray> decrypted =
            cryptor.decrypt(QByteArray::fromStdString(file_message.data()));
        if (!decrypted.has_value())
        {
            // The password is already proven right, so this is damage.
            LOG(ERROR) << "Unable to decrypt address book";
            return Result::INVALID_FORMAT;
        }

        const SecureByteArray plain(std::move(*decrypted));
        if (!parse(plain.toByteArray(), &data))
        {
            LOG(ERROR) << "Unable to parse address book";
            return Result::INVALID_FORMAT;
        }
    }

    ImportCounts local_counters;
    ImportCounters& counters = counts ? *counts : local_counters;

    QHash<qint64, qint64> router_id_map;
    for (const BackupRouter& router : data.routers())
    {
        const qint64 new_id = importRouter(db, router, &counters);
        if (new_id != 0)
            router_id_map.insert(router.id(), new_id);
    }

    QHash<qint64, qint64> group_id_map;
    importGroups(db, data, &group_id_map, &counters);

    importHosts(db, data, group_id_map, router_id_map, &counters);

    if (counters.total() == 0)
        return Result::NOTHING_IMPORTED;

    return Result::SUCCESS;
}
