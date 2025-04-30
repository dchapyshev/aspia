#
# Aspia Project
# Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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
    common.h
    common.proto
    desktop.h
    desktop.proto
    desktop_extensions.h
    desktop_extensions.proto
    desktop_internal.proto
    file_transfer.h
    file_transfer.proto
    key_exchange.proto
    meta_types.cc
    meta_types.h
    port_forwarding.proto
    host_internal.proto
    relay_peer.proto
    router_admin.h
    router_admin.proto
    router_common.proto
    router_peer.proto
    router_relay.proto
    system_info.h
    system_info.proto
    task_manager.h
    task_manager.proto
    text_chat.h
    text_chat.proto)

source_group("" FILES ${SOURCE_PROTO} ${PROTO_CC_FILES} ${PROTO_H_FILES})

add_library(aspia_proto STATIC ${SOURCE_PROTO})
protobuf_generate(LANGUAGE cpp TARGET aspia_proto ${SOURCE_PROTO})
target_link_libraries(aspia_proto ${THIRD_PARTY_LIBS} ${QT_COMMON_LIBS} ${QT_PLATFORM_LIBS})
set_property(TARGET aspia_proto PROPERTY AUTOMOC ON)
set_property(TARGET aspia_proto PROPERTY AUTOUIC ON)
set_property(TARGET aspia_proto PROPERTY AUTORCC ON)
