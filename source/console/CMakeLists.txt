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

add_subdirectory(translations)

list(APPEND SOURCE_CONSOLE
    about_dialog.cc
    about_dialog.h
    about_dialog.ui
    address_book_dialog.cc
    address_book_dialog.h
    address_book_dialog.ui
    address_book_tab.cc
    address_book_tab.h
    address_book_tab.ui
    computer_dialog.cc
    computer_dialog.h
    computer_dialog.ui
    computer_drag.h
    computer_group_dialog.cc
    computer_group_dialog.h
    computer_group_dialog.ui
    computer_group_drag.h
    computer_group_item.cc
    computer_group_item.h
    computer_group_mime_data.h
    computer_group_tree.cc
    computer_group_tree.h
    computer_item.cc
    computer_item.h
    computer_mime_data.h
    computer_tree.cc
    computer_tree.h
    console.rc
    console_main.cc
    console_settings.cc
    console_settings.h
    console_statusbar.cc
    console_statusbar.h
    console_tab.cc
    console_tab.h
    console_window.cc
    console_window.h
    console_window.ui
    open_address_book_dialog.cc
    open_address_book_dialog.h
    open_address_book_dialog.ui)

list(APPEND SOURCE_CONSOLE_RESOURCES
    resources/console.qrc)

source_group("" FILES ${SOURCE_CONSOLE})
source_group(resources FILES ${SOURCE_CONSOLE_RESOURCES})

add_executable(aspia_console ${SOURCE_CONSOLE} ${SOURCE_CONSOLE_RESOURCES})
set_target_properties(aspia_console PROPERTIES WIN32_EXECUTABLE TRUE)

target_link_libraries(aspia_console
    aspia_base
    aspia_client
    aspia_codec
    aspia_crypto
    aspia_desktop_capture
    aspia_network
    aspia_protocol
    aspia_share
    ${THIRD_PARTY_LIBS})
