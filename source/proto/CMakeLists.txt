#
# Aspia Project
# Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.
#

list(APPEND SOURCE_PROTO
    address_book.proto
    chat.proto
    desktop_audio.proto
    desktop_channel.proto
    desktop_clipboard.proto
    desktop_control.proto
    desktop_cursor.proto
    desktop_file.proto
    desktop_input.proto
    desktop_internal.proto
    desktop_legacy.proto
    desktop_power.proto
    desktop_screen.proto
    desktop_user.proto
    desktop_video.proto
    file_transfer.proto
    key_exchange.proto
    peer.proto
    relay_peer.proto
    router.proto
    router_admin.proto
    router_client.proto
    router_legacy_host.proto
    router_manager.proto
    router_host.proto
    router_peer.proto
    router_relay.proto
    stun.proto
    system_info.proto
    task_manager.proto
    user.proto)

source_group("" FILES ${SOURCE_PROTO} ${PROTO_CC_FILES} ${PROTO_H_FILES})

add_library(aspia_proto STATIC ${SOURCE_PROTO})
add_dependencies(aspia_proto qtprotoc)

# Generate standard files (.pb.h, .pb.cc).
protobuf_generate(LANGUAGE cpp TARGET aspia_proto ${SOURCE_PROTO})

# Generate files with Qt meta types.
protobuf_generate(LANGUAGE qt
                  TARGET aspia_proto
                  PLUGIN "protoc-gen-qt=$<TARGET_FILE:qtprotoc>"
                  DEPENDENCIES qtprotoc
                  GENERATE_EXTENSIONS .h .cc
                  ${SOURCE_PROTO})

target_link_libraries(aspia_proto ${THIRD_PARTY_LIBS} ${QT_COMMON_LIBS} ${QT_PLATFORM_LIBS})
