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

# The implementation lives in a static library so both the router binary and the unit tests
# link the same objects; the executable itself is only the entry point.
list(APPEND SOURCE_ROUTER_CORE
    client.cc
    client.h
    client_admin.cc
    client_admin.h
    client_manager.cc
    client_manager.h
    database.cc
    database.h
    group_request_handler.cc
    group_request_handler.h
    host.cc
    host.h
    host_legacy.cc
    host_legacy.h
    host_ng.cc
    host_ng.h
    host_request_handler.cc
    host_request_handler.h
    migration_utils.cc
    migration_utils.h
    relay.cc
    relay.h
    request_caller.h
    router_user_list.cc
    router_user_list.h
    service.cc
    service.h
    settings.cc
    settings.h
    shared_hosts.cc
    shared_hosts.h
    shared_key_pool.cc
    shared_key_pool.h
    user_request_handler.cc
    user_request_handler.h
    workspace.cc
    workspace.h
    workspace_request_handler.cc
    workspace_request_handler.h)

list(APPEND SOURCE_ROUTER
    main.cc)

list(APPEND SOURCE_ROUTER_TESTS
    database_unittest.cc
    group_request_handler_unittest.cc
    host_request_handler_unittest.cc
    router_test_base.h
    tests_main.cc
    user_request_handler_unittest.cc
    workspace_request_handler_unittest.cc)

include(workers/sources.cmake)

if (WIN32)
    list(APPEND SOURCE_ROUTER_WIN win/router.rc)
endif()

source_group("" FILES ${SOURCE_ROUTER_CORE} ${SOURCE_ROUTER} ${SOURCE_ROUTER_TESTS})
source_group(workers FILES ${SOURCE_ROUTER_WORKERS})

if (WIN32)
    source_group(win FILES ${SOURCE_ROUTER_WIN})
    set(ROUTER_PLATFORM_LIBS avrt comsupp crypt32 dbghelp netapi32 version)
endif()

if (LINUX)
    set(ROUTER_PLATFORM_LIBS stdc++fs)
endif()

if (APPLE)
    set(ROUTER_PLATFORM_LIBS ${FOUNDATION_LIB})
endif()

add_library(aspia_router_core STATIC ${SOURCE_ROUTER_CORE} ${SOURCE_ROUTER_WORKERS})

target_link_libraries(aspia_router_core
    aspia_base
    aspia_proto
    OpenSSL::Crypto
    ${Protobuf_LITE_LIBRARIES}
    ${QT_COMMON_LIBS}
    ${QT_PLATFORM_LIBS}
    ${ROUTER_PLATFORM_LIBS})

set_property(TARGET aspia_router_core PROPERTY AUTOMOC ON)
set_property(TARGET aspia_router_core PROPERTY AUTOUIC ON)
set_property(TARGET aspia_router_core PROPERTY AUTORCC ON)

add_executable(aspia_router ${SOURCE_ROUTER} ${SOURCE_ROUTER_WIN})

if (WIN32)
    set_target_properties(aspia_router PROPERTIES LINK_FLAGS "/MANIFEST:NO")
endif()

target_link_libraries(aspia_router
    aspia_router_core)

set_property(TARGET aspia_router PROPERTY AUTOMOC ON)
set_property(TARGET aspia_router PROPERTY AUTOUIC ON)
set_property(TARGET aspia_router PROPERTY AUTORCC ON)

add_executable(aspia_router_tests ${SOURCE_ROUTER_TESTS})

target_link_libraries(aspia_router_tests
    aspia_router_core
    GTest::gtest
    ${THIRD_PARTY_LIBS})

set_property(TARGET aspia_router_tests PROPERTY AUTOMOC ON)
set_property(TARGET aspia_router_tests PROPERTY AUTOUIC ON)
set_property(TARGET aspia_router_tests PROPERTY AUTORCC ON)
qt_import_plugins(aspia_router_tests INCLUDE "" EXCLUDE "" INCLUDE_BY_TYPE "" EXCLUDE_BY_TYPE imageformats)

add_test(NAME aspia_router_tests COMMAND aspia_router_tests)
