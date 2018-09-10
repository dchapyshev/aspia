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
    ${PROJECT_SOURCE_DIR}/console/about_dialog.cc
    ${PROJECT_SOURCE_DIR}/console/about_dialog.h
    ${PROJECT_SOURCE_DIR}/console/about_dialog.ui
    ${PROJECT_SOURCE_DIR}/console/address_book_dialog.cc
    ${PROJECT_SOURCE_DIR}/console/address_book_dialog.h
    ${PROJECT_SOURCE_DIR}/console/address_book_dialog.ui
    ${PROJECT_SOURCE_DIR}/console/address_book_tab.cc
    ${PROJECT_SOURCE_DIR}/console/address_book_tab.h
    ${PROJECT_SOURCE_DIR}/console/address_book_tab.ui
    ${PROJECT_SOURCE_DIR}/console/computer_dialog.cc
    ${PROJECT_SOURCE_DIR}/console/computer_dialog.h
    ${PROJECT_SOURCE_DIR}/console/computer_dialog.ui
    ${PROJECT_SOURCE_DIR}/console/computer_drag.h
    ${PROJECT_SOURCE_DIR}/console/computer_group_dialog.cc
    ${PROJECT_SOURCE_DIR}/console/computer_group_dialog.h
    ${PROJECT_SOURCE_DIR}/console/computer_group_dialog.ui
    ${PROJECT_SOURCE_DIR}/console/computer_group_drag.h
    ${PROJECT_SOURCE_DIR}/console/computer_group_item.cc
    ${PROJECT_SOURCE_DIR}/console/computer_group_item.h
    ${PROJECT_SOURCE_DIR}/console/computer_group_mime_data.h
    ${PROJECT_SOURCE_DIR}/console/computer_group_tree.cc
    ${PROJECT_SOURCE_DIR}/console/computer_group_tree.h
    ${PROJECT_SOURCE_DIR}/console/computer_item.cc
    ${PROJECT_SOURCE_DIR}/console/computer_item.h
    ${PROJECT_SOURCE_DIR}/console/computer_mime_data.h
    ${PROJECT_SOURCE_DIR}/console/computer_tree.cc
    ${PROJECT_SOURCE_DIR}/console/computer_tree.h
    ${PROJECT_SOURCE_DIR}/console/console.rc
    ${PROJECT_SOURCE_DIR}/console/console_main.cc
    ${PROJECT_SOURCE_DIR}/console/console_main.h
    ${PROJECT_SOURCE_DIR}/console/console_settings.cc
    ${PROJECT_SOURCE_DIR}/console/console_settings.h
    ${PROJECT_SOURCE_DIR}/console/console_statusbar.cc
    ${PROJECT_SOURCE_DIR}/console/console_statusbar.h
    ${PROJECT_SOURCE_DIR}/console/console_tab.cc
    ${PROJECT_SOURCE_DIR}/console/console_tab.h
    ${PROJECT_SOURCE_DIR}/console/console_window.cc
    ${PROJECT_SOURCE_DIR}/console/console_window.h
    ${PROJECT_SOURCE_DIR}/console/console_window.ui
    ${PROJECT_SOURCE_DIR}/console/open_address_book_dialog.cc
    ${PROJECT_SOURCE_DIR}/console/open_address_book_dialog.h
    ${PROJECT_SOURCE_DIR}/console/open_address_book_dialog.ui)

list(APPEND SOURCE_CONSOLE_RESOURCES
    ${PROJECT_SOURCE_DIR}/console/resources/console.qrc)

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
