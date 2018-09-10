#
# Aspia Project
# Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

list(APPEND SOURCE_PROTOCOL
    ${PROJECT_SOURCE_DIR}/protocol/address_book.proto
    ${PROJECT_SOURCE_DIR}/protocol/desktop_session.proto
    ${PROJECT_SOURCE_DIR}/protocol/file_transfer_session.proto
    ${PROJECT_SOURCE_DIR}/protocol/key_exchange.proto
    ${PROJECT_SOURCE_DIR}/protocol/notifier.proto
    ${PROJECT_SOURCE_DIR}/protocol/session_type.proto
    ${PROJECT_SOURCE_DIR}/protocol/srp_user.proto)

protobuf_generate_cpp(PROTOCOL_CC_FILES PROTOCOL_H_FILES ${SOURCE_PROTOCOL})
set_source_files_properties(${PROTOCOL_CC_FILES} ${PROTOCOL_H_FILES} PROPERTIES SKIP_AUTOGEN ON)

source_group("" FILES ${SOURCE_PROTOCOL} ${PROTOCOL_CC_FILES} ${PROTOCOL_H_FILES})

add_library(aspia_protocol STATIC ${SOURCE_PROTOCOL} ${PROTOCOL_CC_FILES} ${PROTOCOL_H_FILES})
target_link_libraries(aspia_protocol ${THIRD_PARTY_LIBS})
