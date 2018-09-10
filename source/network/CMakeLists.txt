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

list(APPEND SOURCE_NETWORK
    ${PROJECT_SOURCE_DIR}/network/firewall_manager.cc
    ${PROJECT_SOURCE_DIR}/network/firewall_manager.h
    ${PROJECT_SOURCE_DIR}/network/network_channel.cc
    ${PROJECT_SOURCE_DIR}/network/network_channel.h
    ${PROJECT_SOURCE_DIR}/network/network_channel_client.cc
    ${PROJECT_SOURCE_DIR}/network/network_channel_client.h
    ${PROJECT_SOURCE_DIR}/network/network_channel_host.cc
    ${PROJECT_SOURCE_DIR}/network/network_channel_host.h
    ${PROJECT_SOURCE_DIR}/network/network_server.cc
    ${PROJECT_SOURCE_DIR}/network/network_server.h
    ${PROJECT_SOURCE_DIR}/network/srp_client_context.cc
    ${PROJECT_SOURCE_DIR}/network/srp_client_context.h
    ${PROJECT_SOURCE_DIR}/network/srp_host_context.cc
    ${PROJECT_SOURCE_DIR}/network/srp_host_context.h)

source_group("" FILES ${SOURCE_NETWORK})

add_library(aspia_network STATIC ${SOURCE_NETWORK})
target_link_libraries(aspia_network
    aspia_base
    aspia_crypto
    ${THIRD_PARTY_LIBS})
