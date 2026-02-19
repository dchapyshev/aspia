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

list(APPEND SOURCE_RELAY
    key_factory.cc
    key_factory.h
    key_pool.cc
    key_pool.h
    main.cc
    migration_utils.cc
    migration_utils.h
    pending_session.cc
    pending_session.h
    service.cc
    service.h
    service_constants.cc
    service_constants.h
    session.cc
    session.h
    session_key.cc
    session_key.h
    session_manager.cc
    session_manager.h
    sessions_worker.cc
    sessions_worker.h
    settings.cc
    settings.h)

if (WIN32)
    list(APPEND SOURCE_RELAY_WIN
        win/relay.rc
        win/service_util.cc
        win/service_util.h)
endif()

source_group("" FILES ${SOURCE_RELAY})

if (WIN32)
    source_group(win FILES ${SOURCE_RELAY_WIN})
    set(RELAY_PLATFORM_LIBS avrt comsupp crypt32 dbghelp netapi32 version)
endif()

if (LINUX)
    set(RELAY_PLATFORM_LIBS stdc++fs)
endif()

if (APPLE)
    set(RELAY_PLATFORM_LIBS ${FOUNDATION_LIB})
endif()

add_executable(aspia_relay ${SOURCE_RELAY} ${SOURCE_RELAY_WIN})

if (WIN32)
    set_target_properties(aspia_relay PROPERTIES LINK_FLAGS "/MANIFEST:NO")
endif()

target_link_libraries(aspia_relay
    aspia_base
    aspia_proto
    OpenSSL::Crypto
    ${Protobuf_LITE_LIBRARIES}
    ${QT_COMMON_LIBS}
    ${QT_PLATFORM_LIBS}
    ${RELAY_PLATFORM_LIBS})

set_property(TARGET aspia_relay PROPERTY AUTOMOC ON)
set_property(TARGET aspia_relay PROPERTY AUTOUIC ON)
set_property(TARGET aspia_relay PROPERTY AUTORCC ON)
