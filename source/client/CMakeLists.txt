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

include(translations)

list(APPEND SOURCE_CLIENT
    client.cc
    client.h
    client_authenticator.cc
    client_authenticator.h
    client_config.cc
    client_config.h
    client_desktop.cc
    client_desktop.h
    client_file_transfer.cc
    client_file_transfer.h
    config_factory.cc
    config_factory.h
    desktop_control.h
    desktop_control_proxy.cc
    desktop_control_proxy.h
    desktop_window.h
    desktop_window_proxy.cc
    desktop_window_proxy.h
    file_control.h
    file_control_proxy.cc
    file_control_proxy.h
    file_manager_window.h
    file_manager_window_proxy.cc
    file_manager_window_proxy.h
    file_remove_queue_builder.cc
    file_remove_queue_builder.h
    file_remove_window.h
    file_remove_window_proxy.cc
    file_remove_window_proxy.h
    file_remover.cc
    file_remover.h
    file_remover_proxy.cc
    file_remover_proxy.h
    file_transfer.cc
    file_transfer.h
    file_transfer_proxy.cc
    file_transfer_proxy.h
    file_transfer_queue_builder.cc
    file_transfer_queue_builder.h
    file_transfer_window.h
    file_transfer_window_proxy.cc
    file_transfer_window_proxy.h
    frame_factory.h
    status_window.h
    status_window_proxy.cc
    status_window_proxy.h)

list(APPEND SOURCE_CLIENT_RESOURCES
    resources/client.qrc)

list(APPEND SOURCE_CLIENT_UI
    ui/address_bar.cc
    ui/address_bar.h
    ui/address_bar_model.cc
    ui/address_bar_model.h
    ui/authorization_dialog.cc
    ui/authorization_dialog.h
    ui/authorization_dialog.ui
    ui/client_dialog.cc
    ui/client_dialog.h
    ui/client_dialog.ui
    ui/client_settings.cc
    ui/client_settings.h
    ui/client_window.cc
    ui/client_window.h
    ui/desktop_config_dialog.cc
    ui/desktop_config_dialog.h
    ui/desktop_config_dialog.ui
    ui/file_name_validator.cc
    ui/file_name_validator.h
    ui/desktop_panel.cc
    ui/desktop_panel.h
    ui/desktop_panel.ui
    ui/desktop_widget.cc
    ui/desktop_widget.h
    ui/file_error_code.cc
    ui/file_error_code.h
    ui/desktop_settings.cc
    ui/desktop_settings.h
    ui/file_item_delegate.cc
    ui/file_item_delegate.h
    ui/file_list.cc
    ui/file_list.h
    ui/file_list_model.cc
    ui/file_list_model.h
    ui/file_manager_settings.cc
    ui/file_manager_settings.h
    ui/file_mime_data.cc
    ui/file_mime_data.h
    ui/file_path_validator.cc
    ui/file_path_validator.h
    ui/file_panel.cc
    ui/file_panel.h
    ui/file_panel.ui
    ui/file_remove_dialog.cc
    ui/file_remove_dialog.h
    ui/file_remove_dialog.ui
    ui/file_transfer_dialog.cc
    ui/file_transfer_dialog.h
    ui/file_transfer_dialog.ui
    ui/frame_factory_qimage.cc
    ui/frame_factory_qimage.h
    ui/frame_qimage.cc
    ui/frame_qimage.h
    ui/qt_desktop_window.cc
    ui/qt_desktop_window.h
    ui/qt_file_manager_window.cc
    ui/qt_file_manager_window.h
    ui/qt_file_manager_window.ui
    ui/select_screen_action.h
    ui/status_dialog.cc
    ui/status_dialog.h
    ui/status_dialog.ui
    ui/system_info_window.cc
    ui/system_info_window.h
    ui/system_info_window.ui
    ui/tree_to_html.cc
    ui/tree_to_html.h)

source_group("" FILES ${SOURCE_CLIENT})
source_group(resources FILES ${SOURCE_CLIENT_RESOURCES})
source_group(ui FILES ${SOURCE_CLIENT_UI})

add_library(aspia_client STATIC
    ${SOURCE_CLIENT}
    ${SOURCE_CLIENT_RESOURCES}
    ${SOURCE_CLIENT_UI})
target_link_libraries(aspia_client
    aspia_base
    aspia_codec
    aspia_common
    aspia_crypto
    aspia_desktop
    aspia_net
    aspia_proto
    ${THIRD_PARTY_LIBS})
set_target_properties(aspia_client PROPERTIES COMPILE_DEFINITIONS "CLIENT_IMPLEMENTATION")

if(Qt5LinguistTools_FOUND)
    # Get the list of translation files.
    file(GLOB CLIENT_TS_FILES translations/*.ts)

    add_translations(aspia_client QRC_FILE "translations/client_translations.qrc" TS_FILES ${CLIENT_TS_FILES})

    # Get the list of files containing messages.
    file(GLOB_RECURSE CLIENT_TR_SOURCES
        ${PROJECT_SOURCE_DIR}/client/*.cc
        ${PROJECT_SOURCE_DIR}/client/*.cpp
        ${PROJECT_SOURCE_DIR}/client/*.h
        ${PROJECT_SOURCE_DIR}/client/*.ui)

    create_translation(TS_CLIENT_UPDATE ${CLIENT_TR_SOURCES} ${CLIENT_TS_FILES})
    add_custom_target(aspia_client_ts_update DEPENDS ${TS_CLIENT_UPDATE})
else()
    message(WARNING "Qt5 linguist tools not found. Internationalization support will be disabled.")
endif()
