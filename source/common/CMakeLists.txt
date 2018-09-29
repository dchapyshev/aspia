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

list(APPEND SOURCE_COMMON
    clipboard.cc
    clipboard.h
    file_depacketizer.cc
    file_depacketizer.h
    file_packetizer.cc
    file_packetizer.h
    file_platform_util.h
    file_platform_util_win.cc
    file_request.cc
    file_request.h
    file_worker.cc
    file_worker.h
    locale_loader.cc
    locale_loader.h
    message_serialization.h
    user_util.cc
    user_util.h)

list(APPEND SOURCE_COMMON_WIN
    win/file_enumerator.cc
    win/file_enumerator.h)

source_group("" FILES ${SOURCE_COMMON})
source_group(win FILES ${SOURCE_COMMON_WIN})

add_library(aspia_common STATIC
    ${SOURCE_COMMON}
    ${SOURCE_COMMON_WIN})
target_link_libraries(aspia_common
    aspia_base
    aspia_protocol
    ${THIRD_PARTY_LIBS})
