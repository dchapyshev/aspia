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

if (WIN32)
    set(SIGNER_PLATFORM_LIBS avrt crypt32 dbghelp iphlpapi ws2_32)
endif()

if (LINUX)
    set(SIGNER_PLATFORM_LIBS stdc++fs)
endif()

if (APPLE)
    set(SIGNER_PLATFORM_LIBS ${FOUNDATION_LIB})
endif()

add_executable(aspia_signer main.cc)
target_link_libraries(aspia_signer PRIVATE
    aspia_base
    aspia_proto
    ${SIGNER_PLATFORM_LIBS}
    ${THIRD_PARTY_LIBS}
    ${QT_COMMON_LIBS}
    ${QT_PLATFORM_LIBS})
