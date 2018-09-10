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
    ${PROJECT_SOURCE_DIR}/desktop_capture/capture_scheduler.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/capture_scheduler.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/cursor_capturer.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/cursor_capturer_win.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/cursor_capturer_win.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/desktop_frame.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/desktop_frame.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/desktop_frame_aligned.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/desktop_frame_aligned.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/desktop_frame_dib.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/desktop_frame_dib.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/desktop_frame_qimage.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/desktop_frame_qimage.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/desktop_geometry.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/desktop_geometry.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/desktop_region.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/desktop_region.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/diff_block_avx2.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/diff_block_avx2.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/diff_block_c.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/diff_block_c.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/diff_block_sse2.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/diff_block_sse2.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/diff_block_sse3.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/diff_block_sse3.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/differ.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/differ.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/mouse_cursor.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/mouse_cursor.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/mouse_cursor_cache.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/mouse_cursor_cache.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/pixel_format.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/pixel_format.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/screen_capture_frame_queue.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/screen_capturer.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/screen_capturer_gdi.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/screen_capturer_gdi.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/screen_settings_tracker.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/screen_settings_tracker.h)

list(APPEND SOURCE_DESKTOP_CAPTURE_UNIT_TESTS
    ${PROJECT_SOURCE_DIR}/desktop_capture/desktop_geometry_unittest.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/desktop_region_unittest.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/diff_block_avx2_unittest.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/diff_block_c_unittest.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/diff_block_sse2_unittest.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/diff_block_sse3_unittest.cc)

list(APPEND SOURCE_DESKTOP_CAPTURE_WIN
    ${PROJECT_SOURCE_DIR}/desktop_capture/win/cursor.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/win/cursor.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/win/desktop.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/win/desktop.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/win/scoped_thread_desktop.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/win/scoped_thread_desktop.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/win/screen_capture_utils.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/win/screen_capture_utils.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/win/visual_effects_disabler.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/win/visual_effects_disabler.h)

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

