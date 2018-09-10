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
    ${PROJECT_SOURCE_DIR}/client/client.cc
    ${PROJECT_SOURCE_DIR}/client/client.h
    ${PROJECT_SOURCE_DIR}/client/client_connections.cc
    ${PROJECT_SOURCE_DIR}/client/client_connections.h
    ${PROJECT_SOURCE_DIR}/client/client_session.h
    ${PROJECT_SOURCE_DIR}/client/client_session_desktop_manage.cc
    ${PROJECT_SOURCE_DIR}/client/client_session_desktop_manage.h
    ${PROJECT_SOURCE_DIR}/client/client_session_desktop_view.cc
    ${PROJECT_SOURCE_DIR}/client/client_session_desktop_view.h
    ${PROJECT_SOURCE_DIR}/client/client_session_file_transfer.cc
    ${PROJECT_SOURCE_DIR}/client/client_session_file_transfer.h
    ${PROJECT_SOURCE_DIR}/client/config_factory.cc
    ${PROJECT_SOURCE_DIR}/client/config_factory.h
    ${PROJECT_SOURCE_DIR}/client/connect_data.cc
    ${PROJECT_SOURCE_DIR}/client/connect_data.h
    ${PROJECT_SOURCE_DIR}/client/file_remove_queue_builder.cc
    ${PROJECT_SOURCE_DIR}/client/file_remove_queue_builder.h
    ${PROJECT_SOURCE_DIR}/client/file_remove_task.cc
    ${PROJECT_SOURCE_DIR}/client/file_remove_task.h
    ${PROJECT_SOURCE_DIR}/client/file_remover.cc
    ${PROJECT_SOURCE_DIR}/client/file_remover.h
    ${PROJECT_SOURCE_DIR}/client/file_status.cc
    ${PROJECT_SOURCE_DIR}/client/file_status.h
    ${PROJECT_SOURCE_DIR}/client/file_transfer.cc
    ${PROJECT_SOURCE_DIR}/client/file_transfer.h
    ${PROJECT_SOURCE_DIR}/client/file_transfer_queue_builder.cc
    ${PROJECT_SOURCE_DIR}/client/file_transfer_queue_builder.h
    ${PROJECT_SOURCE_DIR}/client/file_transfer_task.cc
    ${PROJECT_SOURCE_DIR}/client/file_transfer_task.h)

list(APPEND SOURCE_CLIENT_RESOURCES
    ${PROJECT_SOURCE_DIR}/client/resources/client.qrc)

list(APPEND SOURCE_CLIENT_UI
    ${PROJECT_SOURCE_DIR}/client/ui/address_bar.cc
    ${PROJECT_SOURCE_DIR}/client/ui/address_bar.h
    ${PROJECT_SOURCE_DIR}/client/ui/address_bar_model.cc
    ${PROJECT_SOURCE_DIR}/client/ui/address_bar_model.h
    ${PROJECT_SOURCE_DIR}/client/ui/authorization_dialog.cc
    ${PROJECT_SOURCE_DIR}/client/ui/authorization_dialog.h
    ${PROJECT_SOURCE_DIR}/client/ui/authorization_dialog.ui
    ${PROJECT_SOURCE_DIR}/client/ui/category_group_tree_item.cc
    ${PROJECT_SOURCE_DIR}/client/ui/category_group_tree_item.h
    ${PROJECT_SOURCE_DIR}/client/ui/category_tree_item.cc
    ${PROJECT_SOURCE_DIR}/client/ui/category_tree_item.h
    ${PROJECT_SOURCE_DIR}/client/ui/client_dialog.cc
    ${PROJECT_SOURCE_DIR}/client/ui/client_dialog.h
    ${PROJECT_SOURCE_DIR}/client/ui/client_dialog.ui
    ${PROJECT_SOURCE_DIR}/client/ui/desktop_config_dialog.cc
    ${PROJECT_SOURCE_DIR}/client/ui/desktop_config_dialog.h
    ${PROJECT_SOURCE_DIR}/client/ui/desktop_config_dialog.ui
    ${PROJECT_SOURCE_DIR}/client/ui/file_name_validator.cc
    ${PROJECT_SOURCE_DIR}/client/ui/file_name_validator.h
    ${PROJECT_SOURCE_DIR}/client/ui/desktop_panel.cc
    ${PROJECT_SOURCE_DIR}/client/ui/desktop_panel.h
    ${PROJECT_SOURCE_DIR}/client/ui/desktop_panel.ui
    ${PROJECT_SOURCE_DIR}/client/ui/desktop_widget.cc
    ${PROJECT_SOURCE_DIR}/client/ui/desktop_widget.h
    ${PROJECT_SOURCE_DIR}/client/ui/desktop_window.cc
    ${PROJECT_SOURCE_DIR}/client/ui/desktop_window.h
    ${PROJECT_SOURCE_DIR}/client/ui/file_item_delegate.cc
    ${PROJECT_SOURCE_DIR}/client/ui/file_item_delegate.h
    ${PROJECT_SOURCE_DIR}/client/ui/file_list.cc
    ${PROJECT_SOURCE_DIR}/client/ui/file_list.h
    ${PROJECT_SOURCE_DIR}/client/ui/file_list_model.cc
    ${PROJECT_SOURCE_DIR}/client/ui/file_list_model.h
    ${PROJECT_SOURCE_DIR}/client/ui/file_manager_settings.cc
    ${PROJECT_SOURCE_DIR}/client/ui/file_manager_settings.h
    ${PROJECT_SOURCE_DIR}/client/ui/file_manager_window.cc
    ${PROJECT_SOURCE_DIR}/client/ui/file_manager_window.h
    ${PROJECT_SOURCE_DIR}/client/ui/file_manager_window.ui
    ${PROJECT_SOURCE_DIR}/client/ui/file_mime_data.cc
    ${PROJECT_SOURCE_DIR}/client/ui/file_mime_data.h
    ${PROJECT_SOURCE_DIR}/client/ui/file_path_validator.cc
    ${PROJECT_SOURCE_DIR}/client/ui/file_path_validator.h
    ${PROJECT_SOURCE_DIR}/client/ui/file_panel.cc
    ${PROJECT_SOURCE_DIR}/client/ui/file_panel.h
    ${PROJECT_SOURCE_DIR}/client/ui/file_panel.ui
    ${PROJECT_SOURCE_DIR}/client/ui/file_remove_dialog.cc
    ${PROJECT_SOURCE_DIR}/client/ui/file_remove_dialog.h
    ${PROJECT_SOURCE_DIR}/client/ui/file_remove_dialog.ui
    ${PROJECT_SOURCE_DIR}/client/ui/file_transfer_dialog.cc
    ${PROJECT_SOURCE_DIR}/client/ui/file_transfer_dialog.h
    ${PROJECT_SOURCE_DIR}/client/ui/file_transfer_dialog.ui
    ${PROJECT_SOURCE_DIR}/client/ui/key_sequence_dialog.cc
    ${PROJECT_SOURCE_DIR}/client/ui/key_sequence_dialog.h
    ${PROJECT_SOURCE_DIR}/client/ui/key_sequence_dialog.ui
    ${PROJECT_SOURCE_DIR}/client/ui/select_screen_action.h
    ${PROJECT_SOURCE_DIR}/client/ui/status_dialog.cc
    ${PROJECT_SOURCE_DIR}/client/ui/status_dialog.h
    ${PROJECT_SOURCE_DIR}/client/ui/status_dialog.ui)

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
