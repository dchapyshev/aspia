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

include(tbb)
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
    computer_dialog_desktop.cc
    computer_dialog_desktop.h
    computer_dialog_desktop.ui
    computer_dialog_general.cc
    computer_dialog_general.h
    computer_dialog_general.ui
    computer_dialog_parent.cc
    computer_dialog_parent.h
    computer_dialog_parent.ui
    computer_dialog_tab.h
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
    console_application.cc
    console_application.h
    console_main.cc
    console_main_window.cc
    console_main_window.h
    console_main_window.ui
    console_settings.cc
    console_settings.h
    console_statusbar.cc
    console_statusbar.h
    console_tab.cc
    console_tab.h
    mru.cc
    mru.h
    mru_action.cc
    mru_action.h
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
set_target_properties(aspia_console PROPERTIES LINK_FLAGS "/MANIFEST:NO")
target_link_libraries(aspia_console
    aspia_base
    aspia_client
    aspia_codec
    aspia_common
    aspia_crypto
    aspia_desktop
    aspia_net
    aspia_proto
    aspia_qt_base
    aspia_updater
    ${QT_LIBS}
    ${WINDOWS_LIBS}
    ${THIRD_PARTY_LIBS})

if (USE_TBB)
    add_tbb(aspia_console ${ASPIA_THIRD_PARTY_DIR}/tbb)
endif()

if(Qt5LinguistTools_FOUND)
    # Get the list of translation files.
    file(GLOB CONSOLE_TS_FILES translations/*.ts)
    add_translations(aspia_console QRC_FILE "translations/console_translations.qrc" TS_FILES ${CONSOLE_TS_FILES})

    # Get the list of files containing messages.
    file(GLOB_RECURSE CONSOLE_TR_SOURCES
        ${PROJECT_SOURCE_DIR}/console/*.cc
        ${PROJECT_SOURCE_DIR}/console/*.cpp
        ${PROJECT_SOURCE_DIR}/console/*.h
        ${PROJECT_SOURCE_DIR}/console/*.ui)

    create_translation(TS_CONSOLE_UPDATE ${CONSOLE_TR_SOURCES} ${CONSOLE_TS_FILES})
    add_custom_target(aspia_console_ts_update DEPENDS ${TS_CONSOLE_UPDATE})
else()
    message(WARNING "Qt5 linguist tools not found. Internationalization support will be disabled.")
endif()

if(NOT TARGET BUILD_CONSOLE)
    add_custom_target(BUILD_CONSOLE)
    add_dependencies(BUILD_CONSOLE aspia_console)
endif()
