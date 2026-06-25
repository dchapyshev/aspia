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

collect_sources(SOURCE_BASE_DESKTOP
    capture_scheduler.cc
    capture_scheduler.h
    desktop_environment.cc
    desktop_environment.h
    desktop_resizer.cc
    desktop_resizer.h
    diff_block_32bpp_c.cc
    diff_block_32bpp_c.h
    diff_block_32bpp_sse2.cc
    diff_block_32bpp_sse2.h
    differ.cc
    differ.h
    frame.cc
    frame.h
    frame_aligned.cc
    frame_aligned.h
    frame_rotation.cc
    frame_rotation.h
    mouse_cursor.cc
    mouse_cursor.h
    power_save_blocker.cc
    power_save_blocker.h
    region.cc
    region.h
    screen_capturer.cc
    screen_capturer.h
    screen_capturer_helper.cc
    screen_capturer_helper.h
    shared_frame.cc
    shared_frame.h)

if (WIN32)
    collect_sources(SOURCE_BASE_DESKTOP
        desktop_environment_win.cc
        desktop_environment_win.h
        desktop_resizer_win.cc
        desktop_resizer_win.h
        frame_dib.cc
        frame_dib.h
        screen_capturer_dxgi.cc
        screen_capturer_dxgi.h
        screen_capturer_gdi.cc
        screen_capturer_gdi.h
        screen_capturer_win.cc
        screen_capturer_win.h)
endif()

if (LINUX)
    collect_sources(SOURCE_BASE_DESKTOP
        desktop_environment_linux.cc
        desktop_environment_linux.h
        desktop_resizer_x11.cc
        desktop_resizer_x11.h
        screen_capturer_kms.cc
        screen_capturer_kms.h
        screen_capturer_kwin.cc
        screen_capturer_kwin.h
        screen_capturer_pipewire.cc
        screen_capturer_pipewire.h
        screen_capturer_wlr.cc
        screen_capturer_wlr.h
        screen_capturer_x11.cc
        screen_capturer_x11.h)
endif()

if (APPLE)
    collect_sources(SOURCE_BASE_DESKTOP
        desktop_environment_mac.mm
        desktop_environment_mac.h
        screen_capturer_mac.mm
        screen_capturer_mac.h)
endif()

collect_sources(SOURCE_BASE_DESKTOP_TESTS
    diff_block_32bpp_c_unittest.cc
    diff_block_32bpp_sse2_unittest.cc
    frame_unittest.cc
    region_unittest.cc)
