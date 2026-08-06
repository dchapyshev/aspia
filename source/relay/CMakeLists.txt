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

list(APPEND SOURCE_RELAY_CORE
    migration_utils.cc
    migration_utils.h
    pending_session.cc
    pending_session.h
    service.cc
    service.h
    session.cc
    session.h
    session_key.cc
    session_key.h
    settings.cc
    settings.h
    shared_key_pool.cc
    shared_key_pool.h)

list(APPEND SOURCE_RELAY
    main.cc)

list(APPEND SOURCE_RELAY_TESTS
    pending_session_unittest.cc
    relay_test_worker.h
    session_key_unittest.cc
    session_unittest.cc
    shared_key_pool_unittest.cc
    tests_main.cc)

include(workers/sources.cmake)

if (WIN32)
    list(APPEND SOURCE_RELAY_WIN win/relay.rc)
endif()

source_group("" FILES ${SOURCE_RELAY_CORE} ${SOURCE_RELAY} ${SOURCE_RELAY_TESTS})
source_group(workers FILES ${SOURCE_RELAY_WORKERS} ${SOURCE_RELAY_WORKERS_TESTS})

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

add_library(aspia_relay_core STATIC ${SOURCE_RELAY_CORE} ${SOURCE_RELAY_WORKERS})

target_link_libraries(aspia_relay_core
    aspia_base
    aspia_proto
    OpenSSL::Crypto
    ${Protobuf_LITE_LIBRARIES}
    ${QT_COMMON_LIBS}
    ${QT_PLATFORM_LIBS}
    ${RELAY_PLATFORM_LIBS})

set_property(TARGET aspia_relay_core PROPERTY AUTOMOC ON)
set_property(TARGET aspia_relay_core PROPERTY AUTOUIC ON)
set_property(TARGET aspia_relay_core PROPERTY AUTORCC ON)

add_executable(aspia_relay ${SOURCE_RELAY} ${SOURCE_RELAY_WIN})

if (WIN32)
    set_target_properties(aspia_relay PROPERTIES LINK_FLAGS "/MANIFEST:NO")
endif()

target_link_libraries(aspia_relay
    aspia_relay_core)

set_property(TARGET aspia_relay PROPERTY AUTOMOC ON)
set_property(TARGET aspia_relay PROPERTY AUTOUIC ON)
set_property(TARGET aspia_relay PROPERTY AUTORCC ON)

add_executable(aspia_relay_tests ${SOURCE_RELAY_TESTS} ${SOURCE_RELAY_WORKERS_TESTS})

target_link_libraries(aspia_relay_tests
    aspia_relay_core
    GTest::gtest
    ${THIRD_PARTY_LIBS})

set_property(TARGET aspia_relay_tests PROPERTY AUTOMOC ON)
set_property(TARGET aspia_relay_tests PROPERTY AUTOUIC ON)
set_property(TARGET aspia_relay_tests PROPERTY AUTORCC ON)
qt_import_plugins(aspia_relay_tests INCLUDE "" EXCLUDE "" INCLUDE_BY_TYPE "" EXCLUDE_BY_TYPE imageformats)

add_test(NAME aspia_relay_tests COMMAND aspia_relay_tests)
