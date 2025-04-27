#
# Aspia Project
# Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

list(APPEND SOURCE_CLIENT_CORE
    client.cc
    client.h
    client_config.cc
    client_config.h
    client_desktop.cc
    client_desktop.h
    client_file_transfer.cc
    client_file_transfer.h
    client_main.cc
    client_main.h
    client_port_forwarding.cc
    client_port_forwarding.h
    client_proxy.cc
    client_proxy.h
    client_system_info.cc
    client_system_info.h
    client_text_chat.cc
    client_text_chat.h
    config_factory.cc
    config_factory.h
    file_control.h
    file_control_proxy.cc
    file_control_proxy.h
    file_manager_window.h
    file_manager_window_proxy.cc
    file_manager_window_proxy.h
    file_remove_queue_builder.cc
    file_remove_queue_builder.h
    file_remover.cc
    file_remover.h
    file_transfer.cc
    file_transfer.h
    file_transfer_proxy.cc
    file_transfer_proxy.h
    file_transfer_queue_builder.cc
    file_transfer_queue_builder.h
    file_transfer_window.h
    file_transfer_window_proxy.cc
    file_transfer_window_proxy.h
    input_event_filter.cc
    input_event_filter.h
    router.cc
    router.h
    router_proxy.cc
    router_proxy.h
    router_window.h
    router_window_proxy.cc
    router_window_proxy.h
    router_config.cc
    router_config.h
    router_config_storage.cc
    router_config_storage.h
    router_controller.cc
    router_controller.h
    status_window.h
    status_window_proxy.cc
    status_window_proxy.h
    client_session_state.cc
    client_session_state.h)

list(APPEND SOURCE_CLIENT_CORE_RESOURCES
    resources/client.qrc)

list(APPEND SOURCE_CLIENT_CORE_ONLINE_CHECKER
    online_checker/online_checker.cc
    online_checker/online_checker.h
    online_checker/online_checker_direct.cc
    online_checker/online_checker_direct.h
    online_checker/online_checker_router.cc
    online_checker/online_checker_router.h)

list(APPEND SOURCE_CLIENT_CORE_UI
    ui/application.cc
    ui/application.h
    ui/authorization_dialog.cc
    ui/authorization_dialog.h
    ui/authorization_dialog.ui
    ui/client_window.cc
    ui/client_window.h
    ui/client_window.ui
    ui/client_settings.cc
    ui/client_settings.h
    ui/client_settings_dialog.cc
    ui/client_settings_dialog.h
    ui/client_settings_dialog.ui
    ui/session_window.cc
    ui/session_window.h
    ui/update_settings_dialog.cc
    ui/update_settings_dialog.h
    ui/update_settings_dialog.ui)

list(APPEND SOURCE_CLIENT_CORE_UI_DESKTOP
    ui/desktop/desktop_config_dialog.cc
    ui/desktop/desktop_config_dialog.h
    ui/desktop/desktop_config_dialog.ui
    ui/desktop/desktop_toolbar.cc
    ui/desktop/desktop_toolbar.h
    ui/desktop/desktop_toolbar.ui
    ui/desktop/desktop_widget.cc
    ui/desktop/desktop_widget.h
    ui/desktop/desktop_settings.cc
    ui/desktop/desktop_settings.h
    ui/desktop/frame_qimage.cc
    ui/desktop/frame_qimage.h
    ui/desktop/qt_desktop_window.cc
    ui/desktop/qt_desktop_window.h
    ui/desktop/record_settings_dialog.cc
    ui/desktop/record_settings_dialog.h
    ui/desktop/record_settings_dialog.ui
    ui/desktop/select_screen_action.h
    ui/desktop/statistics_dialog.cc
    ui/desktop/statistics_dialog.h
    ui/desktop/statistics_dialog.ui
    ui/desktop/task_manager_settings.cc
    ui/desktop/task_manager_settings.h
    ui/desktop/task_manager_window.cc
    ui/desktop/task_manager_window.h
    ui/desktop/task_manager_window.ui)

list(APPEND SOURCE_CLIENT_CORE_UI_FILE_TRANSFER
    ui/file_transfer/address_bar.cc
    ui/file_transfer/address_bar.h
    ui/file_transfer/address_bar_model.cc
    ui/file_transfer/address_bar_model.h
    ui/file_transfer/file_error_code.cc
    ui/file_transfer/file_error_code.h
    ui/file_transfer/file_item_delegate.cc
    ui/file_transfer/file_item_delegate.h
    ui/file_transfer/file_list.cc
    ui/file_transfer/file_list.h
    ui/file_transfer/file_list_model.cc
    ui/file_transfer/file_list_model.h
    ui/file_transfer/file_manager_settings.cc
    ui/file_transfer/file_manager_settings.h
    ui/file_transfer/file_mime_data.cc
    ui/file_transfer/file_mime_data.h
    ui/file_transfer/file_name_validator.cc
    ui/file_transfer/file_name_validator.h
    ui/file_transfer/file_panel.cc
    ui/file_transfer/file_panel.h
    ui/file_transfer/file_panel.ui
    ui/file_transfer/file_remove_dialog.cc
    ui/file_transfer/file_remove_dialog.h
    ui/file_transfer/file_remove_dialog.ui
    ui/file_transfer/file_transfer_dialog.cc
    ui/file_transfer/file_transfer_dialog.h
    ui/file_transfer/file_transfer_dialog.ui
    ui/file_transfer/qt_file_manager_window.cc
    ui/file_transfer/qt_file_manager_window.h
    ui/file_transfer/qt_file_manager_window.ui)

list(APPEND SOURCE_CLIENT_CORE_UI_PORT_FORWARDING
    ui/port_forwarding/qt_port_forwarding_window.cc
    ui/port_forwarding/qt_port_forwarding_window.h
    ui/port_forwarding/qt_port_forwarding_window.ui)

list(APPEND SOURCE_CLIENT_CORE_UI_ROUTER_MANAGER
    ui/router_manager/router_manager_window.cc
    ui/router_manager/router_manager_window.h
    ui/router_manager/router_manager_window.ui
    ui/router_manager/router_user_dialog.cc
    ui/router_manager/router_user_dialog.h
    ui/router_manager/router_user_dialog.ui)

list(APPEND SOURCE_CLIENT_CORE_UI_SYS_INFO
    ui/sys_info/qt_system_info_window.cc
    ui/sys_info/qt_system_info_window.h
    ui/sys_info/qt_system_info_window.ui
    ui/sys_info/sys_info_widget.cc
    ui/sys_info/sys_info_widget.h
    ui/sys_info/sys_info_widget_applications.cc
    ui/sys_info/sys_info_widget_applications.h
    ui/sys_info/sys_info_widget_applications.ui
    ui/sys_info/sys_info_widget_connections.cc
    ui/sys_info/sys_info_widget_connections.h
    ui/sys_info/sys_info_widget_connections.ui
    ui/sys_info/sys_info_widget_devices.cc
    ui/sys_info/sys_info_widget_devices.h
    ui/sys_info/sys_info_widget_devices.ui
    ui/sys_info/sys_info_widget_drivers.cc
    ui/sys_info/sys_info_widget_drivers.h
    ui/sys_info/sys_info_widget_drivers.ui
    ui/sys_info/sys_info_widget_env_vars.cc
    ui/sys_info/sys_info_widget_env_vars.h
    ui/sys_info/sys_info_widget_env_vars.ui
    ui/sys_info/sys_info_widget_event_logs.cc
    ui/sys_info/sys_info_widget_event_logs.h
    ui/sys_info/sys_info_widget_event_logs.ui
    ui/sys_info/sys_info_widget_licenses.cc
    ui/sys_info/sys_info_widget_licenses.h
    ui/sys_info/sys_info_widget_licenses.ui
    ui/sys_info/sys_info_widget_local_users.cc
    ui/sys_info/sys_info_widget_local_users.h
    ui/sys_info/sys_info_widget_local_users.ui
    ui/sys_info/sys_info_widget_local_user_groups.cc
    ui/sys_info/sys_info_widget_local_user_groups.h
    ui/sys_info/sys_info_widget_local_user_groups.ui
    ui/sys_info/sys_info_widget_monitors.cc
    ui/sys_info/sys_info_widget_monitors.h
    ui/sys_info/sys_info_widget_monitors.ui
    ui/sys_info/sys_info_widget_net_shares.cc
    ui/sys_info/sys_info_widget_net_shares.h
    ui/sys_info/sys_info_widget_net_shares.ui
    ui/sys_info/sys_info_widget_net_adapters.cc
    ui/sys_info/sys_info_widget_net_adapters.h
    ui/sys_info/sys_info_widget_net_adapters.ui
    ui/sys_info/sys_info_widget_open_files.cc
    ui/sys_info/sys_info_widget_open_files.h
    ui/sys_info/sys_info_widget_open_files.ui
    ui/sys_info/sys_info_widget_power_options.cc
    ui/sys_info/sys_info_widget_power_options.h
    ui/sys_info/sys_info_widget_power_options.ui
    ui/sys_info/sys_info_widget_printers.cc
    ui/sys_info/sys_info_widget_printers.h
    ui/sys_info/sys_info_widget_printers.ui
    ui/sys_info/sys_info_widget_processes.cc
    ui/sys_info/sys_info_widget_processes.h
    ui/sys_info/sys_info_widget_processes.ui
    ui/sys_info/sys_info_widget_routes.cc
    ui/sys_info/sys_info_widget_routes.h
    ui/sys_info/sys_info_widget_routes.ui
    ui/sys_info/sys_info_widget_services.cc
    ui/sys_info/sys_info_widget_services.h
    ui/sys_info/sys_info_widget_services.ui
    ui/sys_info/sys_info_widget_summary.cc
    ui/sys_info/sys_info_widget_summary.h
    ui/sys_info/sys_info_widget_summary.ui
    ui/sys_info/sys_info_widget_video_adapters.cc
    ui/sys_info/sys_info_widget_video_adapters.h
    ui/sys_info/sys_info_widget_video_adapters.ui
    ui/sys_info/tree_to_html.cc
    ui/sys_info/tree_to_html.h)

list(APPEND SOURCE_CLIENT_CORE_UI_TEXT_CHAT
    ui/text_chat/qt_text_chat_window.cc
    ui/text_chat/qt_text_chat_window.h
    ui/text_chat/qt_text_chat_window.ui)

source_group("" FILES ${SOURCE_CLIENT_CORE})
source_group(resources FILES ${SOURCE_CLIENT_CORE_RESOURCES})
source_group(online_checker FILES ${SOURCE_CLIENT_CORE_ONLINE_CHECKER})
source_group(ui FILES ${SOURCE_CLIENT_CORE_UI})
source_group(ui\\desktop FILES ${SOURCE_CLIENT_CORE_UI_DESKTOP})
source_group(ui\\file_transfer FILES ${SOURCE_CLIENT_CORE_UI_FILE_TRANSFER})
source_group(ui\\port_forwarding FILES ${SOURCE_CLIENT_CORE_UI_PORT_FORWARDING})
source_group(ui\\router_manager FILES ${SOURCE_CLIENT_CORE_UI_ROUTER_MANAGER})
source_group(ui\\sys_info FILES ${SOURCE_CLIENT_CORE_UI_SYS_INFO})
source_group(ui\\text_chat FILES ${SOURCE_CLIENT_CORE_UI_TEXT_CHAT})

add_library(aspia_client_core STATIC
    ${SOURCE_CLIENT_CORE}
    ${SOURCE_CLIENT_CORE_RESOURCES}
    ${SOURCE_CLIENT_CORE_ONLINE_CHECKER}
    ${SOURCE_CLIENT_CORE_UI}
    ${SOURCE_CLIENT_CORE_UI_DESKTOP}
    ${SOURCE_CLIENT_CORE_UI_FILE_TRANSFER}
    ${SOURCE_CLIENT_CORE_UI_PORT_FORWARDING}
    ${SOURCE_CLIENT_CORE_UI_ROUTER_MANAGER}
    ${SOURCE_CLIENT_CORE_UI_SYS_INFO}
    ${SOURCE_CLIENT_CORE_UI_TEXT_CHAT})
target_link_libraries(aspia_client_core
    aspia_base
    aspia_common
    aspia_proto
    aspia_qt_base
    ${QT_COMMON_LIBS}
    ${QT_PLATFORM_LIBS}
    ${THIRD_PARTY_LIBS})
set_property(TARGET aspia_client_core PROPERTY AUTOMOC ON)
set_property(TARGET aspia_client_core PROPERTY AUTOUIC ON)
set_property(TARGET aspia_client_core PROPERTY AUTORCC ON)

if(Qt5LinguistTools_FOUND)
    # Get the list of translation files.
    file(GLOB CLIENT_CORE_TS_FILES translations/*.ts)

    add_translations(aspia_client_core QRC_FILE "translations/client_translations.qrc" TS_FILES ${CLIENT_CORE_TS_FILES})

    # Get the list of files containing messages.
    file(GLOB_RECURSE CLIENT_CORE_TR_SOURCES
        ${PROJECT_SOURCE_DIR}/source/client/*.cc
        ${PROJECT_SOURCE_DIR}/source/client/*.cpp
        ${PROJECT_SOURCE_DIR}/source/client/*.h
        ${PROJECT_SOURCE_DIR}/source/client/*.ui)

    create_translation(TS_CLIENT_CORE_UPDATE ${CLIENT_CORE_TR_SOURCES} ${CLIENT_CORE_TS_FILES})
    add_custom_target(aspia_client_ts_update DEPENDS ${TS_CLIENT_CORE_UPDATE})
endif()

if (WIN32)
    set(CLIENT_PLATFORM_LIBS
        avrt
        crypt32
        dbghelp
        dwmapi
        imm32
        iphlpapi
        netapi32
        shlwapi
        userenv
        uxtheme
        version
        winmm
        wtsapi32
        Qt5::WinMain)
endif()

if (LINUX)
    set(CLIENT_PLATFORM_LIBS stdc++fs ${XFIXES_LIB} ICU::uc ICU::dt)
endif()

if (APPLE)
    set(CLIENT_PLATFORM_LIBS ${COREAUDIO_LIB} ${AUDIOTOOLBOX_LIB} portaudio ICU::uc ICU::dt)
    set(aspia_client_ICON ${PROJECT_SOURCE_DIR}/source/client/resources/main.icns)
    set(aspia_client_PLIST ${PROJECT_SOURCE_DIR}/source/client/Info.plist)

    set_source_files_properties(${aspia_client_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
endif()

add_executable(aspia_client MACOSX_BUNDLE ${aspia_client_ICON} client_entry_point.cc client.rc)
target_link_libraries(aspia_client aspia_client_core ${CLIENT_PLATFORM_LIBS})
qt5_import_plugins(aspia_client
    INCLUDE ""
    EXCLUDE ""
    INCLUDE_BY_TYPE ""
    EXCLUDE_BY_TYPE imageformats)

if (WIN32)
    set_target_properties(aspia_client PROPERTIES WIN32_EXECUTABLE TRUE)
    set_target_properties(aspia_client PROPERTIES LINK_FLAGS "/MANIFEST:NO")
endif()

if (APPLE)
    set_target_properties(aspia_client PROPERTIES OUTPUT_NAME "Aspia Client")
    set_target_properties(aspia_client PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${aspia_client_PLIST})
    set_target_properties(aspia_client PROPERTIES MACOSX_BUNDLE_ICONFILE ${aspia_client_ICON})
endif()
