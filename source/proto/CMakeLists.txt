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
    desktop.proto
    desktop_internal.proto
    file_transfer.proto
    key_exchange.proto
    peer.proto
    port_forwarding.proto
    host_internal.proto
    relay_peer.proto
    router.proto
    router_admin.proto
    router_peer.proto
    router_relay.proto
    system_info.proto
    task_manager.proto
    text_chat.proto)

source_group("" FILES ${SOURCE_PROTO} ${PROTO_CC_FILES} ${PROTO_H_FILES})

add_library(aspia_proto STATIC ${SOURCE_PROTO})
add_dependencies(aspia_proto qtprotoc)

# Generate standard files (.pb.h, .pb.cc).
protobuf_generate(LANGUAGE cpp TARGET aspia_proto ${SOURCE_PROTO})

if (WIN32)
    set(QTPROTOC_FILENAME "qtprotoc.exe")
else()
    set(QTPROTOC_FILENAME "qtprotoc")
endif()

set(QTPROTOC_FILEPATH "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${QTPROTOC_FILENAME}")

if (NOT EXISTS "${QTPROTOC_FILEPATH}")
    message(STATUS "Default QTPROTOC path NOT exists")
    set(QTPROTOC_FILEPATH "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/${QTPROTOC_FILENAME}")
else()
    message(STATUS "Default QTPROTOC path exists")
endif()

message(STATUS "QTPROTOC_FILEPATH: ${QTPROTOC_FILEPATH}")

# Generate files with Qt meta types.
protobuf_generate(LANGUAGE qt
                  TARGET aspia_proto
                  PLUGIN "protoc-gen-qt=${QTPROTOC_FILEPATH}"
                  GENERATE_EXTENSIONS .h .cc
                  ${SOURCE_PROTO})

target_link_libraries(aspia_proto ${THIRD_PARTY_LIBS} ${QT_COMMON_LIBS} ${QT_PLATFORM_LIBS})
