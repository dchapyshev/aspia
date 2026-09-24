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

#include "base/update/update_info.h"

#include <QString>

#include <gtest/gtest.h>

namespace {

const char kPath[] = "https://aspia.org/download";
const char kFile[] = "aspia-host-3.0.6-x86_64.msi";
const char kUrl[] = "https://aspia.org/download/aspia-host-3.0.6-x86_64.msi";
const char kSha256[] = "9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08";

//--------------------------------------------------------------------------------------------------
// The rules of a channel: which version each released one is offered. A target may name a label
// instead of a version.
QByteArray rulesJson()
{
    return QByteArray(R"({
        "format": 1,
        "targets": { "latest": "3.0.6" },
        "updates": [
            { "source": "2.6.0", "target": "3.0.0" },
            { "source": "3.0.0", "target": "@latest" }
        ]
    })");
}

//--------------------------------------------------------------------------------------------------
// <version>.json: the files of one release. The directory they lie in is named once, so an entry
// carries the file name alone.
QByteArray manifestJson(const QString& version, const QString& description, const QString& file,
                        const QString& sha256)
{
    QString json = QString(R"({
        "format": 1,
        "version": "%1",
        "description": "%2",
        "path": "https://aspia.org/download",
        "packages": {
            "host": {
                "windows": {
                    "x86_64": [ { "format": "msi", "file": "%3", "sha256": "%4" } ]
                },
                "linux": {
                    "x86_64": [
                        { "format": "deb", "file": "host.deb", "sha256": "%4" },
                        { "format": "rpm", "file": "host.rpm", "sha256": "%4" }
                    ]
                }
            }
        }
    })").arg(version, description, file, sha256);

    return json.toUtf8();
}

//--------------------------------------------------------------------------------------------------
QByteArray manifestJson()
{
    return manifestJson("3.0.6", "A new version of the program.", kFile, kSha256);
}

} // namespace

//--------------------------------------------------------------------------------------------------
TEST(UpdateInfoTest, TargetIsTakenFromRules)
{
    EXPECT_EQ(UpdateInfo::targetVersion(rulesJson(), QVersionNumber(2, 6, 0)),
              QVersionNumber(3, 0, 0));
}

//--------------------------------------------------------------------------------------------------
// The label is where the current version is written down, so a rule pointing at it resolves to
// whatever the label holds.
TEST(UpdateInfoTest, LabelIsResolved)
{
    EXPECT_EQ(UpdateInfo::targetVersion(rulesJson(), QVersionNumber(3, 0, 0)),
              QVersionNumber(3, 0, 6));
}

//--------------------------------------------------------------------------------------------------
// A version nobody wrote a rule for gets nothing, and so does the version the label names: no rule
// has it as a source until the next release is out.
TEST(UpdateInfoTest, VersionWithoutRuleIsNotOffered)
{
    std::optional<QVersionNumber> unsupported =
        UpdateInfo::targetVersion(rulesJson(), QVersionNumber(2, 7, 0));
    ASSERT_TRUE(unsupported.has_value());
    EXPECT_TRUE(unsupported->isNull());

    std::optional<QVersionNumber> newest =
        UpdateInfo::targetVersion(rulesJson(), QVersionNumber(3, 0, 6));
    ASSERT_TRUE(newest.has_value());
    EXPECT_TRUE(newest->isNull());
}

//--------------------------------------------------------------------------------------------------
// A label the rules do not carry is a mistake in them, not an absence of updates.
TEST(UpdateInfoTest, UnknownLabelIsRejected)
{
    QByteArray rules = R"({
        "format": 1,
        "targets": { "latest": "3.0.6" },
        "updates": [ { "source": "3.0.0", "target": "@stable" } ]
    })";

    EXPECT_FALSE(UpdateInfo::targetVersion(rules, QVersionNumber(3, 0, 0)).has_value());
}

//--------------------------------------------------------------------------------------------------
// Trailing zeros do not make another version: 3.0 and 3.0.0 are the same release.
TEST(UpdateInfoTest, SourceIsComparedAsVersion)
{
    QByteArray rules = R"({
        "format": 1,
        "updates": [ { "source": "3.0", "target": "3.0.6" } ]
    })";

    EXPECT_EQ(UpdateInfo::targetVersion(rules, QVersionNumber(3, 0, 0)), QVersionNumber(3, 0, 6));
}

//--------------------------------------------------------------------------------------------------
TEST(UpdateInfoTest, BrokenRulesAreRejected)
{
    const char* kAnswers[] =
    {
        "",
        "not a json at all",
        "[]",
        "<html><body>404 Not Found</body></html>",
        R"({ "format": 2, "updates": [ { "source": "3.0.0", "target": "3.0.6" } ] })",
        R"({ "updates": [ { "source": "3.0.0", "target": "3.0.6" } ] })",
        R"({ "format": 1, "updates": [ { "source": "3.0.0", "target": "" } ] })"
    };

    for (const char* answer : kAnswers)
    {
        EXPECT_FALSE(
            UpdateInfo::targetVersion(QByteArray(answer), QVersionNumber(3, 0, 0)).has_value())
            << answer;
    }

    // Rules without a single line are readable, and what they say is that nothing is offered.
    std::optional<QVersionNumber> empty =
        UpdateInfo::targetVersion(QByteArray(R"({ "format": 1 })"), QVersionNumber(3, 0, 0));

    ASSERT_TRUE(empty.has_value());
    EXPECT_TRUE(empty->isNull());
}

//--------------------------------------------------------------------------------------------------
TEST(UpdateInfoTest, ManifestIsParsed)
{
    std::optional<UpdateInfo> update_info =
        UpdateInfo::fromManifest(manifestJson(), "host", "windows", "x86_64", "msi");

    ASSERT_TRUE(update_info.has_value());
    ASSERT_TRUE(update_info->isValid());
    EXPECT_EQ(update_info->version(), QVersionNumber(3, 0, 6));
    EXPECT_EQ(update_info->description(), QString("A new version of the program."));
    EXPECT_EQ(update_info->url(), QString(kUrl));
    EXPECT_EQ(update_info->sha256(), QString(kSha256));
}

//--------------------------------------------------------------------------------------------------
// A release built for other platforms carries no files for this one, and that is not an error.
TEST(UpdateInfoTest, PlatformWithoutFilesIsNotOffered)
{
    const char* kPlatforms[][3] =
    {
        { "host", "windows", "x86" },
        { "host", "macosx", "arm64" },
        { "client", "windows", "x86_64" }
    };

    for (const char* const* platform : kPlatforms)
    {
        std::optional<UpdateInfo> update_info = UpdateInfo::fromManifest(
            manifestJson(), platform[0], platform[1], platform[2], QString());

        ASSERT_TRUE(update_info.has_value()) << platform[1];
        EXPECT_FALSE(update_info->isValid()) << platform[1];
    }
}

//--------------------------------------------------------------------------------------------------
// Where a platform has several formats, the preferred one is taken; without a preference, or with
// one the manifest does not carry, the first file listed is.
TEST(UpdateInfoTest, PreferredFormatIsTaken)
{
    std::optional<UpdateInfo> rpm =
        UpdateInfo::fromManifest(manifestJson(), "host", "linux", "x86_64", "rpm");
    ASSERT_TRUE(rpm.has_value());
    EXPECT_EQ(rpm->url(), QString(kPath) + "/host.rpm");

    std::optional<UpdateInfo> any =
        UpdateInfo::fromManifest(manifestJson(), "host", "linux", "x86_64", QString());
    ASSERT_TRUE(any.has_value());
    EXPECT_EQ(any->url(), QString(kPath) + "/host.deb");

    std::optional<UpdateInfo> missing =
        UpdateInfo::fromManifest(manifestJson(), "host", "linux", "x86_64", "msi");
    ASSERT_TRUE(missing.has_value());
    EXPECT_EQ(missing->url(), QString(kPath) + "/host.deb");
}

//--------------------------------------------------------------------------------------------------
TEST(UpdateInfoTest, ManifestWithoutDescriptionIsValid)
{
    std::optional<UpdateInfo> update_info =
        UpdateInfo::fromManifest(manifestJson("3.0.6", QString(), kFile, kSha256),
                                 "host", "windows", "x86_64", "msi");

    ASSERT_TRUE(update_info.has_value());
    EXPECT_TRUE(update_info->description().isEmpty());
}

//--------------------------------------------------------------------------------------------------
TEST(UpdateInfoTest, TooLongDescriptionIsRejected)
{
    QString description = QString("a").repeated(4097);

    EXPECT_FALSE(UpdateInfo::fromManifest(manifestJson("3.0.6", description, kFile, kSha256),
                                          "host", "windows", "x86_64", "msi").has_value());
}

//--------------------------------------------------------------------------------------------------
TEST(UpdateInfoTest, BrokenFileNameIsRejected)
{
    EXPECT_FALSE(UpdateInfo::fromManifest(manifestJson("3.0.6", "Release", QString(), kSha256),
                                          "host", "windows", "x86_64", "msi").has_value());

    QString long_name = QString("a").repeated(256);

    EXPECT_FALSE(UpdateInfo::fromManifest(manifestJson("3.0.6", "Release", long_name, kSha256),
                                          "host", "windows", "x86_64", "msi").has_value());
}

//--------------------------------------------------------------------------------------------------
// Without the directory of the release there is nowhere to take the file from.
TEST(UpdateInfoTest, ManifestWithoutPathIsRejected)
{
    QByteArray json = QString(R"({
        "format": 1,
        "version": "3.0.6",
        "packages": {
            "host": { "windows": { "x86_64": [ { "format": "msi", "file": "%1", "sha256": "%2" } ] } }
        }
    })").arg(QString(kFile), QString(kSha256)).toUtf8();

    EXPECT_FALSE(UpdateInfo::fromManifest(json, "host", "windows", "x86_64", "msi").has_value());
}

//--------------------------------------------------------------------------------------------------
// A file nobody can check is not offered: the hash is what proves the download is the release.
TEST(UpdateInfoTest, BrokenSha256IsRejected)
{
    const char* kHashes[] =
    {
        "",
        "9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a0",
        "9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a088",
        "9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00zzz"
    };

    for (const char* sha256 : kHashes)
    {
        EXPECT_FALSE(UpdateInfo::fromManifest(manifestJson("3.0.6", "Release", kUrl, sha256),
                                              "host", "windows", "x86_64", "msi").has_value())
            << sha256;
    }
}

//--------------------------------------------------------------------------------------------------
// The hash is written in either case, and the parsed record keeps one of them.
TEST(UpdateInfoTest, Sha256IsLowerCased)
{
    std::optional<UpdateInfo> update_info =
        UpdateInfo::fromManifest(manifestJson("3.0.6", "Release", kUrl, QString(kSha256).toUpper()),
                                 "host", "windows", "x86_64", "msi");

    ASSERT_TRUE(update_info.has_value());
    EXPECT_EQ(update_info->sha256(), QString(kSha256));
}

//--------------------------------------------------------------------------------------------------
TEST(UpdateInfoTest, BrokenManifestIsRejected)
{
    const char* kAnswers[] =
    {
        "",
        "not a json at all",
        "<html><body>404 Not Found</body></html>",
        R"({ "format": 2, "version": "3.0.6", "packages": {} })"
    };

    for (const char* answer : kAnswers)
    {
        EXPECT_FALSE(UpdateInfo::fromManifest(QByteArray(answer), "host", "windows", "x86_64",
                                              "msi").has_value()) << answer;
    }

    // A manifest without a single package is readable, and what it says is that the release was
    // built for nobody.
    std::optional<UpdateInfo> empty = UpdateInfo::fromManifest(
        QByteArray(R"({ "format": 1, "packages": {} })"), "host", "windows", "x86_64", "msi");

    ASSERT_TRUE(empty.has_value());
    EXPECT_FALSE(empty->isValid());
}
