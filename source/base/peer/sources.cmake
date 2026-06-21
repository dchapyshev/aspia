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

collect_sources(SOURCE_BASE_PEER
    authenticator.cc
    authenticator.h
    client_authenticator.cc
    client_authenticator.h
    client_authenticator_legacy.cc
    client_authenticator_legacy.h
    host_id.cc
    host_id.h
    relay_peer.cc
    relay_peer.h
    relay_peer_manager.cc
    relay_peer_manager.h
    router_user.cc
    router_user.h
    server_authenticator.cc
    server_authenticator.h
    server_authenticator_legacy.cc
    server_authenticator_legacy.h
    stun_peer.cc
    stun_peer.h
    stun_server.cc
    stun_server.h
    user.cc
    user.h
    user_list.h)

collect_sources(SOURCE_BASE_PEER_TESTS
    host_id_unittest.cc
    router_user_unittest.cc
    user_unittest.cc)
