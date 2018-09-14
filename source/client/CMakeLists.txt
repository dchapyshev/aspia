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

list(APPEND SOURCE_CLIENT
    client.cc
    client.h
    client_connections.cc
    client_connections.h
    client_session.h
    client_session_desktop_manage.cc
    client_session_desktop_manage.h
    client_session_desktop_view.cc
    client_session_desktop_view.h
    client_session_file_transfer.cc
    client_session_file_transfer.h
    config_factory.cc
    config_factory.h
    connect_data.cc
    connect_data.h
    file_remove_queue_builder.cc
    file_remove_queue_builder.h
    file_remove_task.cc
    file_remove_task.h
    file_remover.cc
    file_remover.h
    file_status.cc
    file_status.h
    file_transfer.cc
    file_transfer.h
    file_transfer_queue_builder.cc
    file_transfer_queue_builder.h
    file_transfer_task.cc
    file_transfer_task.h)

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
    ui/desktop_window.cc
    ui/desktop_window.h
    ui/file_item_delegate.cc
    ui/file_item_delegate.h
    ui/file_list.cc
    ui/file_list.h
    ui/file_list_model.cc
    ui/file_list_model.h
    ui/file_manager_settings.cc
    ui/file_manager_settings.h
    ui/file_manager_window.cc
    ui/file_manager_window.h
    ui/file_manager_window.ui
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
    ui/key_sequence_dialog.cc
    ui/key_sequence_dialog.h
    ui/key_sequence_dialog.ui
    ui/select_screen_action.h
    ui/status_dialog.cc
    ui/status_dialog.h
    ui/status_dialog.ui)

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
    aspia_crypto
    aspia_desktop_capture
    aspia_network
    aspia_protocol
    aspia_share
    ${THIRD_PARTY_LIBS})
