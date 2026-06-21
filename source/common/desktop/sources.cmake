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

collect_sources(SOURCE_COMMON_DESKTOP
    about_dialog.cc
    about_dialog.h
    about_dialog.ui
    chat_status_message.cc
    chat_status_message.h
    chat_status_message.ui
    chat_incoming_message.cc
    chat_incoming_message.h
    chat_incoming_message.ui
    chat_message.h
    chat_outgoing_message.cc
    chat_outgoing_message.h
    chat_outgoing_message.ui
    chat_widget.cc
    chat_widget.h
    chat_widget.ui
    credentials_dialog.cc
    credentials_dialog.h
    credentials_dialog.ui
    dialog_button_box.cc
    dialog_button_box.h
    download_dialog.cc
    download_dialog.h
    download_dialog.ui
    formatter.cc
    formatter.h
    icon_text_button.cc
    icon_text_button.h
    language_action.h
    msg_box.cc
    msg_box.h
    password_edit.cc
    password_edit.h
    session_type.cc
    session_type.h
    status_dialog.cc
    status_dialog.h
    status_dialog.ui
    two_factor_code_dialog.cc
    two_factor_code_dialog.h
    two_factor_code_dialog.ui
    two_factor_enroll_dialog.cc
    two_factor_enroll_dialog.h
    two_factor_enroll_dialog.ui
    update_dialog.cc
    update_dialog.h
    update_dialog.ui)

if (WIN32)
    collect_sources(SOURCE_COMMON_DESKTOP
        taskbar_button.cc
        taskbar_button.h
        taskbar_progress.cc
        taskbar_progress.h)
endif()
