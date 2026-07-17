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

list(APPEND SOURCE_COMMON
    clipboard.cc
    clipboard.h
    clipboard_file_transfer.cc
    clipboard_file_transfer.h
    desktop_session_constants.cc
    desktop_session_constants.h
    file_depacketizer.cc
    file_depacketizer.h
    file_packet.h
    file_packetizer.cc
    file_packetizer.h
    file_platform_util.h
    file_request_handler.cc
    file_request_handler.h
    file_task.cc
    file_task.h
    file_task_factory.cc
    file_task_factory.h
    http_file_downloader.cc
    http_file_downloader.h
    keycode_converter.cc
    keycode_converter.h
    system_info_constants.cc
    system_info_constants.h
    update_checker.cc
    update_checker.h
    update_info.cc
    update_info.h)

if (WIN32)
    list(APPEND SOURCE_COMMON
        clipboard_win.cc
        clipboard_win.h
        file_platform_util_win.cc)
endif()

if (LINUX)
    list(APPEND SOURCE_COMMON
        clipboard_x11.cc
        clipboard_x11.h
        file_platform_util_linux.cc)
endif()

if (ANDROID)
    list(APPEND SOURCE_COMMON
        clipboard_android.cc
        clipboard_android.h
        file_platform_util_android.cc)
endif()

if (APPLE)
    list(APPEND SOURCE_COMMON
        clipboard_mac.mm
        clipboard_mac.h
        file_platform_util_mac.mm)

    include(mac/sources.cmake)
endif()

include(desktop/sources.cmake)

if (WIN32)
    include(win/sources.cmake)
endif()

if (ANDROID)
    include(android/sources.cmake)
endif()

list(APPEND SOURCE_COMMON_RESOURCES resources/common.qrc)

source_group("" FILES ${SOURCE_COMMON})
source_group(desktop FILES ${SOURCE_COMMON_DESKTOP})
source_group(resources FILES ${SOURCE_COMMON_RESOURCES})

if (WIN32)
    source_group(win FILES ${SOURCE_COMMON_WIN})
endif()

if (APPLE)
    source_group(mac FILES ${SOURCE_COMMON_MAC})
endif()

if (ANDROID)
    source_group(android FILES ${SOURCE_COMMON_ANDROID})
endif()

add_library(aspia_common STATIC
    ${SOURCE_COMMON}
    ${SOURCE_COMMON_ANDROID}
    ${SOURCE_COMMON_DESKTOP}
    ${SOURCE_COMMON_WIN}
    ${SOURCE_COMMON_MAC}
    ${SOURCE_COMMON_RESOURCES})
target_link_libraries(aspia_common PRIVATE
    aspia_base
    aspia_proto
    ${QT_COMMON_LIBS}
    ${QT_PLATFORM_LIBS}
    ${THIRD_PARTY_LIBS})
set_property(TARGET aspia_common PROPERTY AUTOMOC ON)
set_property(TARGET aspia_common PROPERTY AUTOUIC ON)
set_property(TARGET aspia_common PROPERTY AUTORCC ON)
