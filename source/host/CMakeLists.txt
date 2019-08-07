#
# Aspia Project
# Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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
    host_export.h
    host_server.cc
    host_server.h
    host_settings.cc
    host_settings.h
    user.cc
    user.h)

list(APPEND SOURCE_HOST_CORE_WIN
    win/dll_main.cc
    win/host_service.cc
    win/host_service.h
    win/host_service_constants.cc
    win/host_service_constants.h
    win/host_service_main.cc
    win/host_service_main.h)

source_group("" FILES ${SOURCE_HOST_CORE})
source_group(win FILES ${SOURCE_HOST_CORE_WIN})

add_library(aspia_host_core SHARED
    ${SOURCE_HOST_CORE}
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
    ${THIRD_PARTY_LIBS})
set_target_properties(aspia_host_core PROPERTIES COMPILE_DEFINITIONS "HOST_IMPLEMENTATION")

if (USE_TBB)
    add_tbb(aspia_host_core ${ASPIA_THIRD_PARTY_DIR}/tbb)
endif()

