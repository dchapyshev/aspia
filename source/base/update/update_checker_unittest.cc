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

#include "base/update/update_checker.h"

#include <QHash>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>

#include <gtest/gtest.h>

#include <memory>

#include "base/build_config.h"
#include "base/version_constants.h"
#include "base/crypto/secure_byte_array.h"
#include "base/crypto/signature.h"

namespace {

// The update server is a set of static files, so the tests are served by one: it answers the paths
// it was given and 404 to everything else.
class FileServer final : public QTcpServer
{
public:
    explicit FileServer(const QHash<QString, QByteArray>& files)
        : files_(files)
    {
        EXPECT_TRUE(listen(QHostAddress::LocalHost));
    }

    QString url() const { return QString("http://127.0.0.1:%1").arg(serverPort()); }

protected:
    void incomingConnection(qintptr handle) final
    {
        QTcpSocket* socket = new QTcpSocket(this);
        if (!socket->setSocketDescriptor(handle))
        {
            delete socket;
            return;
        }

        connect(socket, &QTcpSocket::readyRead, this, [this, socket]()
        {
            QByteArray request = socket->readAll();
            if (!request.contains("\r\n\r\n"))
                return;

            QList<QByteArray> parts = request.split(' ');
            QString path = parts.size() > 1 ? QString::fromLatin1(parts[1]) : QString();

            auto file = files_.constFind(path);
            QByteArray body = file != files_.constEnd() ? *file : QByteArray();
            const char* status = file != files_.constEnd() ? "200 OK" : "404 Not Found";

            QByteArray answer = QByteArray("HTTP/1.1 ") + status + "\r\n"
                "Content-Type: application/json\r\n"
                "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
                "Connection: close\r\n\r\n" + body;

            socket->write(answer);
            socket->disconnectFromHost();
        });

        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    }

private:
    const QHash<QString, QByteArray> files_;
};

//--------------------------------------------------------------------------------------------------
// A release ahead of this build, so what the rules offer is always an update.
QVersionNumber nextVersion()
{
    return QVersionNumber(kCurrentVersion.majorVersion(), kCurrentVersion.minorVersion(),
                          kCurrentVersion.microVersion() + 1);
}

//--------------------------------------------------------------------------------------------------
QByteArray rules()
{
    return QString(R"({
        "format": 1,
        "targets": { "latest": "%1" },
        "updates": [ { "source": "%2", "target": "@latest" } ]
    })").arg(nextVersion().toString(), kCurrentVersion.toString()).toUtf8();
}

//--------------------------------------------------------------------------------------------------
// Every platform the release is built for, so the answer does not depend on where the test runs.
QByteArray manifest()
{
    QString files = R"([ { "format": "%1", "url": "https://files.aspia.net/beta/%2/%3",
        "sha256": "9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08" } ])";

    QString version = nextVersion().toString();
    QString msi = files.arg("msi", version, "aspia-host.msi");
    QString deb = files.arg("deb", version, "aspia-host.deb");
    QString pkg = files.arg("pkg", version, "aspia-host.pkg");
    QString apk = files.arg("apk", version, "aspia-host.apk");

    return QString(R"({
        "format": 1,
        "version": "%1",
        "description": "A new version of the program.",
        "packages": {
            "host": {
                "windows": { "x86_64": %2, "x86": %2, "arm64": %2 },
                "linux":   { "x86_64": %3, "arm64": %3 },
                "macosx":  { "x86_64": %4, "arm64": %4 },
                "android": { "arm64": %5, "arm": %5 }
            }
        }
    })").arg(version, msi, deb, pkg, apk).toUtf8();
}

//--------------------------------------------------------------------------------------------------
// The key of the tests. Test vector 1 of RFC 8032, section 7.1, so that nothing here depends on
// randomness.
SecureByteArray privateKey()
{
    return SecureByteArray(QByteArray::fromHex(
        "9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60"));
}

//--------------------------------------------------------------------------------------------------
// The real server holds a signature next to every file, and so does the one of the tests.
QHash<QString, QByteArray> withSignatures(const QHash<QString, QByteArray>& files)
{
    QHash<QString, QByteArray> result;

    for (auto it = files.constBegin(); it != files.constEnd(); ++it)
    {
        result.insert(it.key(), it.value());
        result.insert(it.key() + ".sig", Signature::create(privateKey(), it.value()).toBase64());
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
std::unique_ptr<UpdateChecker> checkerFor(const FileServer& server)
{
    std::unique_ptr<UpdateChecker> checker =
        std::make_unique<UpdateChecker>(kStableUpdateChannel, "host");

    checker->setServerForTesting(server.url());
    checker->setPublicKeysForTesting({ Signature::publicKey(privateKey()) });
    return checker;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// The whole check: the rules name a version, the manifest of that version names a file for this
// platform, and the answer carries what the download needs.
TEST(UpdateCheckerTest, UpdateIsOffered)
{
    FileServer server(withSignatures(
        { { "/latest.json", rules() },
          { QString("/%1.json").arg(nextVersion().toString()), manifest() } }));

    std::unique_ptr<UpdateChecker> checker = checkerFor(server);
    QSignalSpy spy(checker.get(), &UpdateChecker::sig_checkFinished);
    QSignalSpy failed(checker.get(), &UpdateChecker::sig_checkFailed);

    checker->start();

    ASSERT_TRUE(spy.wait(15000));
    EXPECT_TRUE(failed.isEmpty());

    UpdateInfo update_info = spy.takeFirst().at(0).value<UpdateInfo>();

    ASSERT_TRUE(update_info.isValid());
    EXPECT_EQ(update_info.version(), nextVersion());
    EXPECT_EQ(update_info.description(), QString("A new version of the program."));
    EXPECT_TRUE(update_info.url().startsWith("https://files.aspia.net/beta/"));
    EXPECT_EQ(update_info.sha256().size(), 64);
}

//--------------------------------------------------------------------------------------------------
// No rule for this version is the usual answer of a server that has nothing to offer, not a
// failure, and the manifest is not read at all.
TEST(UpdateCheckerTest, VersionWithoutRuleIsNotOffered)
{
    QByteArray empty_rules = R"({ "format": 1, "targets": {}, "updates": [] })";

    FileServer server(withSignatures({ { "/latest.json", empty_rules } }));

    std::unique_ptr<UpdateChecker> checker = checkerFor(server);
    QSignalSpy spy(checker.get(), &UpdateChecker::sig_checkFinished);
    QSignalSpy failed(checker.get(), &UpdateChecker::sig_checkFailed);

    checker->start();

    ASSERT_TRUE(spy.wait(15000));
    EXPECT_TRUE(failed.isEmpty());
    EXPECT_FALSE(spy.takeFirst().at(0).value<UpdateInfo>().isValid());
}

//--------------------------------------------------------------------------------------------------
// A release built for other platforms: the rules offer it, the manifest has no file for this one.
TEST(UpdateCheckerTest, ReleaseWithoutFilesIsNotOffered)
{
    QByteArray other = QString(R"({
        "format": 1,
        "version": "%1",
        "packages": { "client": {} }
    })").arg(nextVersion().toString()).toUtf8();

    FileServer server(withSignatures(
        { { "/latest.json", rules() },
          { QString("/%1.json").arg(nextVersion().toString()), other } }));

    std::unique_ptr<UpdateChecker> checker = checkerFor(server);
    QSignalSpy spy(checker.get(), &UpdateChecker::sig_checkFinished);

    checker->start();

    ASSERT_TRUE(spy.wait(15000));
    EXPECT_FALSE(spy.takeFirst().at(0).value<UpdateInfo>().isValid());
}

//--------------------------------------------------------------------------------------------------
// A server that answers nothing is a failure of its own: the user is told the check did not
// happen, rather than that there are no updates.
TEST(UpdateCheckerTest, UnreadableServerFails)
{
    FileServer server({});

    std::unique_ptr<UpdateChecker> checker = checkerFor(server);
    QSignalSpy spy(checker.get(), &UpdateChecker::sig_checkFinished);
    QSignalSpy failed(checker.get(), &UpdateChecker::sig_checkFailed);

    checker->start();

    ASSERT_TRUE(failed.wait(15000));
    EXPECT_TRUE(spy.isEmpty());
}

//--------------------------------------------------------------------------------------------------
// The manifest named by the rules is missing: the release is not there, whatever the rules say.
TEST(UpdateCheckerTest, MissingManifestFails)
{
    FileServer server(withSignatures({ { "/latest.json", rules() } }));

    std::unique_ptr<UpdateChecker> checker = checkerFor(server);
    QSignalSpy spy(checker.get(), &UpdateChecker::sig_checkFinished);
    QSignalSpy failed(checker.get(), &UpdateChecker::sig_checkFailed);

    checker->start();

    ASSERT_TRUE(failed.wait(15000));
    EXPECT_TRUE(spy.isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A file the server answers without a signature next to it is refused.
TEST(UpdateCheckerTest, UnsignedFileFails)
{
    FileServer server({ { "/latest.json", rules() } });

    std::unique_ptr<UpdateChecker> checker = checkerFor(server);
    QSignalSpy spy(checker.get(), &UpdateChecker::sig_checkFinished);
    QSignalSpy failed(checker.get(), &UpdateChecker::sig_checkFailed);

    checker->start();

    ASSERT_TRUE(failed.wait(15000));
    EXPECT_TRUE(spy.isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A signature is worth something only when it is ours. Test vector 2 of RFC 8032 stands for the
// key of somebody else.
TEST(UpdateCheckerTest, FileSignedByAnotherKeyFails)
{
    SecureByteArray another_key(QByteArray::fromHex(
        "4ccd089b28ff96da9db6c346ec114e0f5b8a319f35aba624da8cf6ed4fb8a6fb"));

    FileServer server({ { "/latest.json", rules() },
                        { "/latest.json.sig",
                          Signature::create(another_key, rules()).toBase64() } });

    std::unique_ptr<UpdateChecker> checker = checkerFor(server);
    QSignalSpy spy(checker.get(), &UpdateChecker::sig_checkFinished);
    QSignalSpy failed(checker.get(), &UpdateChecker::sig_checkFailed);

    checker->start();

    ASSERT_TRUE(failed.wait(15000));
    EXPECT_TRUE(spy.isEmpty());
}

//--------------------------------------------------------------------------------------------------
// The manifest is signed, but not in the shape it arrives in.
TEST(UpdateCheckerTest, ModifiedManifestFails)
{
    QString manifest_path = QString("/%1.json").arg(nextVersion().toString());

    QHash<QString, QByteArray> files = withSignatures(
        { { "/latest.json", rules() },
          { manifest_path, manifest() } });

    files[manifest_path] = QByteArray(manifest()).replace("A new version", "Another version");

    FileServer server(files);

    std::unique_ptr<UpdateChecker> checker = checkerFor(server);
    QSignalSpy spy(checker.get(), &UpdateChecker::sig_checkFinished);
    QSignalSpy failed(checker.get(), &UpdateChecker::sig_checkFailed);

    checker->start();

    ASSERT_TRUE(failed.wait(15000));
    EXPECT_TRUE(spy.isEmpty());
}

//--------------------------------------------------------------------------------------------------
// The rules name a label that is not there. Nothing can be offered, but that is a mistake in the
// published file and the user is told the check did not happen.
TEST(UpdateCheckerTest, RulesWithUnknownLabelFail)
{
    QByteArray broken_rules = QByteArray(rules()).replace("@latest", "@lastest");
    ASSERT_NE(broken_rules, rules());

    FileServer server(withSignatures({ { "/latest.json", broken_rules } }));

    std::unique_ptr<UpdateChecker> checker = checkerFor(server);
    QSignalSpy spy(checker.get(), &UpdateChecker::sig_checkFinished);
    QSignalSpy failed(checker.get(), &UpdateChecker::sig_checkFailed);

    checker->start();

    ASSERT_TRUE(failed.wait(15000));
    EXPECT_TRUE(spy.isEmpty());
}

//--------------------------------------------------------------------------------------------------
// The rules and the manifest disagree about which release this is. Both are signed, so it is a
// broken publication rather than something to pass off as an absence of updates.
TEST(UpdateCheckerTest, ManifestOfAnotherVersionFails)
{
    QVersionNumber other(nextVersion().majorVersion(), nextVersion().minorVersion(),
                         nextVersion().microVersion() + 1);

    QByteArray other_manifest = QByteArray(manifest()).replace(
        QString("\"version\": \"%1\"").arg(nextVersion().toString()).toUtf8(),
        QString("\"version\": \"%1\"").arg(other.toString()).toUtf8());
    ASSERT_NE(other_manifest, manifest());

    FileServer server(withSignatures(
        { { "/latest.json", rules() },
          { QString("/%1.json").arg(nextVersion().toString()), other_manifest } }));

    std::unique_ptr<UpdateChecker> checker = checkerFor(server);
    QSignalSpy spy(checker.get(), &UpdateChecker::sig_checkFinished);
    QSignalSpy failed(checker.get(), &UpdateChecker::sig_checkFailed);

    checker->start();

    ASSERT_TRUE(failed.wait(15000));
    EXPECT_TRUE(spy.isEmpty());
}
