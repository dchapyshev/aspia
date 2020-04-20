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

list(APPEND SOURCE_DESKTOP
    capture_scheduler.cc
    capture_scheduler.h
    cursor_capturer.h
    cursor_capturer_win.cc
    cursor_capturer_win.h
    diff_block_32bpp_avx2.cc
    diff_block_32bpp_avx2.h
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
    frame_dib.cc
    frame_dib.h
    frame_rotation.cc
    frame_rotation.h
    frame_simple.cc
    frame_simple.h
    geometry.cc
    geometry.h
    mouse_cursor.cc
    mouse_cursor.h
    pixel_format.cc
    pixel_format.h
    region.cc
    region.h
    screen_capturer.cc
    screen_capturer.h
    screen_capturer_dxgi.cc
    screen_capturer_dxgi.h
    screen_capturer_gdi.cc
    screen_capturer_gdi.h
    screen_capturer_wrapper.cc
    screen_capturer_wrapper.h
    shared_frame.cc
    shared_frame.h
    shared_memory_frame.cc
    shared_memory_frame.h)

list(APPEND SOURCE_DESKTOP_UNIT_TESTS
    diff_block_32bpp_avx2_unittest.cc
    diff_block_32bpp_c_unittest.cc
    diff_block_32bpp_sse2_unittest.cc
    geometry_unittest.cc
    region_unittest.cc)

list(APPEND SOURCE_DESKTOP_WIN
    win/bitmap_info.h
    win/cursor.cc
    win/cursor.h
    win/d3d_device.cc
    win/d3d_device.h
    win/desktop_environment.cc
    win/desktop_environment.h
    win/display_configuration_monitor.cc
    win/display_configuration_monitor.h
    win/dxgi_adapter_duplicator.cc
    win/dxgi_adapter_duplicator.h
    win/dxgi_context.cc
    win/dxgi_context.h
    win/dxgi_duplicator_controller.cc
    win/dxgi_duplicator_controller.h
    win/dxgi_frame.cc
    win/dxgi_frame.h
    win/dxgi_output_duplicator.cc
    win/dxgi_output_duplicator.h
    win/dxgi_texture.cc
    win/dxgi_texture.h
    win/dxgi_texture_mapping.cc
    win/dxgi_texture_mapping.h
    win/dxgi_texture_staging.cc
    win/dxgi_texture_staging.h
    win/power_save_blocker.cc
    win/power_save_blocker.h
    win/screen_capture_utils.cc
    win/screen_capture_utils.h)

list(APPEND SOURCE_DESKTOP_WIN_UNIT_TESTS
    win/cursor_unittest.cc
    win/cursor_unittest_resources.h
    win/cursor_unittest_resources.rc)

source_group("" FILES ${SOURCE_DESKTOP})
source_group("" FILES ${SOURCE_DESKTOP_UNIT_TESTS})
source_group(win FILES ${SOURCE_DESKTOP_WIN})
source_group(win FILES ${SOURCE_DESKTOP_WIN_UNIT_TESTS})

add_library(aspia_desktop STATIC ${SOURCE_DESKTOP} ${SOURCE_DESKTOP_WIN})
target_link_libraries(aspia_desktop aspia_base ${THIRD_PARTY_LIBS})

# If the build of unit tests is enabled.
if (BUILD_UNIT_TESTS)
    add_executable(aspia_desktop_tests
        ${SOURCE_DESKTOP_UNIT_TESTS}
        ${SOURCE_DESKTOP_WIN_UNIT_TESTS})
    target_link_libraries(aspia_desktop_tests
        aspia_base
        aspia_desktop
        optimized gtest
        optimized gtest_main
        debug gtestd
        debug gtest_maind
        ${THIRD_PARTY_LIBS})

    add_test(NAME aspia_desktop_tests COMMAND aspia_desktop_tests)
endif()

