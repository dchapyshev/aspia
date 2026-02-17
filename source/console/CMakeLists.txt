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

list(APPEND SOURCE_CONSOLE
    address_book_dialog.cc
    address_book_dialog.h
    address_book_dialog.ui
    address_book_tab.cc
    address_book_tab.h
    address_book_tab.ui
    application.cc
    application.h
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
    computer_group_dialog_desktop.cc
    computer_group_dialog_desktop.h
    computer_group_dialog_desktop.ui
    computer_group_dialog_general.cc
    computer_group_dialog_general.h
    computer_group_dialog_general.ui
    computer_group_dialog_parent.cc
    computer_group_dialog_parent.h
    computer_group_dialog_parent.ui
    computer_group_dialog_tab.h
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
    main.cc
    fast_connect_dialog.cc
    fast_connect_dialog.h
    fast_connect_dialog.ui
    import_export_util.cc
    import_export_util.h
    main_window.cc
    main_window.h
    main_window.ui
    mru.cc
    mru.h
    mru_action.cc
    mru_action.h
    open_address_book_dialog.cc
    open_address_book_dialog.h
    open_address_book_dialog.ui
    settings.cc
    settings.h
    statusbar.cc
    statusbar.h
    update_settings_dialog.cc
    update_settings_dialog.h
    update_settings_dialog.ui)

source_group("" FILES ${SOURCE_CONSOLE})
source_group(moc FILES ${SOURCE_CONSOLE_MOC})

if (WIN32)
    set(CONSOLE_PLATFORM_LIBS
        avrt crypt32 dbghelp dwmapi imm32 iphlpapi netapi32 shlwapi userenv uxtheme version
        winmm wtsapi32)
endif()

if (APPLE)
    set(CONSOLE_PLATFORM_LIBS ${COREAUDIO_LIB} ${AUDIOTOOLBOX_LIB} portaudio)
    set(aspia_console_ICON ${PROJECT_SOURCE_DIR}/source/common/resources/aspia-console.icns)
    set(aspia_console_PLIST ${PROJECT_SOURCE_DIR}/source/console/Info.plist)

    set_source_files_properties(${aspia_console_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
endif()

if (LINUX)
    set(CONSOLE_PLATFORM_LIBS stdc++fs ${XFIXES_LIB})
endif()

add_executable(aspia_console MACOSX_BUNDLE ${aspia_console_ICON} ${SOURCE_CONSOLE})

if (APPLE)
    set_target_properties(aspia_console PROPERTIES OUTPUT_NAME "Aspia Console")
    set_target_properties(aspia_console PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${aspia_console_PLIST})
    set_target_properties(aspia_console PROPERTIES MACOSX_BUNDLE_ICONFILE ${aspia_console_ICON})
endif()

if (WIN32)
    set_target_properties(aspia_console PROPERTIES WIN32_EXECUTABLE TRUE)
    set_target_properties(aspia_console PROPERTIES LINK_FLAGS "/MANIFEST:NO")
endif()

target_link_libraries(aspia_console
    aspia_base
    aspia_client_core
    aspia_common
    aspia_proto
    ${CONSOLE_PLATFORM_LIBS}
    ${QT_COMMON_LIBS}
    ${QT_PLATFORM_LIBS}
    ${THIRD_PARTY_LIBS})
set_property(TARGET aspia_console PROPERTY AUTOMOC ON)
set_property(TARGET aspia_console PROPERTY AUTOUIC ON)
set_property(TARGET aspia_console PROPERTY AUTORCC ON)
qt_import_plugins(aspia_console INCLUDE "" EXCLUDE "" INCLUDE_BY_TYPE "" EXCLUDE_BY_TYPE imageformats)
