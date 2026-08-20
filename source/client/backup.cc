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

#include <optional>

#include "base/logging.h"
#include "base/serialization.h"
#include "base/crypto/data_cryptor.h"
#include "base/crypto/password_hash.h"
#include "base/crypto/secure_byte_array.h"
#include "base/crypto/secure_memory.h"
#include "base/peer/host_id.h"
#include "client/database.h"
#include "client/master_password.h"
#include "proto/router.h"
#include "proto/storage.h"

namespace {

constexpr int kFormatVersion = 1;
constexpr int kSaltSize = 32;

using BackupContent = proto::storage::BackupFile::Content;
using BackupLocalGroup = proto::storage::BackupFile::LocalGroup;
using BackupLocalHost = proto::storage::BackupFile::LocalHost;
using BackupRouter = proto::storage::BackupFile::Router;
using BackupRouterHost = proto::storage::BackupFile::RouterHost;

//--------------------------------------------------------------------------------------------------
SecureString toSecureString(const std::string& value)
{
    return SecureString(QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size())));
}

//--------------------------------------------------------------------------------------------------
void buildRouter(const RouterConfig& router, BackupRouter* out)
{
    out->set_display_name(router.displayName().toStdString());
    out->set_address(router.address().toStdString());
    out->set_session_type(static_cast<quint32>(router.sessionType()));
    out->set_username(router.username().toStdString());
    out->set_guid(router.guid().toStdString());

    const SecureByteArray password = router.password().toUtf8();
    out->set_password(password.constData(), static_cast<size_t>(password.size()));
}

//--------------------------------------------------------------------------------------------------
void buildLocalGroup(const LocalGroupConfig& group, const QString& parent_guid,
                     BackupLocalGroup* out)
{
    out->set_guid(group.guid().toStdString());
    out->set_parent_guid(parent_guid.toStdString());
    out->set_name(group.name().toStdString());
    out->set_comment(group.comment().toStdString());
}

//--------------------------------------------------------------------------------------------------
void buildLocalHost(const LocalHostConfig& host, const QString& group_guid,
                    const QString& router_guid, BackupLocalHost* out)
{
    out->set_group_guid(group_guid.toStdString());
    out->set_router_guid(router_guid.toStdString());
    out->set_guid(host.guid().toStdString());
    out->set_name(host.name().toStdString());
    out->set_comment(host.comment().toStdString());
    out->set_address(host.address().toStdString());
    out->set_username(host.username().toStdString());
    out->set_create_time(host.createTime());
    out->set_modify_time(host.modifyTime());
    out->set_connect_time(host.connectTime());

    const SecureByteArray password = host.password().toUtf8();
    out->set_password(password.constData(), static_cast<size_t>(password.size()));
}

//--------------------------------------------------------------------------------------------------
void buildRouterHost(const RouterHostConfig& host, const QString& router_guid,
                     BackupRouterHost* out)
{
    out->set_router_guid(router_guid.toStdString());
    out->set_host_id(host.hostId());
    out->set_username(host.username().toStdString());

    const SecureByteArray password = host.password().toUtf8();
    out->set_password(password.constData(), static_cast<size_t>(password.size()));
}

//--------------------------------------------------------------------------------------------------
// Zeroes the address, the user name and the password of every record. Freeing the buffer would
// leave them in memory as they are.
void eraseSecretFields(BackupContent* content)
{
    for (BackupRouter& router : *content->mutable_routers())
    {
        memZero(router.mutable_address());
        memZero(router.mutable_username());
        memZero(router.mutable_password());
    }

    for (BackupLocalHost& host : *content->mutable_local_hosts())
    {
        memZero(host.mutable_address());
        memZero(host.mutable_username());
        memZero(host.mutable_password());
    }

    for (BackupRouterHost& host : *content->mutable_router_hosts())
    {
        memZero(host.mutable_username());
        memZero(host.mutable_password());
    }
}

//--------------------------------------------------------------------------------------------------
QByteArray fileSalt(const proto::storage::BackupFile& file_message)
{
    return QByteArray::fromStdString(file_message.salt());
}

//--------------------------------------------------------------------------------------------------
// Fills |data| with the address book of |db|, counting what goes into it. A book with a record that
// does not open is refused as a whole, because the file would be missing what it exists for.
Backup::Result collectContent(Database& db, BackupContent* data, Backup::Report* report)
{
    const QList<RouterConfig> routers = db.routerList();
    const QList<LocalGroupConfig> groups = db.allLocalGroups();

    // What a record of the book is named by in the file. A link resolved to nothing comes out
    // empty, which is what a record reaching no router and a group at the root say anyway.
    QHash<qint64, QString> router_guids;
    for (const RouterConfig& router : std::as_const(routers))
        router_guids.insert(router.routerId(), router.guid());

    QHash<qint64, QString> group_guids;
    for (const LocalGroupConfig& group : std::as_const(groups))
        group_guids.insert(group.id(), group.guid());

    for (const RouterConfig& router : std::as_const(routers))
    {
        if (!router.isValid())
        {
            LOG(ERROR) << "Unable to read credentials of router:" << router.routerId();
            return Backup::Result::INTERNAL_ERROR;
        }

        buildRouter(router, data->add_routers());
        ++report->routers;
    }

    for (const LocalGroupConfig& group : std::as_const(groups))
    {
        buildLocalGroup(group, group_guids.value(group.parentId()), data->add_local_groups());
        ++report->local_groups;
    }

    const QList<LocalHostConfig> hosts = db.allLocalHosts();
    for (const LocalHostConfig& host : std::as_const(hosts))
    {
        if (host.address().isEmpty())
        {
            LOG(ERROR) << "Unable to read credentials of host:" << host.id();
            return Backup::Result::INTERNAL_ERROR;
        }

        // A host keeps naming the router it was reached through even after that router is removed,
        // so that the user is shown a host whose router is gone. The file names only the records it
        // carries, so elsewhere such a host stands on its own.
        buildLocalHost(host, group_guids.value(host.groupId()), router_guids.value(host.routerId()),
                       data->add_local_hosts());
        ++report->local_hosts;
    }

    const QList<RouterHostConfig> router_hosts = db.allRouterHosts();
    for (const RouterHostConfig& host : std::as_const(router_hosts))
    {
        if (host.username().isEmpty() || host.password().isEmpty())
        {
            LOG(ERROR) << "Unable to read credentials of router host:" << host.hostId();
            return Backup::Result::INTERNAL_ERROR;
        }

        buildRouterHost(host, router_guids.value(host.routerId()), data->add_router_hosts());
        ++report->router_hosts;
    }

    return Backup::Result::SUCCESS;
}

//--------------------------------------------------------------------------------------------------
// The routers of the file, in the shape the address book takes them. |router_links| says what a
// router of the file is named by in this batch.
void buildRouters(
    const BackupContent& content, QList<RouterConfig>* routers,
    QHash<QString, qint64>* router_links, Backup::Report* report)
{
    for (const BackupRouter& router : content.routers())
    {
        RouterConfig config;
        config.setRouterId(-(routers->size() + 1));
        config.setDisplayName(QString::fromStdString(router.display_name()));
        config.setAddress(QString::fromStdString(router.address()));
        config.setUsername(QString::fromStdString(router.username()));
        config.setPassword(toSecureString(router.password()));
        config.setSessionType(static_cast<proto::router::SessionType>(router.session_type()));
        config.setGuid(QString::fromStdString(router.guid()));

        router_links->insert(config.guid(), config.routerId());
        routers->append(config);
        ++report->routers;
    }
}

//--------------------------------------------------------------------------------------------------
// The groups of the file, parents before their children. |group_links| says what a group of the
// file is named by in this batch, and the root of the file is in it as the root of the book.
void buildLocalGroups(
    const BackupContent& content, QList<LocalGroupConfig>* local_groups,
    QHash<QString, qint64>* group_links, Backup::Report* report)
{
    QHash<QString, QList<const BackupLocalGroup*>> children;
    for (const BackupLocalGroup& group : content.local_groups())
        children[QString::fromStdString(group.parent_guid())].append(&group);

    group_links->insert(QString(), 0);

    QList<QString> queue;
    queue.append(QString());

    // The groups of the file are a tree, so walking it from the root down reaches every group once
    // and never comes back to one it has already passed.
    while (!queue.isEmpty())
    {
        const QString current_parent_of_file = queue.takeFirst();

        const QList<const BackupLocalGroup*>& list = children.value(current_parent_of_file);
        for (const BackupLocalGroup* group : std::as_const(list))
        {
            LocalGroupConfig config;
            config.setId(-(local_groups->size() + 1));
            config.setParentId(group_links->value(current_parent_of_file));
            config.setGuid(QString::fromStdString(group->guid()));
            config.setName(QString::fromStdString(group->name()));
            config.setComment(QString::fromStdString(group->comment()));

            group_links->insert(config.guid(), config.id());
            local_groups->append(config);
            ++report->local_groups;

            queue.append(config.guid());
        }
    }
}

//--------------------------------------------------------------------------------------------------
void buildLocalHosts(
    const BackupContent& content, QList<LocalHostConfig>* local_hosts,
    const QHash<QString, qint64>& group_links, const QHash<QString, qint64>& router_links,
    Backup::Report* report)
{
    for (const BackupLocalHost& host : content.local_hosts())
    {
        LocalHostConfig config;
        config.setGroupId(group_links.value(QString::fromStdString(host.group_guid())));
        config.setRouterId(router_links.value(QString::fromStdString(host.router_guid()), 0));
        config.setGuid(QString::fromStdString(host.guid()));
        config.setName(QString::fromStdString(host.name()));
        config.setComment(QString::fromStdString(host.comment()));
        config.setAddress(QString::fromStdString(host.address()));
        config.setUsername(QString::fromStdString(host.username()));
        config.setPassword(toSecureString(host.password()));
        config.setCreateTime(host.create_time());
        config.setModifyTime(host.modify_time());
        config.setConnectTime(host.connect_time());

        local_hosts->append(config);
        ++report->local_hosts;
    }
}

//--------------------------------------------------------------------------------------------------
void buildRouterHosts(
    const BackupContent& content, QList<RouterHostConfig>* router_hosts,
    const QHash<QString, qint64>& router_links, Backup::Report* report)
{
    for (const BackupRouterHost& host : content.router_hosts())
    {
        RouterHostConfig config;
        config.setRouterId(router_links.value(QString::fromStdString(host.router_guid())));
        config.setHostId(host.host_id());
        config.setUsername(QString::fromStdString(host.username()));
        config.setPassword(toSecureString(host.password()));

        router_hosts->append(config);
        ++report->router_hosts;
    }
}

//--------------------------------------------------------------------------------------------------
// The session types a router record can carry. The two router editors offer no others.
bool isValidSessionType(quint32 session_type)
{
    return session_type == proto::router::SESSION_TYPE_ADMIN ||
           session_type == proto::router::SESSION_TYPE_OPERATOR ||
           session_type == proto::router::SESSION_TYPE_MANAGER;
}

//--------------------------------------------------------------------------------------------------
// A guid this application hands out. Read on another machine, it is what says that two records
// are one record.
bool isValidGuid(const QString& guid)
{
    return !guid.isEmpty() && QUuid::fromString(guid).toString(QUuid::WithoutBraces) == guid;
}

//--------------------------------------------------------------------------------------------------
bool hasValidRouters(const BackupContent& content)
{
    QSet<QString> guids;

    for (const BackupRouter& router : content.routers())
    {
        // A record of the file is a record the address book would take.
        RouterConfig config;
        config.setDisplayName(QString::fromStdString(router.display_name()));
        config.setAddress(QString::fromStdString(router.address()));
        config.setUsername(QString::fromStdString(router.username()));
        config.setPassword(toSecureString(router.password()));

        if (!config.isValid())
            return false;

        if (!isValidSessionType(router.session_type()))
            return false;

        const QString guid = QString::fromStdString(router.guid());
        if (!isValidGuid(guid) || guids.contains(guid))
            return false;

        guids.insert(guid);
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool hasValidLocalGroups(const BackupContent& content, const QSet<QString>& group_guids)
{
    QSet<QString> guids;

    for (const BackupLocalGroup& group : content.local_groups())
    {
        LocalGroupConfig config;
        config.setName(QString::fromStdString(group.name()));
        config.setComment(QString::fromStdString(group.comment()));

        if (!config.isValid())
            return false;

        const QString guid = QString::fromStdString(group.guid());
        if (!isValidGuid(guid) || guids.contains(guid))
            return false;

        guids.insert(guid);

        // A group lies at the root of the file or under another group the file carries.
        const QString parent_guid = QString::fromStdString(group.parent_guid());
        if (!parent_guid.isEmpty() && !group_guids.contains(parent_guid))
            return false;
    }

    // Walking parents upward from any group ends at the root. A ring of groups is not a tree, and
    // the walk over it would never end.
    QHash<QString, QString> parents;
    for (const BackupLocalGroup& group : content.local_groups())
    {
        parents.insert(QString::fromStdString(group.guid()),
                       QString::fromStdString(group.parent_guid()));
    }

    for (auto it = parents.constBegin(); it != parents.constEnd(); ++it)
    {
        QString current = it.value();
        for (int steps = 0; !current.isEmpty(); ++steps)
        {
            if (steps > parents.size())
                return false;

            current = parents.value(current);
        }
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool hasValidLocalHosts(
    const BackupContent& content, const QSet<QString>& group_guids,
    const QSet<QString>& router_guids)
{
    QSet<QString> guids;

    for (const BackupLocalHost& host : content.local_hosts())
    {
        LocalHostConfig config;
        config.setName(QString::fromStdString(host.name()));
        config.setComment(QString::fromStdString(host.comment()));
        config.setAddress(QString::fromStdString(host.address()));
        config.setUsername(QString::fromStdString(host.username()));
        config.setPassword(toSecureString(host.password()));

        if (!config.isValid())
            return false;

        const QString guid = QString::fromStdString(host.guid());
        if (!isValidGuid(guid) || guids.contains(guid))
            return false;

        guids.insert(guid);

        // A host lies at the root of the file or in a group the file carries, and reaches either no
        // router or one of the file.
        const QString group_guid = QString::fromStdString(host.group_guid());
        if (!group_guid.isEmpty() && !group_guids.contains(group_guid))
            return false;

        const QString router_guid = QString::fromStdString(host.router_guid());
        if (!router_guid.isEmpty() && !router_guids.contains(router_guid))
            return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool hasValidRouterHosts(const BackupContent& content, const QSet<QString>& router_guids)
{
    QSet<QPair<QString, HostId>> hosts;

    for (const BackupRouterHost& host : content.router_hosts())
    {
        const QString router_guid = QString::fromStdString(host.router_guid());
        if (!router_guids.contains(router_guid))
            return false;

        RouterHostConfig config;
        // The record is written against a number the book hands out on the way in, and the router
        // it belongs to is named above. Any number that is not zero stands for it here.
        config.setRouterId(1);
        config.setHostId(host.host_id());
        config.setUsername(QString::fromStdString(host.username()));
        config.setPassword(toSecureString(host.password()));

        if (!config.isValid())
            return false;

        const QPair<QString, HostId> key(router_guid, host.host_id());
        if (hosts.contains(key))
            return false;

        hosts.insert(key);
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
// The file is written by this same application, so what it carries is what an address book holds.
// A file that breaks any of this is not one of ours and nothing of it is taken, because a record
// that cannot be believed says nothing about the records next to it either.
bool isValidContent(const BackupContent& content)
{
    QSet<QString> router_guids;
    for (const BackupRouter& router : content.routers())
        router_guids.insert(QString::fromStdString(router.guid()));

    QSet<QString> group_guids;
    for (const BackupLocalGroup& group : content.local_groups())
        group_guids.insert(QString::fromStdString(group.guid()));

    return hasValidRouters(content) && hasValidLocalGroups(content, group_guids) &&
           hasValidLocalHosts(content, group_guids, router_guids) &&
           hasValidRouterHosts(content, router_guids);
}

//--------------------------------------------------------------------------------------------------
// Reads the sealed file. Nothing of what it carries is opened here.
Backup::Result readFile(const QString& file_path, proto::storage::BackupFile* file_message)
{
    QFile file(file_path);
    if (!file.open(QIODevice::ReadOnly))
    {
        LOG(ERROR) << "Unable to open file" << file_path << ":" << file.errorString();
        return Backup::Result::FILE_ERROR;
    }

    const QByteArray buffer = file.readAll();
    file.close();

    if (buffer.isEmpty())
        return Backup::Result::INVALID_FORMAT;

    if (!parse(buffer, file_message))
    {
        LOG(ERROR) << "Unable to parse backup file";
        return Backup::Result::INVALID_FORMAT;
    }

    if (fileSalt(*file_message).size() != kSaltSize || file_message->verifier().empty() ||
        file_message->data().empty())
    {
        return Backup::Result::INVALID_FORMAT;
    }

    if (file_message->version() != kFormatVersion)
        return Backup::Result::UNSUPPORTED_VERSION;

    return Backup::Result::SUCCESS;
}

//--------------------------------------------------------------------------------------------------
// Opens the address book of the file and hands it back in the clear. What comes out is the
// caller's to wipe. An empty |password| means the key the book is already open with.
Backup::Result openContent(
    const proto::storage::BackupFile& file_message, const SecureString& password,
    BackupContent* content)
{
    SecureByteArray key = MasterPassword::currentKey();

    if (!password.isEmpty())
        key = SecureByteArray(PasswordHash::hash(
            PasswordHash::ARGON2ID, password, fileSalt(file_message)));

    DataCryptor cryptor(CipherType::AES256_GCM, key);

    // Tells a key that does not open this file from a file that is damaged.
    if (!cryptor.decrypt(QByteArray::fromStdString(file_message.verifier())).has_value())
        return Backup::Result::WRONG_PASSWORD;

    std::optional<QByteArray> decrypted =
        cryptor.decrypt(QByteArray::fromStdString(file_message.data()));
    if (!decrypted.has_value())
    {
        // The key is already proven right, so this is damage.
        LOG(ERROR) << "Unable to decrypt address book";
        return Backup::Result::INVALID_FORMAT;
    }

    const SecureByteArray plain(std::move(*decrypted));
    if (!parse(plain.toByteArray(), content))
    {
        LOG(ERROR) << "Unable to parse address book";
        eraseSecretFields(content);
        content->Clear();
        return Backup::Result::INVALID_FORMAT;
    }

    if (!isValidContent(*content))
    {
        LOG(ERROR) << "The file does not carry an address book of this application";
        eraseSecretFields(content);
        content->Clear();
        return Backup::Result::INVALID_FORMAT;
    }

    return Backup::Result::SUCCESS;
}

//--------------------------------------------------------------------------------------------------
// Writes the address book of the file into the database, in place of the one it holds. Everything
// the file is read for happens before a single row is written, and what the database is handed is
// a batch it takes in one go or not at all.
Backup::Result importContent(Database& db, const BackupContent& content, Backup::Report* report)
{
    QList<RouterConfig> routers;
    QList<LocalGroupConfig> local_groups;
    QList<LocalHostConfig> local_hosts;
    QList<RouterHostConfig> router_hosts;

    Backup::Report counted;

    QHash<QString, qint64> router_links;
    buildRouters(content, &routers, &router_links, &counted);

    QHash<QString, qint64> group_links;
    buildLocalGroups(content, &local_groups, &group_links, &counted);

    buildLocalHosts(content, &local_hosts, group_links, router_links, &counted);
    buildRouterHosts(content, &router_hosts, router_links, &counted);

    // A file with nothing in it says nothing about what the book should hold, so the book is left
    // alone instead of being emptied.
    if (counted.total() == 0)
        return Backup::Result::NOTHING_IMPORTED;

    if (!db.import(routers, local_groups, local_hosts, router_hosts))
    {
        LOG(ERROR) << "Unable to write the address book of the file";
        return Backup::Result::INTERNAL_ERROR;
    }

    // The batch goes into the book whole or not at all, so the tally is handed over only once it
    // is there.
    if (report)
        *report = counted;

    return Backup::Result::SUCCESS;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
Backup::Result Backup::exportToFile(Database& db, const QString& file_path, Report* report)
{
    if (!db.isValid())
        return Result::DATABASE_UNAVAILABLE;

    // The file is sealed with the key of this address book, so what proves that key right goes into
    // it as it is. Nothing of the key itself does.
    const QByteArray salt = db.masterPasswordSalt();
    const QByteArray verifier = db.masterPasswordVerifier();
    const SecureByteArray key = MasterPassword::currentKey();

    if (salt.isEmpty() || verifier.isEmpty() || key.isEmpty())
    {
        LOG(ERROR) << "Address book is locked";
        return Result::DATABASE_UNAVAILABLE;
    }

    Report counted;
    BackupContent data;

    const Result collected = collectContent(db, &data, &counted);
    if (collected != Result::SUCCESS)
    {
        eraseSecretFields(&data);
        return collected;
    }

    // An address book with nothing in it seals into an empty payload, which is not a file anything
    // could be read back from.
    if (counted.total() == 0)
        return Result::NOTHING_EXPORTED;

    DataCryptor cryptor(CipherType::AES256_GCM, key);

    // Serialized, the address book is in the clear, so it goes into a buffer that wipes itself.
    std::optional<QByteArray> sealed;
    {
        const SecureByteArray buffer(serialize(data));
        sealed = cryptor.encrypt(buffer.toByteArray());
    }

    eraseSecretFields(&data);

    if (!sealed.has_value())
    {
        LOG(ERROR) << "Unable to encrypt address book";
        return Result::INTERNAL_ERROR;
    }

    proto::storage::BackupFile file_message;
    file_message.set_version(kFormatVersion);
    file_message.set_salt(salt.toStdString());
    file_message.set_verifier(verifier.toStdString());
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

    // Counted while the book was read, handed over only once the file is on disk.
    if (report)
        *report = counted;

    return Result::SUCCESS;
}

//--------------------------------------------------------------------------------------------------
// static
Backup::Result Backup::importFromFile(
    Database& db, const QString& file_path, const SecureString& password, Report* report)
{
    if (!db.isValid())
        return Result::DATABASE_UNAVAILABLE;

    proto::storage::BackupFile file_message;

    Result result = readFile(file_path, &file_message);
    if (result != Result::SUCCESS)
        return result;

    BackupContent content;

    result = openContent(file_message, password, &content);
    if (result != Result::SUCCESS)
        return result;

    result = importContent(db, content, report);

    eraseSecretFields(&content);
    return result;
}
