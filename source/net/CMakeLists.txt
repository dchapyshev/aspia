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

list(APPEND SOURCE_NET
    firewall_manager.cc
    firewall_manager.h
    ip_util.cc
    ip_util.h
    network_adapter_enumerator.cc
    network_adapter_enumerator.h
    network_channel.cc
    network_channel.h
    network_channel_client.cc
    network_channel_client.h
    network_channel_host.cc
    network_channel_host.h
    network_server.cc
    network_server.h
    srp_client_context.cc
    srp_client_context.h
    srp_host_context.cc
    srp_host_context.h
    srp_user.h)

source_group("" FILES ${SOURCE_NET})

add_library(aspia_net STATIC ${SOURCE_NET})
target_link_libraries(aspia_net aspia_base aspia_crypto ${THIRD_PARTY_LIBS})
