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

list(APPEND SOURCE_DESKTOP_CAPTURE
    capture_scheduler.cc
    capture_scheduler.h
    cursor_capturer.h
    cursor_capturer_win.cc
    cursor_capturer_win.h
    desktop_frame.cc
    desktop_frame.h
    desktop_frame_aligned.cc
    desktop_frame_aligned.h
    desktop_frame_dib.cc
    desktop_frame_dib.h
    desktop_frame_qimage.cc
    desktop_frame_qimage.h
    desktop_frame_simple.cc
    desktop_frame_simple.h
    desktop_geometry.cc
    desktop_geometry.h
    desktop_region.cc
    desktop_region.h
    diff_block_avx2.cc
    diff_block_avx2.h
    diff_block_c.cc
    diff_block_c.h
    diff_block_sse2.cc
    diff_block_sse2.h
    diff_block_sse3.cc
    diff_block_sse3.h
    differ.cc
    differ.h
    mouse_cursor.cc
    mouse_cursor.h
    mouse_cursor_cache.cc
    mouse_cursor_cache.h
    pixel_format.cc
    pixel_format.h
    screen_capture_frame_queue.h
    screen_capturer.h
    screen_capturer_gdi.cc
    screen_capturer_gdi.h
    screen_settings_tracker.cc
    screen_settings_tracker.h)

list(APPEND SOURCE_DESKTOP_CAPTURE_UNIT_TESTS
    desktop_geometry_unittest.cc
    desktop_region_unittest.cc
    diff_block_avx2_unittest.cc
    diff_block_c_unittest.cc
    diff_block_sse2_unittest.cc
    diff_block_sse3_unittest.cc)

list(APPEND SOURCE_DESKTOP_CAPTURE_WIN
    win/cursor.cc
    win/cursor.h
    win/desktop.cc
    win/desktop.h
    win/scoped_thread_desktop.cc
    win/scoped_thread_desktop.h
    win/screen_capture_utils.cc
    win/screen_capture_utils.h
    win/visual_effects_disabler.cc
    win/visual_effects_disabler.h)

source_group("" FILES ${SOURCE_DESKTOP_CAPTURE})
source_group("" FILES ${SOURCE_DESKTOP_CAPTURE_UNIT_TESTS})
source_group(win FILES ${SOURCE_DESKTOP_CAPTURE_WIN})

add_library(aspia_desktop_capture STATIC
    ${SOURCE_DESKTOP_CAPTURE}
    ${SOURCE_DESKTOP_CAPTURE_WIN})
target_link_libraries(aspia_desktop_capture
    aspia_base
    ${THIRD_PARTY_LIBS})

# If the build of unit tests is enabled.
if (BUILD_UNIT_TESTS)
    add_executable(aspia_desktop_capture_tests ${SOURCE_DESKTOP_CAPTURE_UNIT_TESTS})
    target_link_libraries(aspia_desktop_capture_tests
        aspia_base
        aspia_desktop_capture
        optimized gtest
        optimized gtest_main
        debug gtestd
        debug gtest_maind
        ${THIRD_PARTY_LIBS})

    add_test(NAME aspia_desktop_capture_tests COMMAND aspia_desktop_capture_tests)
endif()

