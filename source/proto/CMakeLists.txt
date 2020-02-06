#
# Aspia Project
# Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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
    common.proto
    desktop.proto
    desktop_extensions.proto
    desktop_internal.proto
    file_transfer.proto
    key_exchange.proto
    host_internal.proto
    system_info.proto)

protobuf_generate_cpp(PROTO_CC_FILES PROTO_H_FILES ${SOURCE_PROTO})
set_source_files_properties(${PROTO_CC_FILES} ${PROTO_H_FILES} PROPERTIES SKIP_AUTOGEN ON)

source_group("" FILES ${SOURCE_PROTO} ${PROTO_CC_FILES} ${PROTO_H_FILES})

add_library(aspia_proto STATIC ${SOURCE_PROTO} ${PROTO_CC_FILES} ${PROTO_H_FILES})
target_link_libraries(aspia_proto ${THIRD_PARTY_LIBS})
