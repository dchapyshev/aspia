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

#include "client/address_book_importer.h"

#include <QFile>

#include "base/logging.h"
#include "base/serialization.h"
#include "base/crypto/data_cryptor.h"
#include "base/crypto/password_hash.h"
#include "base/crypto/secure_memory.h"
#include "base/net/address.h"
#include "base/peer/host_id.h"
#include "build/build_config.h"
#include "client/database.h"
#include "client/ui/unlock_dialog.h"
#include "common/ui/msg_box.h"
#include "proto/address_book.h"

namespace client {

namespace {

constexpr int kMaxNameLength = 64;
constexpr int kMaxCommentLength = 2048;
constexpr int kMaxRecursionDepth = 32;

//--------------------------------------------------------------------------------------------------
struct InheritedCredentials
{
    QString username;
    QString password;
};

//--------------------------------------------------------------------------------------------------
struct ImportCounters
{
    int groups = 0;
    int computers = 0;
    int computers_skipped = 0;
    int routers = 0;
};

//--------------------------------------------------------------------------------------------------
QString combineHostAndPort(const QString& host, quint32 port)
{
    if (host.isEmpty())
        return QString();

    if (base::isHostId(host))
        return host;

    base::Address address(DEFAULT_HOST_TCP_PORT);
    address.setHost(host);
    address.setPort(port == 0 ? DEFAULT_HOST_TCP_PORT : static_cast<quint16>(port));

    return address.toString();
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
qint64 ensureRouter(const proto::address_book::Router& proto_router, ImportCounters* counters)
{
    QString address = QString::fromStdString(proto_router.address());
    QString username = QString::fromStdString(proto_router.username());
    QString password = QString::fromStdString(proto_router.password());

    if (address.isEmpty() || username.isEmpty())
        return 0;

    QString combined_address = combineHostAndPort(address, proto_router.port());

    Database& db = Database::instance();
    const QList<RouterConfig> routers = db.routerList();

    for (const RouterConfig& router : std::as_const(routers))
    {
        if (router.address == combined_address && router.username == username)
            return router.router_id;
    }

    RouterConfig config;
    config.display_name = AddressBookImporter::tr("%1 (Imported)").arg(address);
    config.address = combined_address;
    config.username = username;
    config.password = password;
    config.session_type = proto::router::SESSION_TYPE_CLIENT;

    if (!db.addRouter(config))
    {
        LOG(ERROR) << "Unable to add router during import";
        return 0;
    }

    ++counters->routers;
    return config.router_id;
}

//--------------------------------------------------------------------------------------------------
bool importComputer(const proto::address_book::Computer& proto_computer,
                    qint64 group_id,
                    qint64 router_id,
                    const InheritedCredentials& inherited,
                    ImportCounters* counters)
{
    QString name = sanitizedName(QString::fromStdString(proto_computer.name()));
    if (name.isEmpty())
    {
        ++counters->computers_skipped;
        LOG(INFO) << "Skip computer with empty name";
        return false;
    }

    QString comment = sanitizedComment(QString::fromStdString(proto_computer.comment()));
    QString address_raw = QString::fromStdString(proto_computer.address());

    QString address = combineHostAndPort(address_raw, proto_computer.port());
    if (address.isEmpty())
    {
        ++counters->computers_skipped;
        LOG(INFO) << "Skip computer with empty address:" << name;
        return false;
    }

    QString username;
    QString password;

    if (proto_computer.has_inherit() && proto_computer.inherit().credentials())
    {
        username = inherited.username;
        password = inherited.password;
    }
    else
    {
        username = QString::fromStdString(proto_computer.username());
        password = QString::fromStdString(proto_computer.password());
    }

    qint64 effective_router_id = base::isHostId(address) ? router_id : 0;

    ComputerConfig config;
    config.group_id = group_id;
    config.router_id = effective_router_id;
    config.name = name;
    config.comment = comment;
    config.address = address;
    config.username = username;
    config.password = password;

    if (!Database::instance().addComputer(config))
    {
        LOG(ERROR) << "Unable to add computer to local database";
        return false;
    }

    ++counters->computers;
    return true;
}

//--------------------------------------------------------------------------------------------------
void importGroup(const proto::address_book::ComputerGroup& proto_group,
                 qint64 parent_group_id,
                 qint64 router_id,
                 const InheritedCredentials& inherited_from_parent,
                 int depth,
                 ImportCounters* counters)
{
    if (depth > kMaxRecursionDepth)
    {
        LOG(ERROR) << "Maximum recursion depth exceeded during import";
        return;
    }

    qint64 group_id = parent_group_id;

    // At depth 0 the imported root group is transparent: its children go directly into the
    // destination root. Only nested groups are materialized.
    if (depth > 0)
    {
        QString group_name = sanitizedName(QString::fromStdString(proto_group.name()));
        if (group_name.isEmpty())
        {
            LOG(INFO) << "Skip group with empty name";
            return;
        }

        GroupConfig group_config;
        group_config.parent_id = parent_group_id;
        group_config.name = group_name;
        group_config.comment = sanitizedComment(QString::fromStdString(proto_group.comment()));
        group_config.expanded = proto_group.expanded();

        if (!Database::instance().addGroup(group_config))
        {
            LOG(ERROR) << "Unable to add group to local database";
            return;
        }

        group_id = group_config.id;
        ++counters->groups;
    }

    InheritedCredentials inherited_for_children = inherited_from_parent;

    if (proto_group.has_config())
    {
        const proto::address_book::ComputerGroupConfig& group_config = proto_group.config();
        const bool inherit_from_parent =
            group_config.has_inherit() && group_config.inherit().credentials();

        if (!inherit_from_parent)
        {
            inherited_for_children.username = QString::fromStdString(group_config.username());
            inherited_for_children.password = QString::fromStdString(group_config.password());
        }
    }

    for (int i = 0; i < proto_group.computer_size(); ++i)
        importComputer(proto_group.computer(i), group_id, router_id, inherited_for_children, counters);

    for (int i = 0; i < proto_group.computer_group_size(); ++i)
    {
        importGroup(proto_group.computer_group(i),
                    group_id,
                    router_id,
                    inherited_for_children,
                    depth + 1,
                    counters);
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
bool AddressBookImporter::import(QWidget* parent, const QString& file_path)
{
    QFile file(file_path);
    if (!file.open(QIODevice::ReadOnly))
    {
        common::MsgBox::warning(parent,
            tr("Unable to open file \"%1\": %2").arg(file_path, file.errorString()));
        return false;
    }

    QByteArray buffer = file.readAll();
    file.close();

    if (buffer.isEmpty())
    {
        common::MsgBox::warning(parent, tr("Selected file is empty."));
        return false;
    }

    proto::address_book::File proto_file;
    if (!proto_file.ParseFromArray(buffer.constData(), buffer.size()))
    {
        common::MsgBox::warning(parent,
            tr("The address book file is corrupted or has an unknown format."));
        return false;
    }

    QByteArray key;

    switch (proto_file.encryption_type())
    {
        case proto::address_book::ENCRYPTION_TYPE_NONE:
            break;

        case proto::address_book::ENCRYPTION_TYPE_CHACHA20_POLY1305:
        {
            UnlockDialog dialog(parent, file_path, tr("ChaCha20 + Poly1305 (256-bit key)"));
            if (dialog.exec() != QDialog::Accepted)
                return false;

            key = base::PasswordHash::hash(
                base::PasswordHash::SCRYPT,
                dialog.password(),
                QByteArray::fromStdString(proto_file.hashing_salt()));
            break;
        }

        default:
            common::MsgBox::warning(parent,
                tr("The address book file is encrypted with an unsupported encryption type."));
            return false;
    }

    base::DataCryptor cryptor(key);

    std::optional<QByteArray> decrypted = cryptor.decrypt(proto_file.data());
    if (!decrypted.has_value())
    {
        base::memZero(&key);
        common::MsgBox::warning(parent,
            tr("Unable to decrypt the address book with the specified password."));
        return false;
    }

    proto::address_book::Data proto_data;
    if (!base::parse(*decrypted, &proto_data))
    {
        base::memZero(&*decrypted);
        base::memZero(&key);
        common::MsgBox::warning(parent,
            tr("The address book file is corrupted or has an unknown format."));
        return false;
    }

    base::memZero(&*decrypted);
    base::memZero(&key);

    ImportCounters counters;

    qint64 router_id = 0;
    if (proto_data.enable_router())
        router_id = ensureRouter(proto_data.router(), &counters);

    InheritedCredentials inherited;
    importGroup(proto_data.root_group(),
                /* parent_group_id */ 0,
                router_id,
                inherited,
                /* depth */ 0,
                &counters);

    if (counters.groups == 0 && counters.computers == 0 && counters.routers == 0)
    {
        common::MsgBox::information(parent, tr("Nothing was imported."));
        return false;
    }

    common::MsgBox::information(parent,
        tr("Import completed successfully.\n"
           "Groups added: %1\n"
           "Computers added: %2\n"
           "Computers skipped: %3\n"
           "Routers added: %4")
            .arg(counters.groups)
            .arg(counters.computers)
            .arg(counters.computers_skipped)
            .arg(counters.routers));

    return true;
}

} // namespace client
