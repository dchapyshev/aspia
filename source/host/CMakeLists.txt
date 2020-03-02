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

list(APPEND SOURCE_HOST_CORE
    client_session.cc
    client_session.h
    client_session_desktop.cc
    client_session_desktop.h
    client_session_file_transfer.cc
    client_session_file_transfer.h
    clipboard_monitor.cc
    clipboard_monitor.h
    desktop_agent_main.cc
    desktop_agent_main.h
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
    host_authenticator.cc
    host_authenticator.h
    host_authenticator_manager.cc
    host_authenticator_manager.h
    host_export.h
    host_main.cc
    host_main.h
    host_server.cc
    host_server.h
    host_system_info.cc
    host_system_info.h
    input_injector.h
    input_injector_win.cc
    input_injector_win.h
    sas_injector.cc
    sas_injector.h
    system_settings.cc
    system_settings.h
    user.cc
    user.h
    user_session.cc
    user_session.h
    user_session_agent.cc
    user_session_agent.h
    user_session_agent_proxy.cc
    user_session_agent_proxy.h
    user_session_constants.cc
    user_session_constants.h
    user_session_manager.cc
    user_session_manager.h
    user_session_window.h
    user_session_window_proxy.cc
    user_session_window_proxy.h)

list(APPEND SOURCE_HOST_CORE_RESOURCES
    resources/host.qrc)

list(APPEND SOURCE_HOST_CORE_UI
    ui/host_application.cc
    ui/host_application.h
    ui/host_config_dialog.cc
    ui/host_config_dialog.h
    ui/host_config_dialog.ui
    ui/host_main_window.cc
    ui/host_main_window.h
    ui/host_main_window.ui
    ui/host_notifier_window.cc
    ui/host_notifier_window.h
    ui/host_notifier_window.ui
    ui/settings_util.cc
    ui/settings_util.h
    ui/user_dialog.cc
    ui/user_dialog.h
    ui/user_dialog.ui
    ui/user_settings.cc
    ui/user_settings.h
    ui/user_tree_item.cc
    ui/user_tree_item.h)

list(APPEND SOURCE_HOST_CORE_WIN
    win/dll_main.cc
    win/host_service.cc
    win/host_service.h
    win/host_service_constants.cc
    win/host_service_constants.h
    win/host_service_main.cc
    win/host_service_main.h
    win/updater_launcher.cc
    win/updater_launcher.h)

source_group("" FILES ${SOURCE_HOST_CORE})
source_group(moc FILES ${SOURCE_HOST_CORE_MOC})
source_group(resources FILES ${SOURCE_HOST_CORE_RESOURCES})
source_group(ui FILES ${SOURCE_HOST_CORE_UI})
source_group(win FILES ${SOURCE_HOST_CORE_WIN})

add_library(aspia_host_core SHARED
    ${SOURCE_HOST_CORE}
    ${SOURCE_HOST_CORE_RESOURCES}
    ${SOURCE_HOST_CORE_UI}
    ${SOURCE_HOST_CORE_WIN})
target_link_libraries(aspia_host_core
    aspia_base
    aspia_codec
    aspia_common
    aspia_crypto
    aspia_desktop
    aspia_ipc
    aspia_net
    aspia_proto
    aspia_qt_base
    aspia_updater
    ${QT_LIBS}
    ${WINDOWS_LIBS}
    ${THIRD_PARTY_LIBS})
set_target_properties(aspia_host_core PROPERTIES COMPILE_DEFINITIONS "HOST_IMPLEMENTATION")

if (USE_TBB)
    add_tbb(aspia_host_core ${ASPIA_THIRD_PARTY_DIR}/tbb)
endif()

if(Qt5LinguistTools_FOUND)
    # Get the list of translation files.
    file(GLOB HOST_TS_FILES translations/*.ts)
    add_translations(aspia_host_core QRC_FILE "translations/host_translations.qrc" TS_FILES ${HOST_TS_FILES})

    # Get the list of files containing messages.
    file(GLOB_RECURSE HOST_TR_SOURCES
        ${PROJECT_SOURCE_DIR}/host/*.cc
        ${PROJECT_SOURCE_DIR}/host/*.cpp
        ${PROJECT_SOURCE_DIR}/host/*.h
        ${PROJECT_SOURCE_DIR}/host/*.ui)

    create_translation(TS_HOST_UPDATE ${HOST_TR_SOURCES} ${HOST_TS_FILES})
    add_custom_target(aspia_host_ts_update DEPENDS ${TS_HOST_UPDATE})
else()
    message(WARNING "Qt5 linguist tools not found. Internationalization support will be disabled.")
endif()

add_executable(aspia_host host_entry_point.cc host.rc)
set_target_properties(aspia_host PROPERTIES WIN32_EXECUTABLE TRUE)
set_target_properties(aspia_host PROPERTIES LINK_FLAGS "/MANIFEST:NO")
target_link_libraries(aspia_host aspia_host_core)

add_executable(aspia_host_service win/host_service_entry_point.cc win/host_service.rc)
set_target_properties(aspia_host_service PROPERTIES LINK_FLAGS "/MANIFEST:NO")
target_link_libraries(aspia_host_service aspia_host_core)

add_executable(aspia_desktop_agent desktop_agent_entry_point.cc desktop_agent.rc)
set_target_properties(aspia_desktop_agent PROPERTIES WIN32_EXECUTABLE TRUE)
set_target_properties(aspia_desktop_agent PROPERTIES LINK_FLAGS "/MANIFEST:NO")
target_link_libraries(aspia_desktop_agent aspia_host_core)

if(NOT TARGET BUILD_HOST)
    add_custom_target(BUILD_HOST)
    add_dependencies(BUILD_HOST aspia_host_core aspia_host aspia_host_service aspia_desktop_agent)
endif()
