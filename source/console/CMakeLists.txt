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

include(translations)

list(APPEND SOURCE_CONSOLE
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
    computer_factory.cc
    computer_factory.h
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
    mru.cc
    mru.h
    open_address_book_dialog.cc
    open_address_book_dialog.h
    open_address_book_dialog.ui
    update_settings_dialog.cc
    update_settings_dialog.h
    update_settings_dialog.ui)

list(APPEND SOURCE_CONSOLE_RESOURCES
    resources/console.qrc)

source_group("" FILES ${SOURCE_CONSOLE})
source_group(moc FILES ${SOURCE_CONSOLE_MOC})
source_group(resources FILES ${SOURCE_CONSOLE_RESOURCES})

add_executable(aspia_console
    ${SOURCE_CONSOLE}
    ${SOURCE_CONSOLE_RESOURCES})
set_target_properties(aspia_console PROPERTIES WIN32_EXECUTABLE TRUE)
target_link_libraries(aspia_console
    aspia_base
    aspia_client
    aspia_codec
    aspia_common
    aspia_crypto
    aspia_desktop
    aspia_net
    aspia_protocol
    aspia_updater
    ${THIRD_PARTY_LIBS})

if(Qt5LinguistTools_FOUND)
    # Get the list of Qt translation files.
    file(GLOB QT_QM_FILES ${ASPIA_THIRD_PARTY_DIR}/qt/translations/*.qm)
    add_qt_translations(aspia_console QRC_FILE "translations/qt_translations.qrc" QM_FILES ${QT_QM_FILES})

    # Get the list of Aspia translation files.
    file(GLOB CONSOLE_TS_FILES translations/*.ts)
    add_translations(aspia_console QRC_FILE "translations/console_translations.qrc" TS_FILES ${CONSOLE_TS_FILES})

    # Get the list of files containing messages.
    file(GLOB_RECURSE CONSOLE_TR_SOURCES
        ${PROJECT_SOURCE_DIR}/console/*.cc
        ${PROJECT_SOURCE_DIR}/console/*.cpp
        ${PROJECT_SOURCE_DIR}/console/*.h
        ${PROJECT_SOURCE_DIR}/console/*.ui)

    qt5_create_translation(TS_CONSOLE_UPDATE ${CONSOLE_TR_SOURCES} ${CONSOLE_TS_FILES})
    add_custom_target(aspia_console_ts_update DEPENDS ${TS_CONSOLE_UPDATE})
else()
    message(WARNING "Qt5 linguist tools not found. Internationalization support will be disabled.")
endif()
