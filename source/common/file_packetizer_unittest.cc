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

#include "common/file_packetizer.h"

#include <QFile>
#include <QTemporaryDir>

#include <gtest/gtest.h>

#include "common/file_packet.h"
#include "proto/file_transfer.h"

namespace {

using Packet = proto::file_transfer::Packet;
using PacketRequest = proto::file_transfer::PacketRequest;

//--------------------------------------------------------------------------------------------------
QByteArray dataOf(const Packet& packet)
{
    return QByteArray::fromStdString(packet.data());
}

//--------------------------------------------------------------------------------------------------
PacketRequest nextRequest()
{
    PacketRequest request;
    request.set_flags(PacketRequest::NO_FLAGS);
    return request;
}

//--------------------------------------------------------------------------------------------------
PacketRequest cancelRequest()
{
    PacketRequest request;
    request.set_flags(PacketRequest::CANCEL);
    return request;
}

class FilePacketizerTest : public testing::Test
{
protected:
    void SetUp() final { ASSERT_TRUE(dir_.isValid()); }

    // Creates a file with |content| and returns its path.
    QString makeFile(const QByteArray& content)
    {
        const QString path = dir_.filePath("source.bin");

        QFile file(path);
        EXPECT_TRUE(file.open(QFile::WriteOnly));
        EXPECT_EQ(file.write(content), content.size());

        return path;
    }

    QTemporaryDir dir_;
};

} // namespace

//--------------------------------------------------------------------------------------------------
TEST_F(FilePacketizerTest, MissingFileHasNoPacketizer)
{
    EXPECT_EQ(FilePacketizer::create(dir_.filePath("no-such-file.bin")), nullptr);
}

//--------------------------------------------------------------------------------------------------
TEST_F(FilePacketizerTest, FileSmallerThanOnePacketIsSentAtOnce)
{
    const QByteArray content("The quick brown fox jumps over the lazy dog");

    std::unique_ptr<FilePacketizer> packetizer = FilePacketizer::create(makeFile(content));
    ASSERT_NE(packetizer, nullptr);

    std::unique_ptr<Packet> packet = packetizer->readNextPacket(nextRequest());
    ASSERT_NE(packet, nullptr);

    EXPECT_TRUE(packet->flags() & Packet::FIRST_PACKET);
    EXPECT_TRUE(packet->flags() & Packet::LAST_PACKET);
    EXPECT_EQ(packet->file_size(), static_cast<quint64>(content.size()));
    EXPECT_EQ(dataOf(*packet), content);
}

//--------------------------------------------------------------------------------------------------
// A file with no content still produces a packet, so that the other side learns the file is there.
TEST_F(FilePacketizerTest, EmptyFileIsSentAsOneEmptyPacket)
{
    std::unique_ptr<FilePacketizer> packetizer = FilePacketizer::create(makeFile(QByteArray()));
    ASSERT_NE(packetizer, nullptr);

    std::unique_ptr<Packet> packet = packetizer->readNextPacket(nextRequest());
    ASSERT_NE(packet, nullptr);

    EXPECT_TRUE(packet->flags() & Packet::FIRST_PACKET);
    EXPECT_TRUE(packet->flags() & Packet::LAST_PACKET);
    EXPECT_EQ(packet->file_size(), 0U);
    EXPECT_TRUE(packet->data().empty());
}

//--------------------------------------------------------------------------------------------------
TEST_F(FilePacketizerTest, FileOfExactlyOnePacketIsSentAtOnce)
{
    const QByteArray content(static_cast<qsizetype>(kMaxFilePacketSize), 'a');

    std::unique_ptr<FilePacketizer> packetizer = FilePacketizer::create(makeFile(content));
    ASSERT_NE(packetizer, nullptr);

    std::unique_ptr<Packet> packet = packetizer->readNextPacket(nextRequest());
    ASSERT_NE(packet, nullptr);

    EXPECT_TRUE(packet->flags() & Packet::FIRST_PACKET);
    EXPECT_TRUE(packet->flags() & Packet::LAST_PACKET);
    EXPECT_EQ(packet->data().size(), kMaxFilePacketSize);
}

//--------------------------------------------------------------------------------------------------
// The file size travels in the first packet only; the packets that follow carry data alone.
TEST_F(FilePacketizerTest, LargerFileIsSplitIntoPackets)
{
    const qsizetype kTailSize = 100;
    QByteArray content(static_cast<qsizetype>(kMaxFilePacketSize), 'a');
    content.append(QByteArray(kTailSize, 'b'));

    std::unique_ptr<FilePacketizer> packetizer = FilePacketizer::create(makeFile(content));
    ASSERT_NE(packetizer, nullptr);

    std::unique_ptr<Packet> first = packetizer->readNextPacket(nextRequest());
    ASSERT_NE(first, nullptr);

    EXPECT_TRUE(first->flags() & Packet::FIRST_PACKET);
    EXPECT_FALSE(first->flags() & Packet::LAST_PACKET);
    EXPECT_EQ(first->file_size(), static_cast<quint64>(content.size()));
    EXPECT_EQ(first->data().size(), kMaxFilePacketSize);

    std::unique_ptr<Packet> second = packetizer->readNextPacket(nextRequest());
    ASSERT_NE(second, nullptr);

    EXPECT_FALSE(second->flags() & Packet::FIRST_PACKET);
    EXPECT_TRUE(second->flags() & Packet::LAST_PACKET);
    EXPECT_EQ(second->file_size(), 0U);
    EXPECT_EQ(second->data().size(), static_cast<size_t>(kTailSize));

    EXPECT_EQ(dataOf(*first) + dataOf(*second), content);
}

//--------------------------------------------------------------------------------------------------
// The receiver gives up: the answer to a cancelled request is an empty packet that ends the
// transfer, and nothing is read from the file.
TEST_F(FilePacketizerTest, CancelledRequestEndsTheTransfer)
{
    const QByteArray content(static_cast<qsizetype>(kMaxFilePacketSize) + 100, 'a');

    std::unique_ptr<FilePacketizer> packetizer = FilePacketizer::create(makeFile(content));
    ASSERT_NE(packetizer, nullptr);

    std::unique_ptr<Packet> packet = packetizer->readNextPacket(cancelRequest());
    ASSERT_NE(packet, nullptr);

    EXPECT_EQ(packet->flags(), static_cast<quint32>(Packet::LAST_PACKET));
    EXPECT_EQ(packet->file_size(), 0U);
    EXPECT_TRUE(packet->data().empty());
}

//--------------------------------------------------------------------------------------------------
TEST_F(FilePacketizerTest, TransferCanBeCancelledInTheMiddle)
{
    const QByteArray content(static_cast<qsizetype>(kMaxFilePacketSize) + 100, 'a');

    std::unique_ptr<FilePacketizer> packetizer = FilePacketizer::create(makeFile(content));
    ASSERT_NE(packetizer, nullptr);

    std::unique_ptr<Packet> first = packetizer->readNextPacket(nextRequest());
    ASSERT_NE(first, nullptr);
    ASSERT_FALSE(first->flags() & Packet::LAST_PACKET);

    std::unique_ptr<Packet> cancelled = packetizer->readNextPacket(cancelRequest());
    ASSERT_NE(cancelled, nullptr);

    EXPECT_EQ(cancelled->flags(), static_cast<quint32>(Packet::LAST_PACKET));
    EXPECT_TRUE(cancelled->data().empty());
}
