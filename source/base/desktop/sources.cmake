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
    region.cc
    region.h
    shared_frame.cc
    shared_frame.h)

if (WIN32)
    collect_sources(SOURCE_BASE_DESKTOP
        frame_dib.cc
        frame_dib.h)
endif()

collect_sources(SOURCE_BASE_DESKTOP_TESTS
    diff_block_32bpp_c_unittest.cc
    diff_block_32bpp_sse2_unittest.cc
    frame_rotation_unittest.cc
    frame_unittest.cc
    region_unittest.cc)
