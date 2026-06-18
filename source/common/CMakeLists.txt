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
    file_task.cc
    file_task.h
    file_task_factory.cc
    file_task_factory.h
    file_worker.cc
    file_worker.h
    http_file_downloader.cc
    http_file_downloader.h
    keycode_converter.cc
    keycode_converter.h
    pch.h
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
        clipboard_android.h)
endif()

if (APPLE)
    list(APPEND SOURCE_COMMON
        clipboard_mac.mm
        clipboard_mac.h
        file_platform_util_mac.mm)

    list(APPEND SOURCE_COMMON_MAC
        mac/file_data_provider.mm
        mac/file_data_provider.h
        mac/file_promise_writer.mm
        mac/file_promise_writer.h)
endif()

list(APPEND SOURCE_COMMON_DESKTOP
    desktop/about_dialog.cc
    desktop/about_dialog.h
    desktop/about_dialog.ui
    desktop/chat_status_message.cc
    desktop/chat_status_message.h
    desktop/chat_status_message.ui
    desktop/chat_incoming_message.cc
    desktop/chat_incoming_message.h
    desktop/chat_incoming_message.ui
    desktop/chat_message.h
    desktop/chat_outgoing_message.cc
    desktop/chat_outgoing_message.h
    desktop/chat_outgoing_message.ui
    desktop/chat_widget.cc
    desktop/chat_widget.h
    desktop/chat_widget.ui
    desktop/credentials_dialog.cc
    desktop/credentials_dialog.h
    desktop/credentials_dialog.ui
    desktop/dialog_button_box.cc
    desktop/dialog_button_box.h
    desktop/download_dialog.cc
    desktop/download_dialog.h
    desktop/download_dialog.ui
    desktop/formatter.cc
    desktop/formatter.h
    desktop/icon_text_button.cc
    desktop/icon_text_button.h
    desktop/language_action.h
    desktop/msg_box.cc
    desktop/msg_box.h
    desktop/password_edit.cc
    desktop/password_edit.h
    desktop/session_type.cc
    desktop/session_type.h
    desktop/status_dialog.cc
    desktop/status_dialog.h
    desktop/status_dialog.ui
    desktop/two_factor_code_dialog.cc
    desktop/two_factor_code_dialog.h
    desktop/two_factor_code_dialog.ui
    desktop/two_factor_enroll_dialog.cc
    desktop/two_factor_enroll_dialog.h
    desktop/two_factor_enroll_dialog.ui
    desktop/update_dialog.cc
    desktop/update_dialog.h
    desktop/update_dialog.ui)

if (WIN32)
    list(APPEND SOURCE_COMMON_DESKTOP
        desktop/taskbar_button.cc
        desktop/taskbar_button.h
        desktop/taskbar_progress.cc
        desktop/taskbar_progress.h)

    list(APPEND SOURCE_COMMON_WIN
        win/file_object.cc
        win/file_object.h
        win/file_stream.cc
        win/file_stream.h
        win/format_enumerator.cc
        win/format_enumerator.h)
endif()

if (ANDROID)
    list(APPEND SOURCE_COMMON_ANDROID
        android/app_bar.cc
        android/app_bar.h
        android/bottom_navigation_bar.cc
        android/bottom_navigation_bar.h
        android/bottom_sheet.cc
        android/bottom_sheet.h
        android/button.cc
        android/button.h
        android/card.cc
        android/card.h
        android/combo_box.cc
        android/combo_box.h
        android/controls.cc
        android/controls.h
        android/dialog.cc
        android/dialog.h
        android/expandable_panel.cc
        android/expandable_panel.h
        android/floating_action_button.cc
        android/floating_action_button.h
        android/icon_button.cc
        android/icon_button.h
        android/label.cc
        android/label.h
        android/line_edit.cc
        android/line_edit.h
        android/menu.cc
        android/menu.h
        android/message_dialog.cc
        android/message_dialog.h
        android/radio_button.cc
        android/radio_button.h
        android/scroll_area.cc
        android/scroll_area.h
        android/scroll_indicator.cc
        android/scroll_indicator.h
        android/switch.cc
        android/switch.h
        android/text_area.cc
        android/text_area.h
        android/tree_widget.cc
        android/tree_widget.h)
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

target_precompile_headers(aspia_common PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:pch.h>")
