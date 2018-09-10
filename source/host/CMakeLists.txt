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
    ${PROJECT_SOURCE_DIR}/host/desktop_config_tracker.cc
    ${PROJECT_SOURCE_DIR}/host/desktop_config_tracker.h
    ${PROJECT_SOURCE_DIR}/host/host_config_main.cc
    ${PROJECT_SOURCE_DIR}/host/host_config_main.h
    ${PROJECT_SOURCE_DIR}/host/host_notifier.cc
    ${PROJECT_SOURCE_DIR}/host/host_notifier.h
    ${PROJECT_SOURCE_DIR}/host/host_server.cc
    ${PROJECT_SOURCE_DIR}/host/host_server.h
    ${PROJECT_SOURCE_DIR}/host/host_session.cc
    ${PROJECT_SOURCE_DIR}/host/host_session.h
    ${PROJECT_SOURCE_DIR}/host/host_session_desktop.cc
    ${PROJECT_SOURCE_DIR}/host/host_session_desktop.h
    ${PROJECT_SOURCE_DIR}/host/host_session_fake.cc
    ${PROJECT_SOURCE_DIR}/host/host_session_fake.h
    ${PROJECT_SOURCE_DIR}/host/host_session_fake_desktop.cc
    ${PROJECT_SOURCE_DIR}/host/host_session_fake_desktop.h
    ${PROJECT_SOURCE_DIR}/host/host_session_fake_file_transfer.cc
    ${PROJECT_SOURCE_DIR}/host/host_session_fake_file_transfer.h
    ${PROJECT_SOURCE_DIR}/host/host_session_file_transfer.cc
    ${PROJECT_SOURCE_DIR}/host/host_session_file_transfer.h
    ${PROJECT_SOURCE_DIR}/host/host_settings.cc
    ${PROJECT_SOURCE_DIR}/host/host_settings.h
    ${PROJECT_SOURCE_DIR}/host/input_injector.cc
    ${PROJECT_SOURCE_DIR}/host/input_injector.h
    ${PROJECT_SOURCE_DIR}/host/screen_updater.cc
    ${PROJECT_SOURCE_DIR}/host/screen_updater.h)

list(APPEND SOURCE_HOST_RESOURCES
    ${PROJECT_SOURCE_DIR}/host/resources/host.qrc)

list(APPEND SOURCE_HOST_UI
    ${PROJECT_SOURCE_DIR}/host/ui/host_config_dialog.cc
    ${PROJECT_SOURCE_DIR}/host/ui/host_config_dialog.h
    ${PROJECT_SOURCE_DIR}/host/ui/host_config_dialog.ui
    ${PROJECT_SOURCE_DIR}/host/ui/host_notifier_window.cc
    ${PROJECT_SOURCE_DIR}/host/ui/host_notifier_window.h
    ${PROJECT_SOURCE_DIR}/host/ui/host_notifier_window.ui
    ${PROJECT_SOURCE_DIR}/host/ui/user_dialog.cc
    ${PROJECT_SOURCE_DIR}/host/ui/user_dialog.h
    ${PROJECT_SOURCE_DIR}/host/ui/user_dialog.ui
    ${PROJECT_SOURCE_DIR}/host/ui/user_tree_item.cc
    ${PROJECT_SOURCE_DIR}/host/ui/user_tree_item.h)

list(APPEND SOURCE_HOST_WIN
    ${PROJECT_SOURCE_DIR}/host/win/host.cc
    ${PROJECT_SOURCE_DIR}/host/win/host.h
    ${PROJECT_SOURCE_DIR}/host/win/host_main.cc
    ${PROJECT_SOURCE_DIR}/host/win/host_main.h
    ${PROJECT_SOURCE_DIR}/host/win/host_process.cc
    ${PROJECT_SOURCE_DIR}/host/win/host_process.h
    ${PROJECT_SOURCE_DIR}/host/win/host_process_impl.cc
    ${PROJECT_SOURCE_DIR}/host/win/host_process_impl.h
    ${PROJECT_SOURCE_DIR}/host/win/host_service.cc
    ${PROJECT_SOURCE_DIR}/host/win/host_service.h
    ${PROJECT_SOURCE_DIR}/host/win/host_service_constants.cc
    ${PROJECT_SOURCE_DIR}/host/win/host_service_constants.h
    ${PROJECT_SOURCE_DIR}/host/win/host_service_main.cc
    ${PROJECT_SOURCE_DIR}/host/win/host_service_main.h)

source_group("" FILES ${SOURCE_HOST})
source_group(resources FILES ${SOURCE_HOST_RESOURCES})
source_group(ui FILES ${SOURCE_HOST_UI})
source_group(win FILES ${SOURCE_HOST_WIN})

add_library(aspia_host SHARED
    ${SOURCE_HOST}
    ${SOURCE_HOST_RESOURCES}
    ${SOURCE_HOST_UI}
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
    ${PROJECT_SOURCE_DIR}/host/host_config_entry_point.cc
    ${PROJECT_SOURCE_DIR}/host/host_config.rc)
set_target_properties(aspia_host_config PROPERTIES WIN32_EXECUTABLE TRUE)
set_target_properties(aspia_host_config PROPERTIES LINK_FLAGS "/MANIFEST:NO")
target_link_libraries(aspia_host_config aspia_host)

add_executable(aspia_host_service
    ${PROJECT_SOURCE_DIR}/host/win/host_service_entry_point.cc
    ${PROJECT_SOURCE_DIR}/host/win/host_service.rc)
target_link_libraries(aspia_host_service aspia_host)

add_executable(aspia_host_session
    ${PROJECT_SOURCE_DIR}/host/win/host_session_entry_point.cc
    ${PROJECT_SOURCE_DIR}/host/win/host_session.rc)
set_target_properties(aspia_host_session PROPERTIES WIN32_EXECUTABLE TRUE)
set_target_properties(aspia_host_session PROPERTIES LINK_FLAGS "/MANIFEST:NO")
target_link_libraries(aspia_host_session aspia_host)
