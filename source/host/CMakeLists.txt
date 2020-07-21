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
    host_export.h
    host_main.cc
    host_main.h
    input_injector.h
    input_injector_win.cc
    input_injector_win.h
    sas_injector.cc
    sas_injector.h
    server.cc
    server.h
    system_info.cc
    system_info.h
    system_settings.cc
    system_settings.h
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

list(APPEND SOURCE_HOST_CORE_UI
    ui/application.cc
    ui/application.h
    ui/config_dialog.cc
    ui/config_dialog.h
    ui/config_dialog.ui
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

list(APPEND SOURCE_HOST_CORE_WIN
    win/dll_main.cc
    win/service.cc
    win/service.h
    win/service_constants.cc
    win/service_constants.h
    win/service_main.cc
    win/service_main.h
    win/updater_launcher.cc
    win/updater_launcher.h)

source_group("" FILES ${SOURCE_HOST_CORE})
source_group(ui FILES ${SOURCE_HOST_CORE_UI})
source_group(win FILES ${SOURCE_HOST_CORE_WIN})

add_library(aspia_host_core SHARED
    ${SOURCE_HOST_CORE}
    ${SOURCE_HOST_CORE_UI}
    ${SOURCE_HOST_CORE_WIN})
target_link_libraries(aspia_host_core
    aspia_base
    aspia_codec
    aspia_common
    aspia_desktop
    aspia_net
    aspia_proto
    aspia_qt_base
    aspia_updater
    ${QT_LIBS}
    ${WINDOWS_LIBS}
    ${THIRD_PARTY_LIBS})
set_target_properties(aspia_host_core PROPERTIES COMPILE_DEFINITIONS "HOST_IMPLEMENTATION")
set_property(TARGET aspia_host_core PROPERTY AUTOMOC ON)
set_property(TARGET aspia_host_core PROPERTY AUTOUIC ON)
set_property(TARGET aspia_host_core PROPERTY AUTORCC ON)

if (USE_TBB)
    add_tbb(aspia_host_core ${ASPIA_THIRD_PARTY_DIR}/tbb)
endif()

if(Qt5LinguistTools_FOUND)
    # Get the list of translation files.
    file(GLOB HOST_TS_FILES ui/translations/*.ts)
    add_translations(aspia_host_core QRC_FILE "ui/translations.qrc" TS_FILES ${HOST_TS_FILES})

    # Get the list of files containing messages.
    file(GLOB_RECURSE HOST_TR_SOURCES
        ${PROJECT_SOURCE_DIR}/host/ui/*.cc
        ${PROJECT_SOURCE_DIR}/host/ui/*.cpp
        ${PROJECT_SOURCE_DIR}/host/ui/*.h
        ${PROJECT_SOURCE_DIR}/host/ui/*.ui)

    create_translation(TS_HOST_UPDATE ${HOST_TR_SOURCES} ${HOST_TS_FILES})
    add_custom_target(aspia_host_ts_update DEPENDS ${TS_HOST_UPDATE})
else()
    message(WARNING "Qt5 linguist tools not found. Internationalization support will be disabled.")
endif()

add_executable(aspia_host host_entry_point.cc host.rc)
set_target_properties(aspia_host PROPERTIES WIN32_EXECUTABLE TRUE)
set_target_properties(aspia_host PROPERTIES LINK_FLAGS "/MANIFEST:NO")
target_link_libraries(aspia_host aspia_host_core)

add_executable(aspia_host_service win/service_entry_point.cc win/service.rc)
set_target_properties(aspia_host_service PROPERTIES LINK_FLAGS "/MANIFEST:NO")
target_link_libraries(aspia_host_service aspia_host_core)

add_executable(aspia_desktop_agent WIN32 desktop_agent_entry_point.cc desktop_agent.rc)
set_target_properties(aspia_desktop_agent PROPERTIES LINK_FLAGS "/MANIFEST:NO")
target_link_libraries(aspia_desktop_agent aspia_host_core)

if(NOT TARGET BUILD_HOST)
    add_custom_target(BUILD_HOST)
    add_dependencies(BUILD_HOST aspia_host_core aspia_host aspia_host_service aspia_desktop_agent)
endif()
