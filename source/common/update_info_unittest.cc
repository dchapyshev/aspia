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

#include "common/update_info.h"

#include <QString>

#include <gtest/gtest.h>

namespace {

const char kUrl[] = "https://aspia.org/download/aspia-host-2.7.0-x86_64.msi";

//--------------------------------------------------------------------------------------------------
// The answer of the update server (common/web/update.php) for a package that has an update: an XML
// declaration and one space of indentation per level, as PHP xmlwriter emits it.
QByteArray serverXml(const QString& version, const QString& description, const QString& url)
{
    QString xml = QString("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n") +
        "<update>\n"
        " <version>" + version + "</version>\n"
        " <description>" + description + "</description>\n"
        " <url>" + url + "</url>\n"
        "</update>\n";
    return xml.toUtf8();
}

//--------------------------------------------------------------------------------------------------
QByteArray serverXml()
{
    return serverXml("2.7.0", "A new version of the program.", kUrl);
}

} // namespace

//--------------------------------------------------------------------------------------------------
TEST(UpdateInfoTest, ServerAnswerIsParsed)
{
    UpdateInfo update_info = UpdateInfo::fromXml(serverXml());

    ASSERT_TRUE(update_info.isValid());
    EXPECT_EQ(update_info.version(), QVersionNumber(2, 7, 0));
    EXPECT_EQ(update_info.description(), QString("A new version of the program."));
    EXPECT_EQ(update_info.url(), QString(kUrl));
}

//--------------------------------------------------------------------------------------------------
TEST(UpdateInfoTest, EmptyAnswerIsNotAnUpdate)
{
    UpdateInfo update_info = UpdateInfo::fromXml(QByteArray());

    EXPECT_FALSE(update_info.isValid());
    EXPECT_TRUE(update_info.version().isNull());
    EXPECT_TRUE(update_info.description().isEmpty());
    EXPECT_TRUE(update_info.url().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// Every failure of the update server is a plain text message from die(), not XML - including the
// normal "there is nothing to update" answer.
TEST(UpdateInfoTest, ServerErrorMessagesAreNotUpdates)
{
    const char* kMessages[] =
    {
        "No updates available",
        "No releases available",
        "Download url not found.",
        "Empty URL for download",
        "Empty target version",
        "Invalid request received.",
        "Could not connect to database: Access denied for user",
        "Failed to execute database query: Table 'updates' doesn't exist"
    };

    for (const char* message : kMessages)
        EXPECT_FALSE(UpdateInfo::fromXml(QByteArray(message)).isValid()) << message;
}

//--------------------------------------------------------------------------------------------------
// The welcome page of the update server (common/web/index.php) - what a request that missed
// update.php gets back. It has a blank line in front of the markup, so it is not even taken for XML.
TEST(UpdateInfoTest, WelcomePageIsNotAnUpdate)
{
    const QByteArray page =
        "\n\n<html>\n\t<head>\n\t\t<title>Aspia Update Server</title>\n\t</head>\n</html>";

    EXPECT_FALSE(UpdateInfo::fromXml(page).isValid());
}

//--------------------------------------------------------------------------------------------------
// Anything in front of the declaration - a byte order mark, a stray newline printed by a PHP file
// past its closing tag - makes the whole answer unusable.
TEST(UpdateInfoTest, LeadingCharactersAreNotAccepted)
{
    EXPECT_FALSE(UpdateInfo::fromXml("\n" + serverXml()).isValid());
    EXPECT_FALSE(UpdateInfo::fromXml("\xEF\xBB\xBF" + serverXml()).isValid());
}

//--------------------------------------------------------------------------------------------------
// The description is written with xmlwriter_text(), which escapes the markup characters.
TEST(UpdateInfoTest, EscapedDescriptionIsDecoded)
{
    UpdateInfo update_info = UpdateInfo::fromXml(
        serverXml("2.7.0", "Fixes &lt;b&gt;R&amp;D&lt;/b&gt; issues", kUrl));

    ASSERT_TRUE(update_info.isValid());
    EXPECT_EQ(update_info.description(), QString("Fixes <b>R&D</b> issues"));
}

//--------------------------------------------------------------------------------------------------
// The description column is a text field, so it holds whatever the release notes were written as.
TEST(UpdateInfoTest, MultilineDescriptionIsKept)
{
    UpdateInfo update_info = UpdateInfo::fromXml(
        serverXml("2.7.0", "First line.\nSecond line.\nThird line.", kUrl));

    ASSERT_TRUE(update_info.isValid());
    EXPECT_EQ(update_info.description(), QString("First line.\nSecond line.\nThird line."));
}

//--------------------------------------------------------------------------------------------------
TEST(UpdateInfoTest, NonAsciiDescriptionIsDecoded)
{
    const QString description = QString::fromUtf8("Nouvelle version. Neue Version.");

    UpdateInfo update_info = UpdateInfo::fromXml(serverXml("2.7.0", description, kUrl));

    ASSERT_TRUE(update_info.isValid());
    EXPECT_EQ(update_info.description(), description);
}

//--------------------------------------------------------------------------------------------------
// A release with no notes at all: the column is empty, so the element carries no text.
TEST(UpdateInfoTest, EmptyDescriptionIsAccepted)
{
    UpdateInfo update_info = UpdateInfo::fromXml(serverXml("2.7.0", QString(), kUrl));

    ASSERT_TRUE(update_info.isValid());
    EXPECT_TRUE(update_info.description().isEmpty());
    EXPECT_EQ(update_info.version(), QVersionNumber(2, 7, 0));
    EXPECT_EQ(update_info.url(), QString(kUrl));
}

//--------------------------------------------------------------------------------------------------
TEST(UpdateInfoTest, SelfClosingElementIsAccepted)
{
    const QByteArray xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<update>\n"
        " <version>2.7.0</version>\n"
        " <description/>\n"
        " <url>" + QByteArray(kUrl) + "</url>\n"
        "</update>\n";

    UpdateInfo update_info = UpdateInfo::fromXml(xml);

    ASSERT_TRUE(update_info.isValid());
    EXPECT_TRUE(update_info.description().isEmpty());
    EXPECT_EQ(update_info.url(), QString(kUrl));
}

//--------------------------------------------------------------------------------------------------
// A field the server learns to send later must not disturb the fields that are known.
TEST(UpdateInfoTest, UnknownElementsAreIgnored)
{
    const QByteArray xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<update>\n"
        " <version>2.7.0</version>\n"
        " <signature>3045022100</signature>\n"
        " <description>A new version of the program.</description>\n"
        " <url>" + QByteArray(kUrl) + "</url>\n"
        "</update>\n";

    UpdateInfo update_info = UpdateInfo::fromXml(xml);

    ASSERT_TRUE(update_info.isValid());
    EXPECT_EQ(update_info.version(), QVersionNumber(2, 7, 0));
    EXPECT_EQ(update_info.description(), QString("A new version of the program."));
    EXPECT_EQ(update_info.url(), QString(kUrl));
}

//--------------------------------------------------------------------------------------------------
// The version column of the server holds any string; the request is cut to three groups, the
// release version is not.
TEST(UpdateInfoTest, VersionWithFourGroupsIsParsed)
{
    UpdateInfo update_info = UpdateInfo::fromXml(serverXml("2.7.0.1234", "Release", kUrl));

    ASSERT_TRUE(update_info.isValid());
    EXPECT_EQ(update_info.version(), QVersionNumber({ 2, 7, 0, 1234 }));
}

//--------------------------------------------------------------------------------------------------
// The answer is taken as an update even when the version is missing or unreadable: only the lengths
// of the description and of the URL are checked. The caller compares the version with its own, and
// a null version never looks newer.
TEST(UpdateInfoTest, VersionIsNotValidated)
{
    const QByteArray xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<update>\n"
        " <description>A new version of the program.</description>\n"
        " <url>" + QByteArray(kUrl) + "</url>\n"
        "</update>\n";

    UpdateInfo update_info = UpdateInfo::fromXml(xml);

    EXPECT_TRUE(update_info.isValid());
    EXPECT_TRUE(update_info.version().isNull());
}

//--------------------------------------------------------------------------------------------------
// An answer without a download link is useless, and the empty URL is shorter than the minimum.
TEST(UpdateInfoTest, MissingUrlIsNotAnUpdate)
{
    const QByteArray xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<update>\n"
        " <version>2.7.0</version>\n"
        " <description>A new version of the program.</description>\n"
        "</update>\n";

    EXPECT_FALSE(UpdateInfo::fromXml(xml).isValid());
}

//--------------------------------------------------------------------------------------------------
TEST(UpdateInfoTest, TooShortUrlIsNotAnUpdate)
{
    // Nine characters, one below the minimum.
    EXPECT_FALSE(UpdateInfo::fromXml(serverXml("2.7.0", "Release", "http://a.")).isValid());
}

//--------------------------------------------------------------------------------------------------
TEST(UpdateInfoTest, UrlOfMinimumLengthIsAccepted)
{
    const QString url("http://a.b");
    ASSERT_EQ(url.size(), 10);

    UpdateInfo update_info = UpdateInfo::fromXml(serverXml("2.7.0", "Release", url));

    ASSERT_TRUE(update_info.isValid());
    EXPECT_EQ(update_info.url(), url);
}

//--------------------------------------------------------------------------------------------------
// The url column of the server is a varchar(256), so a link of exactly that length can be stored
// and must still be taken.
TEST(UpdateInfoTest, UrlOfMaximumLengthIsAccepted)
{
    const QString url = QString("https://aspia.org/") + QString(238, 'a');
    ASSERT_EQ(url.size(), 256);

    UpdateInfo update_info = UpdateInfo::fromXml(serverXml("2.7.0", "Release", url));

    ASSERT_TRUE(update_info.isValid());
    EXPECT_EQ(update_info.url(), url);
}

//--------------------------------------------------------------------------------------------------
TEST(UpdateInfoTest, TooLongUrlIsNotAnUpdate)
{
    const QString url = QString("https://aspia.org/") + QString(239, 'a');
    ASSERT_EQ(url.size(), 257);

    EXPECT_FALSE(UpdateInfo::fromXml(serverXml("2.7.0", "Release", url)).isValid());
}

//--------------------------------------------------------------------------------------------------
TEST(UpdateInfoTest, DescriptionOfMaximumLengthIsAccepted)
{
    const QString description(4096, 'a');

    UpdateInfo update_info = UpdateInfo::fromXml(serverXml("2.7.0", description, kUrl));

    ASSERT_TRUE(update_info.isValid());
    EXPECT_EQ(update_info.description(), description);
}

//--------------------------------------------------------------------------------------------------
// The description column of the server is a text field, which holds far more than the client takes.
// Release notes past the limit make the whole update disappear for the user.
TEST(UpdateInfoTest, TooLongDescriptionIsNotAnUpdate)
{
    const QString description(4097, 'a');

    EXPECT_FALSE(UpdateInfo::fromXml(serverXml("2.7.0", description, kUrl)).isValid());
}

//--------------------------------------------------------------------------------------------------
// The answer of the server can arrive cut short - a dropped connection, a proxy that gave up. The
// document then has no closing element for the field being read.
TEST(UpdateInfoTest, TruncatedAnswerIsNotAnUpdate)
{
    const QByteArray kAnswers[] =
    {
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<update>\n <version>2.7.0</version>\n "
            "<description>A new version",
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<update>\n <version>",
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<update>\n <ver",
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<update>\n"
    };

    for (const QByteArray& answer : kAnswers)
        EXPECT_FALSE(UpdateInfo::fromXml(answer).isValid()) << answer.constData();
}
