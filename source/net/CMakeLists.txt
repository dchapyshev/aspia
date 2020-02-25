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

list(APPEND SOURCE_NET
    adapter_enumerator.cc
    adapter_enumerator.h
    address.cc
    address.h
    firewall_manager.cc
    firewall_manager.h
    ip_util.cc
    ip_util.h
    network_channel.cc
    network_channel.h
    network_channel_proxy.cc
    network_channel_proxy.h
    network_error.cc
    network_error.h
    network_listener.h
    network_server.cc
    network_server.h)

list(APPEND SOURCE_NET_UNIT_TESTS
    address_unittest.cc)

source_group("" FILES ${SOURCE_NET})
source_group("" FILES ${SOURCE_NET_UNIT_TESTS})

add_library(aspia_net STATIC ${SOURCE_NET})
target_link_libraries(aspia_net aspia_base aspia_crypto ${THIRD_PARTY_LIBS})

# If the build of unit tests is enabled.
if (BUILD_UNIT_TESTS)
    add_executable(aspia_net_tests ${SOURCE_NET_UNIT_TESTS})
    target_link_libraries(aspia_net_tests
        aspia_net
        optimized gtest
        optimized gtest_main
        debug gtestd
        debug gtest_maind
        ${WINDOWS_LIBS}
        ${THIRD_PARTY_LIBS})

    add_test(NAME aspia_net_tests COMMAND aspia_net_tests)
endif()
