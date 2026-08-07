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

#include "common/file_depacketizer.h"

#include <QFile>
#include <QTemporaryDir>

#include <gtest/gtest.h>

#include "common/file_packet.h"
#include "common/file_packetizer.h"
#include "proto/file_transfer.h"

namespace {

using Packet = proto::file_transfer::Packet;
using PacketRequest = proto::file_transfer::PacketRequest;

//--------------------------------------------------------------------------------------------------
// Content the offsets can be checked against: every byte is a function of its position, so a packet
// written at the wrong place is visible.
QByteArray content(qsizetype size)
{
    QByteArray result;
    result.resize(size);

    for (qsizetype i = 0; i < size; ++i)
        result[i] = static_cast<char>((i * 31 + 7) & 0xFF);

    return result;
}

//--------------------------------------------------------------------------------------------------
Packet dataPacket(quint32 flags, quint64 file_size, const QByteArray& data)
{
    Packet packet;
    packet.set_flags(flags);
    packet.set_file_size(file_size);
    packet.set_data(data.constData(), data.size());
    return packet;
}

class FileDepacketizerTest : public testing::Test
{
protected:
    void SetUp() final { ASSERT_TRUE(dir_.isValid()); }

    QString targetPath() const { return dir_.filePath("target.bin"); }

    // Creates a file with |data| and returns its path.
    QString makeFile(const QString& name, const QByteArray& data)
    {
        const QString path = dir_.filePath(name);

        QFile file(path);
        EXPECT_TRUE(file.open(QFile::WriteOnly));
        EXPECT_EQ(file.write(data), data.size());

        return path;
    }

    QByteArray readFile(const QString& path) const
    {
        QFile file(path);
        EXPECT_TRUE(file.open(QFile::ReadOnly));
        return file.readAll();
    }

    QTemporaryDir dir_;
};

} // namespace

//--------------------------------------------------------------------------------------------------
TEST_F(FileDepacketizerTest, FileThatCannotBeOpenedHasNoDepacketizer)
{
    EXPECT_EQ(FileDepacketizer::create(dir_.filePath("no-such-directory/target.bin"), true),
              nullptr);
}

//--------------------------------------------------------------------------------------------------
// The whole path a downloaded file takes: the sender cuts it into packets, the receiver puts it
// back together.
TEST_F(FileDepacketizerTest, TransferRestoresTheFile)
{
    const QByteArray source_content = content(3 * static_cast<qsizetype>(kMaxFilePacketSize) + 517);
    const QString source_path = makeFile("source.bin", source_content);

    std::unique_ptr<FilePacketizer> packetizer = FilePacketizer::create(source_path);
    ASSERT_NE(packetizer, nullptr);

    std::unique_ptr<FileDepacketizer> depacketizer =
        FileDepacketizer::create(targetPath(), true);
    ASSERT_NE(depacketizer, nullptr);

    PacketRequest request;
    request.set_flags(PacketRequest::NO_FLAGS);

    int packet_count = 0;

    for (;;)
    {
        std::unique_ptr<Packet> packet = packetizer->readNextPacket(request);
        ASSERT_NE(packet, nullptr);
        ASSERT_TRUE(depacketizer->writeNextPacket(*packet));

        ++packet_count;

        if (packet->flags() & Packet::LAST_PACKET)
            break;
    }

    EXPECT_EQ(packet_count, 4);
    EXPECT_EQ(readFile(targetPath()), source_content);
}

//--------------------------------------------------------------------------------------------------
TEST_F(FileDepacketizerTest, SinglePacketFileIsWritten)
{
    const QByteArray data = content(1024);

    std::unique_ptr<FileDepacketizer> depacketizer =
        FileDepacketizer::create(targetPath(), true);
    ASSERT_NE(depacketizer, nullptr);

    EXPECT_TRUE(depacketizer->writeNextPacket(
        dataPacket(Packet::FIRST_PACKET | Packet::LAST_PACKET, data.size(), data)));

    EXPECT_EQ(readFile(targetPath()), data);
}

//--------------------------------------------------------------------------------------------------
// A file with no content: the packet carries the two flags and nothing else, and the file stays.
TEST_F(FileDepacketizerTest, EmptyFileIsKept)
{
    std::unique_ptr<FileDepacketizer> depacketizer =
        FileDepacketizer::create(targetPath(), true);
    ASSERT_NE(depacketizer, nullptr);

    EXPECT_TRUE(depacketizer->writeNextPacket(
        dataPacket(Packet::FIRST_PACKET | Packet::LAST_PACKET, 0, QByteArray())));

    depacketizer.reset();

    ASSERT_TRUE(QFile::exists(targetPath()));
    EXPECT_EQ(QFile(targetPath()).size(), 0);
}

//--------------------------------------------------------------------------------------------------
// The sender cancels: an empty packet with the last flag alone. The file was never completed, so
// the half-written file must not be left behind.
TEST_F(FileDepacketizerTest, CancelledTransferLeavesNoFile)
{
    const QByteArray data = content(kMaxFilePacketSize);

    std::unique_ptr<FileDepacketizer> depacketizer =
        FileDepacketizer::create(targetPath(), true);
    ASSERT_NE(depacketizer, nullptr);

    ASSERT_TRUE(depacketizer->writeNextPacket(
        dataPacket(Packet::FIRST_PACKET, 2 * kMaxFilePacketSize, data)));

    EXPECT_TRUE(depacketizer->writeNextPacket(
        dataPacket(Packet::LAST_PACKET, 0, QByteArray())));

    depacketizer.reset();

    EXPECT_FALSE(QFile::exists(targetPath()));
}

//--------------------------------------------------------------------------------------------------
// The connection drops in the middle of a transfer: the object goes away with the file still open,
// and the incomplete file goes with it.
TEST_F(FileDepacketizerTest, InterruptedTransferLeavesNoFile)
{
    const QByteArray data = content(kMaxFilePacketSize);

    std::unique_ptr<FileDepacketizer> depacketizer =
        FileDepacketizer::create(targetPath(), true);
    ASSERT_NE(depacketizer, nullptr);

    ASSERT_TRUE(depacketizer->writeNextPacket(
        dataPacket(Packet::FIRST_PACKET, 2 * kMaxFilePacketSize, data)));

    depacketizer.reset();

    EXPECT_FALSE(QFile::exists(targetPath()));
}

//--------------------------------------------------------------------------------------------------
// Data before the packet that declares the file size has nowhere to go: the remaining size is still
// zero, and subtracting from it would wrap the unsigned counter around.
TEST_F(FileDepacketizerTest, DataBeforeTheFirstPacketIsRejected)
{
    std::unique_ptr<FileDepacketizer> depacketizer =
        FileDepacketizer::create(targetPath(), true);
    ASSERT_NE(depacketizer, nullptr);

    EXPECT_FALSE(depacketizer->writeNextPacket(
        dataPacket(Packet::NO_FLAGS, 0, content(64))));
}

//--------------------------------------------------------------------------------------------------
TEST_F(FileDepacketizerTest, PacketLargerThanTheDeclaredSizeIsRejected)
{
    std::unique_ptr<FileDepacketizer> depacketizer =
        FileDepacketizer::create(targetPath(), true);
    ASSERT_NE(depacketizer, nullptr);

    EXPECT_FALSE(depacketizer->writeNextPacket(
        dataPacket(Packet::FIRST_PACKET, 64, content(65))));
}

//--------------------------------------------------------------------------------------------------
// A peer that keeps sending after the declared size is reached is cut off.
TEST_F(FileDepacketizerTest, DataPastTheDeclaredSizeIsRejected)
{
    const QByteArray data = content(64);

    std::unique_ptr<FileDepacketizer> depacketizer =
        FileDepacketizer::create(targetPath(), true);
    ASSERT_NE(depacketizer, nullptr);

    ASSERT_TRUE(depacketizer->writeNextPacket(
        dataPacket(Packet::FIRST_PACKET, data.size(), data)));

    EXPECT_FALSE(depacketizer->writeNextPacket(dataPacket(Packet::NO_FLAGS, 0, data)));
}

//--------------------------------------------------------------------------------------------------
TEST_F(FileDepacketizerTest, EmptyPacketWithoutTheLastFlagIsRejected)
{
    std::unique_ptr<FileDepacketizer> depacketizer =
        FileDepacketizer::create(targetPath(), true);
    ASSERT_NE(depacketizer, nullptr);

    EXPECT_FALSE(depacketizer->writeNextPacket(
        dataPacket(Packet::FIRST_PACKET, 64, QByteArray())));
}

//--------------------------------------------------------------------------------------------------
// Nothing of the previous file survives a transfer that is shorter than it, whatever |overwrite|
// says: refusing to replace an existing file is the business of the request handler, which checks
// it before the depacketizer is ever created.
TEST_F(FileDepacketizerTest, PreviousContentOfTheFileIsGone)
{
    const QString path = makeFile("target.bin", QByteArray("0123456789"));
    const QByteArray data("ab");

    std::unique_ptr<FileDepacketizer> depacketizer = FileDepacketizer::create(path, false);
    ASSERT_NE(depacketizer, nullptr);

    EXPECT_TRUE(depacketizer->writeNextPacket(
        dataPacket(Packet::FIRST_PACKET | Packet::LAST_PACKET, data.size(), data)));

    EXPECT_EQ(readFile(path), data);
}
