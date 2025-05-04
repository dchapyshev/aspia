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

list(APPEND SOURCE_HOST_CORE
    client_session.cc
    client_session.h
    client_session_desktop.cc
    client_session_desktop.h
    client_session_file_transfer.cc
    client_session_file_transfer.h
    client_session_port_forwarding.cc
    client_session_port_forwarding.h
    client_session_system_info.cc
    client_session_system_info.h
    client_session_text_chat.cc
    client_session_text_chat.h
    desktop_agent_main.cc
    desktop_agent_main.h
    desktop_session.cc
    desktop_session.h
    desktop_session_manager.cc
    desktop_session_manager.h
    desktop_session_agent.cc
    desktop_session_agent.h
    desktop_session_fake.cc
    desktop_session_fake.h
    desktop_session_ipc.cc
    desktop_session_ipc.h
    desktop_session_process.cc
    desktop_session_process.h
    desktop_session_proxy.cc
    desktop_session_proxy.h
    file_transfer_agent.cc
    file_transfer_agent.h
    file_transfer_agent_main.cc
    file_transfer_agent_main.h
    host_export.h
    host_ipc_storage.cc
    host_ipc_storage.h
    host_key_storage.cc
    host_key_storage.h
    input_injector.h
    integrity_check.cc
    integrity_check.h
    router_controller.cc
    router_controller.h
    server.cc
    server.h
    service.cc
    service.h
    service_constants.cc
    service_constants.h
    service_main.cc
    service_main.h
    stat_counter.cc
    stat_counter.h
    system_settings.cc
    system_settings.h
    unconfirmed_client_session.cc
    unconfirmed_client_session.h
    user_session.cc
    user_session.h
    user_session_agent.cc
    user_session_agent.h
    user_session_manager.cc
    user_session_manager.h)

if (WIN32)
    list(APPEND SOURCE_HOST_CORE
        host_core.rc
        input_injector_win.cc
        input_injector_win.h
        process_monitor.cc
        process_monitor.h
        system_info.cc
        system_info.h
        task_manager.cc
        task_manager.h)
endif()

if (LINUX)
    list(APPEND SOURCE_HOST_CORE
        input_injector_x11.cc
        input_injector_x11.h)
endif()

if (APPLE)
    list(APPEND SOURCE_HOST_CORE
        input_injector_mac.mm
        input_injector_mac.h)
endif()

list(APPEND SOURCE_HOST_CORE_UI
    ui/application.cc
    ui/application.h
    ui/change_password_dialog.cc
    ui/change_password_dialog.h
    ui/change_password_dialog.ui
    ui/check_password_dialog.cc
    ui/check_password_dialog.h
    ui/check_password_dialog.ui
    ui/config_dialog.cc
    ui/config_dialog.h
    ui/config_dialog.ui
    ui/connect_confirm_dialog.cc
    ui/connect_confirm_dialog.h
    ui/connect_confirm_dialog.ui
    ui/host_main.cc
    ui/host_main.h
    ui/main_window.cc
    ui/main_window.h
    ui/main_window.ui
    ui/notifier_window.cc
    ui/notifier_window.h
    ui/notifier_window.ui
    ui/resources.qrc
    ui/settings_util.cc
    ui/settings_util.h
    ui/user_dialog.cc
    ui/user_dialog.h
    ui/user_dialog.ui
    ui/user_settings.cc
    ui/user_settings.h
    ui/user_tree_item.cc
    ui/user_tree_item.h)

if (WIN32)
    list(APPEND SOURCE_HOST_CORE_WIN
        win/sas_injector.cc
        win/sas_injector.h
        win/touch_injector.cc
        win/touch_injector.h
        win/touch_injector_defines.h
        win/updater_launcher.cc
        win/updater_launcher.h)
endif()

source_group("" FILES ${SOURCE_HOST_CORE})
source_group(ui FILES ${SOURCE_HOST_CORE_UI})

if (WIN32)
    source_group(win FILES ${SOURCE_HOST_CORE_WIN})

    set(HOST_PLATFORM_LIBS
        crypt32
        dbghelp
        dxgi
        d3d11
        imm32
        iphlpapi
        netapi32
        setupapi
        userenv
        uxtheme
        version
        winmm
        wtsapi32)
endif()

add_library(aspia_host_core SHARED
    ${SOURCE_HOST_CORE}
    ${SOURCE_HOST_CORE_UI}
    ${SOURCE_HOST_CORE_WIN})
target_link_libraries(aspia_host_core PRIVATE
    aspia_base
    aspia_common
    aspia_proto
    ${HOST_PLATFORM_LIBS}
    ${QT_COMMON_LIBS}
    ${QT_PLATFORM_LIBS}
    ${THIRD_PARTY_LIBS})
set_target_properties(aspia_host_core PROPERTIES COMPILE_DEFINITIONS "HOST_IMPLEMENTATION")
set_property(TARGET aspia_host_core PROPERTY AUTOMOC ON)
set_property(TARGET aspia_host_core PROPERTY AUTOUIC ON)
set_property(TARGET aspia_host_core PROPERTY AUTORCC ON)
qt5_import_plugins(aspia_host_core
    INCLUDE ""
    EXCLUDE ""
    INCLUDE_BY_TYPE ""
    EXCLUDE_BY_TYPE imageformats)

if(Qt5LinguistTools_FOUND)
    # Get the list of translation files.
    file(GLOB HOST_TS_FILES ui/translations/*.ts)
    add_translations(aspia_host_core QRC_FILE "ui/translations.qrc" TS_FILES ${HOST_TS_FILES})

    # Get the list of files containing messages.
    file(GLOB_RECURSE HOST_TR_SOURCES
        ${PROJECT_SOURCE_DIR}/source/host/ui/*.cc
        ${PROJECT_SOURCE_DIR}/source/host/ui/*.cpp
        ${PROJECT_SOURCE_DIR}/source/host/ui/*.h
        ${PROJECT_SOURCE_DIR}/source/host/ui/*.ui)

    create_translation(TS_HOST_UPDATE ${HOST_TR_SOURCES} ${HOST_TS_FILES})
    add_custom_target(aspia_host_ts_update DEPENDS ${TS_HOST_UPDATE})
else()
    message(WARNING "Qt5 linguist tools not found. Internationalization support will be disabled.")
endif()

if (WIN32)
    set(HOST_PLATFORM_LIBS Qt5::WinMain)
endif()

if (LINUX)
    set(HOST_PLATFORM_LIBS
        stdc++fs
        ${XFIXES_LIB}
        ${XDAMAGE_LIB}
        ${XRANDR_LIB}
        ${XTST_LIB}
        ${XI_LIB}
        ${X11_LIB}
        ${XEXT_LIB})
endif()

# Aspia Host (GUI)
add_executable(aspia_host ui/host_entry_point.cc ui/host.rc)
if (WIN32)
    set_target_properties(aspia_host PROPERTIES WIN32_EXECUTABLE TRUE)
    set_target_properties(aspia_host PROPERTIES LINK_FLAGS "/MANIFEST:NO")
endif()
target_link_libraries(aspia_host aspia_host_core ${HOST_PLATFORM_LIBS})

# Aspia Host Service
add_executable(aspia_host_service service_entry_point.cc service.rc)
if (WIN32)
    set_target_properties(aspia_host_service PROPERTIES LINK_FLAGS "/MANIFEST:NO")
endif()
target_link_libraries(aspia_host_service aspia_host_core ${HOST_PLATFORM_LIBS})

# Aspia Desktop Agent
add_executable(aspia_desktop_agent WIN32 desktop_agent_entry_point.cc desktop_agent.rc)
if (WIN32)
    set_target_properties(aspia_desktop_agent PROPERTIES LINK_FLAGS "/MANIFEST:NO")
endif()
target_link_libraries(aspia_desktop_agent aspia_host_core ${HOST_PLATFORM_LIBS})

# Aspia File Transfer Agent
add_executable(aspia_file_transfer_agent WIN32 file_transfer_agent_entry_point.cc file_transfer_agent.rc)
if (WIN32)
    set_target_properties(aspia_file_transfer_agent PROPERTIES LINK_FLAGS "/MANIFEST:NO")
endif()
target_link_libraries(aspia_file_transfer_agent aspia_host_core ${HOST_PLATFORM_LIBS})
