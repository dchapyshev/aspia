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

collect_sources(SOURCE_BASE_NET
    anti_replay_window.cc
    anti_replay_window.h
    address.cc
    address.h
    curl_util.cc
    curl_util.h
    flood_guard.cc
    flood_guard.h
    gateway_port_mapper.cc
    gateway_port_mapper.h
    net_utils.cc
    net_utils.h
    pcp_port_mapper.cc
    pcp_port_mapper.h
    tcp_channel.cc
    tcp_channel.h
    tcp_channel_legacy.cc
    tcp_channel_legacy.h
    tcp_channel_ng.cc
    tcp_channel_ng.h
    tcp_server.cc
    tcp_server.h
    tcp_server_legacy.cc
    tcp_server_legacy.h
    udp_channel.cc
    udp_channel.h
    upnp_port_mapper.cc
    upnp_port_mapper.h)

if (WIN32)
    collect_sources(SOURCE_BASE_NET
        connect_enumerator.cc
        connect_enumerator.h
        firewall_manager.cc
        firewall_manager.h
        net_utils_win.cc
        open_files_enumerator.cc
        open_files_enumerator.h)
endif()

if (LINUX)
    collect_sources(SOURCE_BASE_NET
        net_utils_linux.cc)
endif()

if (APPLE)
    collect_sources(SOURCE_BASE_NET
        net_utils_mac.cc)
endif()

collect_sources(SOURCE_BASE_NET_TESTS
    address_unittest.cc
    anti_replay_window_unittest.cc
    flood_guard_unittest.cc)
