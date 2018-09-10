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

list(APPEND SOURCE_HOST
    desktop_config_tracker.cc
    desktop_config_tracker.h
    host.rc
    host_config_main.cc
    host_config_main.h
    host_export.h
    host_notifier.cc
    host_notifier.h
    host_server.cc
    host_server.h
    host_session.cc
    host_session.h
    host_session_desktop.cc
    host_session_desktop.h
    host_session_fake.cc
    host_session_fake.h
    host_session_fake_desktop.cc
    host_session_fake_desktop.h
    host_session_fake_file_transfer.cc
    host_session_fake_file_transfer.h
    host_session_file_transfer.cc
    host_session_file_transfer.h
    host_settings.cc
    host_settings.h
    input_injector.cc
    input_injector.h
    screen_updater.cc
    screen_updater.h)

list(APPEND SOURCE_HOST_RESOURCES
    resources/host.qrc)

list(APPEND SOURCE_HOST_UI
    ui/host_config_dialog.cc
    ui/host_config_dialog.h
    ui/host_config_dialog.ui
    ui/host_notifier_window.cc
    ui/host_notifier_window.h
    ui/host_notifier_window.ui
    ui/user_dialog.cc
    ui/user_dialog.h
    ui/user_dialog.ui
    ui/user_tree_item.cc
    ui/user_tree_item.h)

list(APPEND SOURCE_HOST_WIN
    win/host.cc
    win/host.h
    win/host_main.cc
    win/host_main.h
    win/host_process.cc
    win/host_process.h
    win/host_process_impl.cc
    win/host_process_impl.h
    win/host_service.cc
    win/host_service.h
    win/host_service_constants.cc
    win/host_service_constants.h
    win/host_service_main.cc
    win/host_service_main.h)

file(GLOB_RECURSE HOST_HEADERS ${PROJECT_SOURCE_DIR}/host/*.h)
qt5_wrap_cpp(SOURCE_HOST_MOC ${HOST_HEADERS} OPTIONS "--no-notes")

file(GLOB_RECURSE HOST_UI_FILES ${PROJECT_SOURCE_DIR}/host/ui/*.ui)
qt5_wrap_ui(SOURCE_HOST_UIC ${HOST_UI_FILES})

file(GLOB_RECURSE HOST_QRC_FILES ${PROJECT_SOURCE_DIR}/host/resources/*.qrc)
qt5_add_resources(SOURCE_HOST_RCC ${HOST_QRC_FILES})

source_group("" FILES ${SOURCE_HOST})
source_group(moc FILES ${SOURCE_HOST_MOC})
source_group(resources FILES ${SOURCE_HOST_RESOURCES})
source_group(resources FILES ${SOURCE_HOST_RCC})
source_group(ui FILES ${SOURCE_HOST_UI})
source_group(uic FILES ${SOURCE_HOST_UIC})
source_group(win FILES ${SOURCE_HOST_WIN})

add_library(aspia_host SHARED
    ${SOURCE_HOST}
    ${SOURCE_HOST_MOC}
    ${SOURCE_HOST_RESOURCES}
    ${SOURCE_HOST_RCC}
    ${SOURCE_HOST_UI}
    ${SOURCE_HOST_UIC}
    ${SOURCE_HOST_WIN})
target_link_libraries(aspia_host
    aspia_base
    aspia_codec
    aspia_crypto
    aspia_desktop_capture
    aspia_ipc
    aspia_network
    aspia_protocol
    aspia_share
    ${THIRD_PARTY_LIBS})

add_executable(aspia_host_config
    host_config_entry_point.cc
    host_config.rc)
set_target_properties(aspia_host_config PROPERTIES WIN32_EXECUTABLE TRUE)
set_target_properties(aspia_host_config PROPERTIES LINK_FLAGS "/MANIFEST:NO")
target_link_libraries(aspia_host_config aspia_host)

add_executable(aspia_host_service
    win/host_service_entry_point.cc
    win/host_service.rc)
target_link_libraries(aspia_host_service aspia_host)

add_executable(aspia_host_session
    win/host_session_entry_point.cc
    win/host_session.rc)
set_target_properties(aspia_host_session PROPERTIES WIN32_EXECUTABLE TRUE)
set_target_properties(aspia_host_session PROPERTIES LINK_FLAGS "/MANIFEST:NO")
target_link_libraries(aspia_host_session aspia_host)
