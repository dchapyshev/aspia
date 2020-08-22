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

list(APPEND SOURCE_ROUTER
    database.h
    database_factory.h
    database_factory_sqlite.cc
    database_factory_sqlite.h
    database_sqlite.cc
    database_sqlite.h
    main.cc
    server.cc
    server.h
    server_proxy.cc
    server_proxy.h
    session.cc
    session.h
    session_admin.cc
    session_admin.h
    session_client.cc
    session_client.h
    session_host.cc
    session_host.h
    session_relay.cc
    session_relay.h
    settings.cc
    settings.h
    shared_key_pool.cc
    shared_key_pool.h)

if (WIN32)
    list(APPEND SOURCE_ROUTER_WIN
        win/router.rc
        win/service.cc
        win/service.h
        win/service_constants.cc
        win/service_constants.h)
endif()

source_group("" FILES ${SOURCE_ROUTER})

if (WIN32)
    source_group(win FILES ${SOURCE_ROUTER_WIN})
endif()

add_executable(aspia_router ${SOURCE_ROUTER} ${SOURCE_ROUTER_WIN})
set_target_properties(aspia_router PROPERTIES LINK_FLAGS "/MANIFEST:NO")
target_link_libraries(aspia_router
    aspia_base
    aspia_proto
    comsupp
    crypt32
    libcrypto
    libssl
    modp_b64
    netapi32
    sqlite
    version
    ${Protobuf_LITE_LIBRARIES})

add_subdirectory(keygen)
add_subdirectory(manager)
