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

list(APPEND SOURCE_CLIENT_CORE
    aab_importer.cc
    aab_importer.h
    application.cc
    application.h
    database.cc
    database.h
    client.cc
    client.h
    config.cc
    config.h
    client_desktop.cc
    client_desktop.h
    client_file_transfer.cc
    client_file_transfer.h
    main.cc
    client_system_info.cc
    client_system_info.h
    client_text_chat.cc
    client_text_chat.h
    file_remove_queue_builder.cc
    file_remove_queue_builder.h
    file_remover.cc
    file_remover.h
    file_transfer.cc
    file_transfer.h
    file_transfer_queue_builder.cc
    file_transfer_queue_builder.h
    json_backup.cc
    json_backup.h
    master_password.cc
    master_password.h
    pch.h
    router.cc
    router.h
    session_keeper.cc
    session_keeper.h
    session_state.cc
    session_state.h
    settings.cc
    settings.h
    udp_attempt.cc
    udp_attempt.h)

if (WIN32)
    list(APPEND SOURCE_CLIENT_CORE client.rc)
endif()

list(APPEND SOURCE_CLIENT_ANDROID
    android/about_widget.cc
    android/about_widget.h
    android/biometric_gate.cc
    android/biometric_gate.h
    android/desktop_view.cc
    android/desktop_view.h
    android/desktop_window.cc
    android/desktop_window.h
    android/key_bar.cc
    android/key_bar.h
    android/local_group_editor.cc
    android/local_group_editor.h
    android/local_host_editor.cc
    android/local_host_editor.h
    android/local_widget.cc
    android/local_widget.h
    android/main_window.cc
    android/main_window.h
    android/master_password_dialog.cc
    android/master_password_dialog.h
    android/password_dialog.cc
    android/password_dialog.h
    android/remote_widget.cc
    android/remote_widget.h
    android/router_card.cc
    android/router_card.h
    android/router_editor.cc
    android/router_editor.h
    android/routers_widget.cc
    android/routers_widget.h
    android/session_keeper_android.cc
    android/session_keeper_android.h
    android/settings_widget.cc
    android/settings_widget.h
    android/statistics_dialog.cc
    android/statistics_dialog.h
    android/two_factor_dialog.cc
    android/two_factor_dialog.h)

list(APPEND SOURCE_CLIENT_CORE_ONLINE_CHECKER
    online_checker/online_checker.cc
    online_checker/online_checker.h
    online_checker/online_checker_direct.cc
    online_checker/online_checker_direct.h
    online_checker/online_checker_router.cc
    online_checker/online_checker_router.h)

list(APPEND SOURCE_CLIENT_CORE_DESKTOP
    desktop/authorization_dialog.cc
    desktop/authorization_dialog.h
    desktop/authorization_dialog.ui
    desktop/client_tab.cc
    desktop/client_tab.h
    desktop/client_window.cc
    desktop/client_window.h
    desktop/hosts_tab.cc
    desktop/hosts_tab.h
    desktop/hosts_tab.ui
    desktop/main_window.cc
    desktop/main_window.h
    desktop/main_window.ui
    desktop/tab.cc
    desktop/tab.h
    desktop/tab_bar.cc
    desktop/tab_bar.h
    desktop/tab_widget.cc
    desktop/tab_widget.h
    desktop/router_dialog.cc
    desktop/router_dialog.h
    desktop/router_dialog.ui
    desktop/settings_tab.cc
    desktop/settings_tab.h
    desktop/settings_tab.ui
)

list(APPEND SOURCE_CLIENT_CORE_DESKTOP_DESKTOP
    desktop/desktop/desktop_window.cc
    desktop/desktop/desktop_window.h
    desktop/desktop/desktop_toolbar.cc
    desktop/desktop/desktop_toolbar.h
    desktop/desktop/desktop_toolbar.ui
    desktop/desktop/desktop_widget.cc
    desktop/desktop/desktop_widget.h
    desktop/desktop/select_screen_action.h
    desktop/desktop/statistics_dialog.cc
    desktop/desktop/statistics_dialog.h
    desktop/desktop/statistics_dialog.ui
    desktop/desktop/task_manager_settings.cc
    desktop/desktop/task_manager_settings.h
    desktop/desktop/task_manager_window.cc
    desktop/desktop/task_manager_window.h
    desktop/desktop/task_manager_window.ui)

list(APPEND SOURCE_CLIENT_CORE_DESKTOP_FILE_TRANSFER
    desktop/file_transfer/address_bar.cc
    desktop/file_transfer/address_bar.h
    desktop/file_transfer/address_bar_model.cc
    desktop/file_transfer/address_bar_model.h
    desktop/file_transfer/file_error_code.cc
    desktop/file_transfer/file_error_code.h
    desktop/file_transfer/file_item_delegate.cc
    desktop/file_transfer/file_item_delegate.h
    desktop/file_transfer/file_list.cc
    desktop/file_transfer/file_list.h
    desktop/file_transfer/file_list_model.cc
    desktop/file_transfer/file_list_model.h
    desktop/file_transfer/file_mime_data.cc
    desktop/file_transfer/file_mime_data.h
    desktop/file_transfer/file_name_validator.cc
    desktop/file_transfer/file_name_validator.h
    desktop/file_transfer/file_panel.cc
    desktop/file_transfer/file_panel.h
    desktop/file_transfer/file_panel.ui
    desktop/file_transfer/file_remove_widget.cc
    desktop/file_transfer/file_remove_widget.h
    desktop/file_transfer/file_remove_widget.ui
    desktop/file_transfer/file_transfer_widget.cc
    desktop/file_transfer/file_transfer_widget.h
    desktop/file_transfer/file_transfer_widget.ui
    desktop/file_transfer/file_transfer_window.cc
    desktop/file_transfer/file_transfer_window.h
    desktop/file_transfer/file_transfer_window.ui)

list(APPEND SOURCE_CLIENT_CORE_DESKTOP_SYS_INFO
    desktop/sys_info/sys_info_widget.cc
    desktop/sys_info/sys_info_widget.h
    desktop/sys_info/sys_info_widget_applications.cc
    desktop/sys_info/sys_info_widget_applications.h
    desktop/sys_info/sys_info_widget_applications.ui
    desktop/sys_info/sys_info_widget_connections.cc
    desktop/sys_info/sys_info_widget_connections.h
    desktop/sys_info/sys_info_widget_connections.ui
    desktop/sys_info/sys_info_widget_devices.cc
    desktop/sys_info/sys_info_widget_devices.h
    desktop/sys_info/sys_info_widget_devices.ui
    desktop/sys_info/sys_info_widget_drivers.cc
    desktop/sys_info/sys_info_widget_drivers.h
    desktop/sys_info/sys_info_widget_drivers.ui
    desktop/sys_info/sys_info_widget_env_vars.cc
    desktop/sys_info/sys_info_widget_env_vars.h
    desktop/sys_info/sys_info_widget_env_vars.ui
    desktop/sys_info/sys_info_widget_event_logs.cc
    desktop/sys_info/sys_info_widget_event_logs.h
    desktop/sys_info/sys_info_widget_event_logs.ui
    desktop/sys_info/sys_info_widget_licenses.cc
    desktop/sys_info/sys_info_widget_licenses.h
    desktop/sys_info/sys_info_widget_licenses.ui
    desktop/sys_info/sys_info_widget_local_users.cc
    desktop/sys_info/sys_info_widget_local_users.h
    desktop/sys_info/sys_info_widget_local_users.ui
    desktop/sys_info/sys_info_widget_local_user_groups.cc
    desktop/sys_info/sys_info_widget_local_user_groups.h
    desktop/sys_info/sys_info_widget_local_user_groups.ui
    desktop/sys_info/sys_info_widget_monitors.cc
    desktop/sys_info/sys_info_widget_monitors.h
    desktop/sys_info/sys_info_widget_monitors.ui
    desktop/sys_info/sys_info_widget_net_shares.cc
    desktop/sys_info/sys_info_widget_net_shares.h
    desktop/sys_info/sys_info_widget_net_shares.ui
    desktop/sys_info/sys_info_widget_net_adapters.cc
    desktop/sys_info/sys_info_widget_net_adapters.h
    desktop/sys_info/sys_info_widget_net_adapters.ui
    desktop/sys_info/sys_info_widget_open_files.cc
    desktop/sys_info/sys_info_widget_open_files.h
    desktop/sys_info/sys_info_widget_open_files.ui
    desktop/sys_info/sys_info_widget_power_options.cc
    desktop/sys_info/sys_info_widget_power_options.h
    desktop/sys_info/sys_info_widget_power_options.ui
    desktop/sys_info/sys_info_widget_printers.cc
    desktop/sys_info/sys_info_widget_printers.h
    desktop/sys_info/sys_info_widget_printers.ui
    desktop/sys_info/sys_info_widget_processes.cc
    desktop/sys_info/sys_info_widget_processes.h
    desktop/sys_info/sys_info_widget_processes.ui
    desktop/sys_info/sys_info_widget_routes.cc
    desktop/sys_info/sys_info_widget_routes.h
    desktop/sys_info/sys_info_widget_routes.ui
    desktop/sys_info/sys_info_widget_services.cc
    desktop/sys_info/sys_info_widget_services.h
    desktop/sys_info/sys_info_widget_services.ui
    desktop/sys_info/sys_info_widget_summary.cc
    desktop/sys_info/sys_info_widget_summary.h
    desktop/sys_info/sys_info_widget_summary.ui
    desktop/sys_info/sys_info_widget_video_adapters.cc
    desktop/sys_info/sys_info_widget_video_adapters.h
    desktop/sys_info/sys_info_widget_video_adapters.ui
    desktop/sys_info/system_info_window.cc
    desktop/sys_info/system_info_window.h
    desktop/sys_info/system_info_window.ui
    desktop/sys_info/tree_to_html.cc
    desktop/sys_info/tree_to_html.h)

list(APPEND SOURCE_CLIENT_CORE_DESKTOP_CHAT
    desktop/chat/chat_window.cc
    desktop/chat/chat_window.h
    desktop/chat/chat_window.ui)

list(APPEND SOURCE_CLIENT_CORE_DESKTOP_HOSTS_TAB
    desktop/hosts/content_widget.cc
    desktop/hosts/content_widget.h
    desktop/hosts/drag_and_drop.cc
    desktop/hosts/drag_and_drop.h
    desktop/hosts/group_combo_box.cc
    desktop/hosts/group_combo_box.h
    desktop/hosts/local_group_dialog.cc
    desktop/hosts/local_group_dialog.h
    desktop/hosts/local_group_dialog.ui
    desktop/hosts/local_group_widget.cc
    desktop/hosts/local_group_widget.h
    desktop/hosts/local_group_widget.ui
    desktop/hosts/local_host_dialog.cc
    desktop/hosts/local_host_dialog.h
    desktop/hosts/local_host_dialog.ui
    desktop/hosts/router_clients_widget.cc
    desktop/hosts/router_clients_widget.h
    desktop/hosts/router_clients_widget.ui
    desktop/hosts/router_group_dialog.cc
    desktop/hosts/router_group_dialog.h
    desktop/hosts/router_group_dialog.ui
    desktop/hosts/router_group_widget.cc
    desktop/hosts/router_group_widget.h
    desktop/hosts/router_group_widget.ui
    desktop/hosts/router_host_dialog.cc
    desktop/hosts/router_host_dialog.h
    desktop/hosts/router_host_dialog.ui
    desktop/hosts/router_hosts_widget.cc
    desktop/hosts/router_hosts_widget.h
    desktop/hosts/router_hosts_widget.ui
    desktop/hosts/router_relays_widget.cc
    desktop/hosts/router_relays_widget.h
    desktop/hosts/router_relays_widget.ui
    desktop/hosts/router_status_widget.cc
    desktop/hosts/router_status_widget.h
    desktop/hosts/router_status_widget.ui
    desktop/hosts/router_user_dialog.cc
    desktop/hosts/router_user_dialog.h
    desktop/hosts/router_user_dialog.ui
    desktop/hosts/router_users_widget.cc
    desktop/hosts/router_users_widget.h
    desktop/hosts/router_users_widget.ui
    desktop/hosts/router_workspace_dialog.cc
    desktop/hosts/router_workspace_dialog.h
    desktop/hosts/router_workspace_dialog.ui
    desktop/hosts/search_dialog.cc
    desktop/hosts/search_dialog.h
    desktop/hosts/search_dialog.ui
    desktop/hosts/search_widget.cc
    desktop/hosts/search_widget.h
    desktop/hosts/sidebar.cc
    desktop/hosts/sidebar.h
    desktop/hosts/sidebar_items.cc
    desktop/hosts/sidebar_items.h)

source_group("" FILES ${SOURCE_CLIENT_CORE})
source_group(android FILES ${SOURCE_CLIENT_ANDROID})
source_group(online_checker FILES ${SOURCE_CLIENT_CORE_ONLINE_CHECKER})
source_group(desktop FILES ${SOURCE_CLIENT_CORE_DESKTOP})
source_group(desktop\\desktop FILES ${SOURCE_CLIENT_CORE_DESKTOP_DESKTOP})
source_group(desktop\\file_transfer FILES ${SOURCE_CLIENT_CORE_DESKTOP_FILE_TRANSFER})
source_group(desktop\\router_manager FILES ${SOURCE_CLIENT_CORE_DESKTOP_ROUTER_MANAGER})
source_group(desktop\\sys_info FILES ${SOURCE_CLIENT_CORE_DESKTOP_SYS_INFO})
source_group(desktop\\chat FILES ${SOURCE_CLIENT_CORE_DESKTOP_CHAT})
source_group(desktop\\hosts FILES ${SOURCE_CLIENT_CORE_DESKTOP_HOSTS_TAB})

if (WIN32)
    set(CLIENT_PLATFORM_LIBS
        avrt crypt32 dbghelp dwmapi imm32 iphlpapi netapi32 shlwapi userenv uxtheme version
        winmm wtsapi32)
endif()

if (LINUX)
    set(CLIENT_PLATFORM_LIBS stdc++fs ${XFIXES_LIB})
endif()

if (APPLE)
    set(CLIENT_PLATFORM_LIBS ${COREAUDIO_LIB} ${AUDIOTOOLBOX_LIB} portaudio)
    set(aspia_client_ICON ${PROJECT_SOURCE_DIR}/source/common/resources/aspia-client.icns)
    set(aspia_client_PLIST ${PROJECT_SOURCE_DIR}/source/client/Info.plist)

    set_source_files_properties(${aspia_client_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
endif()

if (ANDROID)
    qt_add_executable(aspia_client
        ${SOURCE_CLIENT_CORE}
        ${SOURCE_CLIENT_ANDROID}
        ${SOURCE_CLIENT_CORE_ONLINE_CHECKER})
else()
    add_executable(aspia_client MACOSX_BUNDLE
        ${aspia_client_ICON}
        ${SOURCE_CLIENT_CORE}
        ${SOURCE_CLIENT_CORE_ONLINE_CHECKER}
        ${SOURCE_CLIENT_CORE_DESKTOP}
        ${SOURCE_CLIENT_CORE_DESKTOP_DESKTOP}
        ${SOURCE_CLIENT_CORE_DESKTOP_FILE_TRANSFER}
        ${SOURCE_CLIENT_CORE_DESKTOP_ROUTER_MANAGER}
        ${SOURCE_CLIENT_CORE_DESKTOP_SYS_INFO}
        ${SOURCE_CLIENT_CORE_DESKTOP_CHAT}
        ${SOURCE_CLIENT_CORE_DESKTOP_HOSTS_TAB})
endif()

target_link_libraries(aspia_client PRIVATE
    aspia_base
    aspia_common
    aspia_proto
    ${CLIENT_PLATFORM_LIBS}
    ${QT_COMMON_LIBS}
    ${QT_PLATFORM_LIBS}
    ${THIRD_PARTY_LIBS})
set_property(TARGET aspia_client PROPERTY AUTOMOC ON)
set_property(TARGET aspia_client PROPERTY AUTOUIC ON)
set_property(TARGET aspia_client PROPERTY AUTORCC ON)
qt_import_plugins(aspia_client INCLUDE "" EXCLUDE "" INCLUDE_BY_TYPE imageformats iconengines EXCLUDE_BY_TYPE "")

target_precompile_headers(aspia_client PRIVATE pch.h)

if (ANDROID)
    set_target_properties(aspia_client PROPERTIES
        QT_ANDROID_PACKAGE_NAME "org.aspia.client"
        QT_ANDROID_APP_NAME "Aspia Client"
        QT_ANDROID_APP_ICON "@mipmap/ic_launcher"
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android/packaging")
endif()

if (WIN32)
    set_target_properties(aspia_client PROPERTIES WIN32_EXECUTABLE TRUE)
    set_target_properties(aspia_client PROPERTIES LINK_FLAGS "/MANIFEST:NO")
endif()

if (APPLE)
    set_target_properties(aspia_client PROPERTIES OUTPUT_NAME "Aspia Client")
    set_target_properties(aspia_client PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${aspia_client_PLIST})
    set_target_properties(aspia_client PROPERTIES MACOSX_BUNDLE_ICONFILE ${aspia_client_ICON})
endif()
