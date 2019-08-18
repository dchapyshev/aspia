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

list(APPEND SOURCE_PROXY
    proxy_main.cc
    proxy_server.cc
    proxy_server.h)

list(APPEND SOURCE_PROXY_WIN
    win/proxy_service.cc
    win/proxy_service.h
    win/proxy_service_constants.cc
    win/proxy_service_constants.h)

source_group("" FILES ${SOURCE_PROXY})
source_group(win FILES ${SOURCE_PROXY_WIN})

add_executable(aspia_proxy ${SOURCE_PROXY} ${SOURCE_PROXY_WIN} win/proxy.rc)
set_target_properties(aspia_proxy PROPERTIES LINK_FLAGS "/MANIFEST:NO")
target_link_libraries(aspia_proxy
    aspia_base
    aspia_crypto
    aspia_net
    aspia_proto
    ${THIRD_PARTY_LIBS})

if (USE_TBB)
    add_tbb(aspia_proxy ${ASPIA_THIRD_PARTY_DIR}/tbb)
endif()
