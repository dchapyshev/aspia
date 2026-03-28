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

#include "base/serialization.h"

#include "proto/file_transfer.h"

#include <gtest/gtest.h>

#include <string>

namespace base {

// ============================================================================
// Free function: serialize(MessageLite) / parse(QByteArray, T*)
// ============================================================================

TEST(serialization_test, serialize_and_parse_free_functions)
{
    proto::file_transfer::ListRequest original;
    original.set_path("/home/user/documents");

    QByteArray buffer = serialize(original);
    ASSERT_FALSE(buffer.isEmpty());

    proto::file_transfer::ListRequest parsed;
    ASSERT_TRUE(parse(buffer, &parsed));
    EXPECT_EQ(parsed.path(), "/home/user/documents");
}

TEST(serialization_test, serialize_empty_message)
{
    proto::file_transfer::ListRequest empty;
    QByteArray buffer = serialize(empty);
    EXPECT_TRUE(buffer.isEmpty());
}

TEST(serialization_test, parse_invalid_data)
{
    proto::file_transfer::ListRequest msg;
    QByteArray garbage(100, '\xFF');
    // Should not crash. Lite runtime may or may not reject garbage.
    msg.ParseFromArray(garbage.data(), garbage.size());
}

// ============================================================================
// Free functions: Version serialize/parse
// ============================================================================

TEST(serialization_test, version_roundtrip)
{
    QVersionNumber original(2, 5, 13);
    proto::peer::Version proto_ver = serialize(original);
    QVersionNumber restored = parse(proto_ver);
    EXPECT_EQ(restored, original);
}

TEST(serialization_test, version_zero)
{
    QVersionNumber original(0, 0, 0);
    proto::peer::Version proto_ver = serialize(original);
    QVersionNumber restored = parse(proto_ver);
    EXPECT_EQ(restored, original);
}

TEST(serialization_test, version_large_values)
{
    QVersionNumber original(999, 888, 777);
    proto::peer::Version proto_ver = serialize(original);
    QVersionNumber restored = parse(proto_ver);
    EXPECT_EQ(restored, original);
}

// ============================================================================
// Free functions: Rect serialize/parse
// ============================================================================

TEST(serialization_test, rect_roundtrip)
{
    QRect original(10, 20, 1920, 1080);
    proto::video::Rect proto_rect = serialize(original);
    QRect restored = parse(proto_rect);
    EXPECT_EQ(restored, original);
}

TEST(serialization_test, rect_negative_coords)
{
    QRect original(-100, -200, 640, 480);
    proto::video::Rect proto_rect = serialize(original);
    QRect restored = parse(proto_rect);
    EXPECT_EQ(restored, original);
}

TEST(serialization_test, rect_zero)
{
    QRect original(0, 0, 0, 0);
    proto::video::Rect proto_rect = serialize(original);
    QRect restored = parse(proto_rect);
    EXPECT_EQ(restored, original);
}

// ============================================================================
// Free functions: Point serialize/parse
// ============================================================================

TEST(serialization_test, point_roundtrip)
{
    QPoint original(123, 456);
    proto::screen::Point proto_point = serialize(original);
    QPoint restored = parse(proto_point);
    EXPECT_EQ(restored, original);
}

TEST(serialization_test, point_negative)
{
    QPoint original(-50, -75);
    proto::screen::Point proto_point = serialize(original);
    QPoint restored = parse(proto_point);
    EXPECT_EQ(restored, original);
}

// ============================================================================
// Free functions: Size serialize/parse
// ============================================================================

TEST(serialization_test, size_roundtrip)
{
    QSize original(1920, 1080);
    proto::screen::Size proto_size = serialize(original);
    QSize restored = parse(proto_size);
    EXPECT_EQ(restored, original);
}

TEST(serialization_test, size_zero)
{
    QSize original(0, 0);
    proto::screen::Size proto_size = serialize(original);
    QSize restored = parse(proto_size);
    EXPECT_EQ(restored, original);
}

// ============================================================================
// SerializerImpl
// ============================================================================

TEST(serialization_test, serializer_impl_basic)
{
    SerializerImpl impl;

    proto::file_transfer::ListRequest msg;
    msg.set_path("/test/path");

    const QByteArray& data = impl.serialize(msg);
    ASSERT_FALSE(data.isEmpty());

    proto::file_transfer::ListRequest parsed;
    ASSERT_TRUE(parsed.ParseFromArray(data.data(), data.size()));
    EXPECT_EQ(parsed.path(), "/test/path");
}

TEST(serialization_test, serializer_impl_empty_message)
{
    SerializerImpl impl;

    proto::file_transfer::ListRequest empty;
    const QByteArray& data = impl.serialize(empty);
    EXPECT_TRUE(data.isEmpty());
}

TEST(serialization_test, serializer_impl_circular_buffer)
{
    SerializerImpl impl;

    // Serialize kBufferCount different messages, collecting references.
    // Each call rotates the internal buffer index.
    std::vector<const QByteArray*> refs;
    for (int i = 0; i < SerializerImpl::kBufferCount; ++i)
    {
        proto::file_transfer::ListRequest msg;
        msg.set_path("/path/" + std::to_string(i));
        refs.push_back(&impl.serialize(msg));
    }

    // All kBufferCount references should be distinct (different buffers).
    for (int i = 0; i < SerializerImpl::kBufferCount; ++i)
    {
        for (int j = i + 1; j < SerializerImpl::kBufferCount; ++j)
            EXPECT_NE(refs[i], refs[j]) << "buffers " << i << " and " << j << " should differ";
    }

    // After full rotation, the next serialize should reuse the first buffer.
    proto::file_transfer::ListRequest msg;
    msg.set_path("/path/wrap");
    const QByteArray* wrapped = &impl.serialize(msg);
    EXPECT_EQ(wrapped, refs[0]);
}

TEST(serialization_test, serializer_impl_buffer_reuse_preserves_data)
{
    SerializerImpl impl;

    proto::file_transfer::ListRequest msg1;
    msg1.set_path("/first");
    const QByteArray& data1 = impl.serialize(msg1);

    // Verify data1 is correct before it gets overwritten.
    proto::file_transfer::ListRequest parsed;
    ASSERT_TRUE(parsed.ParseFromArray(data1.data(), data1.size()));
    EXPECT_EQ(parsed.path(), "/first");
}

// ============================================================================
// Serializer<T>
// ============================================================================

TEST(serialization_test, serializer_basic)
{
    Serializer<proto::file_transfer::Reply> serializer;

    auto& msg = serializer.newMessage();
    msg.set_error_code(proto::file_transfer::ERROR_CODE_SUCCESS);
    auto* item = msg.mutable_file_list()->add_item();
    item->set_name("test.txt");
    item->set_size(12345);
    item->set_is_directory(false);

    const QByteArray& data = serializer.serialize();
    ASSERT_FALSE(data.isEmpty());

    proto::file_transfer::Reply parsed;
    ASSERT_TRUE(parsed.ParseFromArray(data.data(), data.size()));
    EXPECT_EQ(parsed.error_code(), proto::file_transfer::ERROR_CODE_SUCCESS);
    ASSERT_EQ(parsed.file_list().item_size(), 1);
    EXPECT_EQ(parsed.file_list().item(0).name(), "test.txt");
    EXPECT_EQ(parsed.file_list().item(0).size(), 12345u);
}

TEST(serialization_test, serializer_new_message_clears)
{
    Serializer<proto::file_transfer::Request> serializer;

    // First message.
    serializer.newMessage().mutable_file_list_request()->set_path("/old/path");
    const QByteArray data1 = serializer.serialize();
    ASSERT_FALSE(data1.isEmpty());

    // Second message — newMessage() should Clear().
    serializer.newMessage().mutable_drive_list_request()->set_dummy(42);
    const QByteArray data2 = serializer.serialize();

    proto::file_transfer::Request parsed;
    ASSERT_TRUE(parsed.ParseFromArray(data2.data(), data2.size()));
    EXPECT_FALSE(parsed.has_file_list_request());
    EXPECT_TRUE(parsed.has_drive_list_request());
    EXPECT_EQ(parsed.drive_list_request().dummy(), 42u);
}

TEST(serialization_test, serializer_message_accessor)
{
    Serializer<proto::file_transfer::Reply> serializer;
    serializer.newMessage().set_error_code(proto::file_transfer::ERROR_CODE_DISK_FULL);

    const auto& msg = serializer.message();
    EXPECT_EQ(msg.error_code(), proto::file_transfer::ERROR_CODE_DISK_FULL);
}

// ============================================================================
// Parser<T>
// ============================================================================

TEST(serialization_test, parser_basic)
{
    proto::file_transfer::Reply original;
    original.set_error_code(proto::file_transfer::ERROR_CODE_SUCCESS);
    auto* item = original.mutable_file_list()->add_item();
    item->set_name("document.pdf");
    item->set_size(999999);
    item->set_modification_time(1700000000);
    item->set_is_directory(false);

    QByteArray buffer = serialize(original);

    Parser<proto::file_transfer::Reply> parser;
    ASSERT_TRUE(parser.parse(buffer));
    EXPECT_EQ(parser->error_code(), proto::file_transfer::ERROR_CODE_SUCCESS);
    ASSERT_EQ(parser->file_list().item_size(), 1);
    EXPECT_EQ(parser->file_list().item(0).name(), "document.pdf");
    EXPECT_EQ(parser->file_list().item(0).size(), 999999u);
}

TEST(serialization_test, parser_reuse)
{
    Parser<proto::file_transfer::Reply> parser;

    // First parse.
    proto::file_transfer::Reply msg1;
    msg1.set_error_code(proto::file_transfer::ERROR_CODE_SUCCESS);
    for (int i = 0; i < 5; ++i)
        msg1.mutable_file_list()->add_item()->set_name("file" + std::to_string(i));

    QByteArray buf1 = serialize(msg1);
    ASSERT_TRUE(parser.parse(buf1));
    EXPECT_EQ(parser->file_list().item_size(), 5);

    // Second parse — must fully replace previous content.
    proto::file_transfer::Reply msg2;
    msg2.set_error_code(proto::file_transfer::ERROR_CODE_DISK_FULL);
    QByteArray buf2 = serialize(msg2);
    ASSERT_TRUE(parser.parse(buf2));
    EXPECT_EQ(parser->error_code(), proto::file_transfer::ERROR_CODE_DISK_FULL);
    EXPECT_EQ(parser->file_list().item_size(), 0);
}

TEST(serialization_test, parser_message_accessor)
{
    proto::file_transfer::Reply original;
    original.set_error_code(proto::file_transfer::ERROR_CODE_ACCESS_DENIED);
    QByteArray buffer = serialize(original);

    Parser<proto::file_transfer::Reply> parser;
    ASSERT_TRUE(parser.parse(buffer));

    const auto& msg = parser.message();
    EXPECT_EQ(msg.error_code(), proto::file_transfer::ERROR_CODE_ACCESS_DENIED);
}

TEST(serialization_test, parser_arrow_operator_mutable)
{
    proto::file_transfer::Reply original;
    original.set_error_code(proto::file_transfer::ERROR_CODE_SUCCESS);
    QByteArray buffer = serialize(original);

    Parser<proto::file_transfer::Reply> parser;
    ASSERT_TRUE(parser.parse(buffer));

    // Mutable arrow operator.
    parser->set_error_code(proto::file_transfer::ERROR_CODE_ACCESS_DENIED);
    EXPECT_EQ(parser->error_code(), proto::file_transfer::ERROR_CODE_ACCESS_DENIED);
}

TEST(serialization_test, parser_does_not_crash_on_garbage)
{
    Parser<proto::file_transfer::Reply> parser;
    QByteArray garbage(100, '\xFF');
    // Should not crash. Result is implementation-defined for lite runtime.
    parser.parse(garbage);
}

// ============================================================================
// Roundtrip: Serializer → Parser
// ============================================================================

TEST(serialization_test, serializer_parser_roundtrip)
{
    Serializer<proto::file_transfer::Request> serializer;
    Parser<proto::file_transfer::Request> parser;

    auto& msg = serializer.newMessage();
    msg.mutable_file_list_request()->set_path("/home/user");
    auto* packet = msg.mutable_packet();
    packet->set_flags(proto::file_transfer::Packet::FIRST_PACKET);
    packet->set_file_size(1048576);
    packet->set_data(std::string(256, '\xAB'));

    ASSERT_TRUE(parser.parse(serializer.serialize()));
    EXPECT_EQ(parser->file_list_request().path(), "/home/user");
    EXPECT_EQ(parser->packet().flags(), proto::file_transfer::Packet::FIRST_PACKET);
    EXPECT_EQ(parser->packet().file_size(), 1048576u);
    EXPECT_EQ(parser->packet().data().size(), 256u);
}

TEST(serialization_test, serializer_parser_repeated_roundtrip)
{
    Serializer<proto::file_transfer::Reply> serializer;
    Parser<proto::file_transfer::Reply> parser;

    for (int round = 0; round < 10; ++round)
    {
        auto& msg = serializer.newMessage();
        msg.set_error_code(proto::file_transfer::ERROR_CODE_SUCCESS);
        for (int i = 0; i < round + 1; ++i)
        {
            auto* item = msg.mutable_file_list()->add_item();
            item->set_name("file_" + std::to_string(round) + "_" + std::to_string(i));
            item->set_size(1024 * (i + 1));
        }

        ASSERT_TRUE(parser.parse(serializer.serialize()));
        EXPECT_EQ(parser->file_list().item_size(), round + 1);
        EXPECT_EQ(parser->file_list().item(0).name(),
                  "file_" + std::to_string(round) + "_0");
    }
}

} // namespace base
