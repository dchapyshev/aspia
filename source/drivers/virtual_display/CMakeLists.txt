#
# SmartCafe Project
# Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

list(APPEND SOURCE_VIRTUAL_DISPLAY
    driver.cpp
    driver.h
    driver.rc)

source_group("" FILES ${SOURCE_VIRTUAL_DISPLAY})

wdk_add_usermode_driver(aspia_virtual_display
    UMDF 2.27
    WINVER 0x0A00            # Windows 10
    NTDDI_VERSION 0x0A000004 # Windows 10 1709
    ${SOURCE_VIRTUAL_DISPLAY})

target_include_directories(aspia_virtual_display PRIVATE ${CMAKE_SOURCE_DIR})
